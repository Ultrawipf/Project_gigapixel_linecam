#include "cameracontrol.h"
#include "constants_pi.h"
#include <iostream>
#include <vector>
// #include <tuple>
// #include <memory>
#include <chrono>
#include <imagehelpers.hpp>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <condition_variable>

static int64_t steadyNs(){
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

#include <spng.h>

TMC2209_motor* Motors::slideMotor_p;
//TMC2209_motor Motors::slideMotor(MOTLIN_STEPPIN,MOTLIN_DIRPIN,MOTLIN_ENPIN,MOTLIN_DIAGPIN,MOTOR_SERIAL);
Motors::Motors()
: slideMotor(MOTLIN_STEPPIN,MOTLIN_DIRPIN,MOTORLIN_MMTOSTEPS,MOTLIN_ENPIN,MOTLIN_DIAGPIN,MOTOR_SERIAL)
{
    //TMC2209_motor Motors::slideMotor(MOTLIN_STEPPIN,MOTLIN_DIRPIN,MOTLIN_ENPIN,MOTLIN_DIAGPIN,MOTOR_SERIAL);
    //slideMotor.verbose = true;
#ifdef SLIDE_STALLGUARD
    slideMotor.homeSG = SLIDE_STALLGUARD;
#endif
    slideMotor.init();
    slideMotor.invdir = false;
    //slideMotor.updateConfigFromChip();
    slideMotor_p = &slideMotor;
    slideMotor.setDoubleEdge(true);
    slideMotor.setMultistepFilt(false);
    slideMotor.setInterpolation(false); // disable interpolation for more reliable movement
}

Motors::~Motors(){
    slideMotor.setEnable(false);
    slideMotor_p = nullptr;
    //std::cout << "Deleting Motors "<< std::endl;
}

// Static functions
void Motors::homeSlide(){
    if(!slideMotor_p) return;
    slideMotor_p->setEnable(true);
    slideMotor_p->setMicrostep(TMC2209_motor::microstep::ms8);
    slideMotor_p->move(1000,5000,10);
    slideMotor_p->home(-50000);
    slideMotor_p->setEnable(false);
}

PinCtrl::PinCtrl(){
    wiringPiSetupGpio();
    pinMode(RESETPIN, OUTPUT);
}
PinCtrl::~PinCtrl(){
    digitalWrite(RESETPIN, 0);
}
void PinCtrl::setEnablePin(bool active){
    digitalWrite(RESETPIN, active);
}


LinearCamCtrl::LinearCamCtrl(Motors& motors,CCD_Ctrl& ccd)
    : motors(motors),ccd(ccd)
{
    /*
    bool inited = ccd.initCCD();
    std::cout << "Init?: "<< inited << std::endl;
    std::cout << "State: "<< ccd.readReg(0x61) << std::endl;
    */
    motors.slideMotor_p->setMicrostep(TMC2209_motor::microstep::ms128);

}

LinearCamCtrl::~LinearCamCtrl(){
    ccd.setClock(false);
}

void LinearCamCtrl::startCapture(bool nomovemode){
    std::cout << "Capture start "<< std::endl;
    if(!nomovemode){
        motors.slideMotor_p->setEnable(true);
        motors.slideMotor_p->setMicrostep(TMC2209_motor::microstep::ms128); // Use with dedge
        this->imageCorrection = std::make_unique<CCD_ImageCorrection>(ccd.getCurParams().samplesPerLine,ccd.getCurParams().lineBuffers);
    }else{
        this->imageCorrection = std::make_unique<CCD_ImageCorrection>(ccd.getCurParams().samplesPerLine,2);
    }
    lineTimings.clear();
    lastLineStart.reset();
    lineIndex = 0;
    calibStarts.clear();
    exposureStats = ExposureStats{};
    exposureStats.requestedUs = linePeriodUs;
    ccd.startCapture();
    // clear ccd
    // Clear last buffer

}

void LinearCamCtrl::endCapture(){
    ccd.endCapture();
    std::cout << "Capture stop "<< std::endl;
    if(processThread.joinable()){
        processThread.join();
    }
    motors.slideMotor_p->setEnable(false);
}

// Can be called from a different thread if a capture process is running. Might break the saved file!
void LinearCamCtrl::abortImageCapture(){
    captureInProgress = false;
}

// Writes lines on its own thread so a slow encode or file write never delays the next line start
// (which would lengthen that line's exposure). Lines are written in order. push() only blocks when
// maxLines are waiting, finish() writes the rest and stops the thread
class LineWriter{
public:
    LineWriter(std::function<void(const std::vector<ccdpixel>&)> write,size_t maxLines = 64)
        : write(write),maxLines(maxLines),worker([this](){run();}){}
    ~LineWriter(){
        finish();
    }
    void push(std::vector<ccdpixel>&& line){
        std::unique_lock<std::mutex> lock(mutex);
        notFull.wait(lock,[this](){return queue.size() < maxLines;});
        queue.push_back(std::move(line));
        notEmpty.notify_one();
    }
    void finish(){
        {
            std::lock_guard<std::mutex> lock(mutex);
            done = true;
        }
        notEmpty.notify_one();
        if(worker.joinable()){
            worker.join();
        }
    }
private:
    void run(){
        while(true){
            std::vector<ccdpixel> line;
            {
                std::unique_lock<std::mutex> lock(mutex);
                notEmpty.wait(lock,[this](){return done || !queue.empty();});
                if(queue.empty()){
                    return; // done and drained
                }
                line = std::move(queue.front());
                queue.pop_front();
                notFull.notify_one();
            }
            write(line);
        }
    }
    std::function<void(const std::vector<ccdpixel>&)> write;
    size_t maxLines;
    std::mutex mutex;
    std::condition_variable notEmpty,notFull;
    std::deque<std::vector<ccdpixel>> queue;
    bool done = false;
    std::thread worker; // Last member, starts after the others are initialized
};

// Saves image into a file and optionally returns last line as callback argument
void LinearCamCtrl::captureImage(std::string filename,uint32_t lines,int usDelay,std::optional<std::function<void(const std::vector<ccdpixel>&)>> lineCallback,bool nomovemode){
    CCD_Parameters params = ccd.getCurParams();
    // Debug aid: LINESCAN_TIMING_CSV=<file> writes the per line timestamps of every capture
    const char* timingCsv = std::getenv("LINESCAN_TIMING_CSV");
    const bool recordTimingBefore = recordTiming;
    if(timingCsv){
        recordTiming = true;
    }

    //float curPos = motors.slideMotor_p->getActualPos();
    uint32_t linewidth = params.validPixelEnd - params.validPixelBegin;
    std::cout << "Expected image size:" << linewidth << "x" << lines << std::endl;

    std::string ext = filename.substr(filename.find_last_of(".") + 1);

    if(ext == "png"){

        std::cout << "Save as PNG" << std::endl;
        int bonuslines = params.lineBuffers+startupSkipLines;
        /* Creating an encoder context requires a flag */
        FILE* pngfile = fopen(filename.c_str(),"wb");
        if(!pngfile){
            throw std::runtime_error("Cannot open image file " + filename);
        }
        spng_ctx* ctx = spng_ctx_new(SPNG_CTX_ENCODER);
        spng_set_png_file(ctx, pngfile);

        png::pngSetDefaultOptionsProgressive(ctx,linewidth,lines,16,pnggamma,pngCompressionLevel);

        LineWriter writer([ctx](const std::vector<ccdpixel>& l){
            spng_encode_row(ctx, l.data(), l.size() * sizeof(ccdpixel));
        });
        startCapture(nomovemode);
        captureInProgress = true;
        for(int line = 0;line < lines+bonuslines && captureInProgress;line++){
            std::vector<ccdpixel> linedata = captureLine(usDelay,nomovemode);
            if(line < bonuslines){
                continue; // Ignore first lines
            }
            if(lineCallback.has_value()){
                (lineCallback.value())(linedata);
            }
            writer.push(std::move(linedata));
        }
        endCapture();
        writer.finish(); // Encode the queued lines before the context is freed

        spng_ctx_free(ctx);
        fclose(pngfile);
        captureInProgress = false;
    }else if(ext == "bmp"){
        // BMP
        // Bitmap?
        std::ofstream file(filename,std::ios::binary);
        std::ostream_iterator<ccdpixel> file_it(file);
        bmp::BitmapHeader bmphdr(params.samplesPerLine/3,lines);
        bmphdr.writeHeader(file);

        if(bmphdr.header.size_image == 0){
            std::cout << "WARNING: Bitmap header size set to 0 due to large size" << std::endl;
        }

        startCapture(nomovemode);
        captureInProgress = true;
        for(int line = 0;line < lines && captureInProgress;line++){
            std::vector<ccdpixel> linedata = captureLine(usDelay,nomovemode);
            if(lineCallback){
                (*lineCallback)(linedata);
            }

            uint8_t bmpoffset = 2;
            if(sizeof(ccdpixel) == 2){
                std::vector<uint8_t> bmpline;
                bmpline.resize(linedata.size() ,0); // 8b, reserve
                // Start at bmpoffset+1 so the reversed index stays inside the line
                for(size_t i = bmpoffset+1;i<linedata.size()-bmpoffset;i++){
                    bmpline[linedata.size()-i+bmpoffset] = (linedata[i] >> 8);
                }

                file.write((char *) bmpline.data(),bmpline.size());
                //std::copy(bmpline.rbegin(),bmpline.rend(),file_it); // Write line in reverse for BMP
            }else{
                std::copy(linedata.rbegin(),linedata.rend(),file_it); // Write line in reverse for BMP
            }
        }
        endCapture();
        file.close();
        captureInProgress = false;
    }else{
        // Raw
        std::cout << "Save raw" << std::endl;
        FILE* file = fopen(filename.c_str(),"wb");
        if(!file){
            throw std::runtime_error("Cannot open image file " + filename);
        }
        LineWriter writer([file](const std::vector<ccdpixel>& l){
            fwrite(l.data(), l.size(), sizeof(ccdpixel), file);
        });
        startCapture(nomovemode);
        captureInProgress = true;
        int bonuslines = params.lineBuffers+startupSkipLines;
        for(int line = 0;line < lines + bonuslines && captureInProgress;line++){
            std::vector<ccdpixel> linedata = captureLine(usDelay,nomovemode);
            if(line < bonuslines){
                continue; // Ignore first lines
            }
            if(lineCallback.has_value()){
                (lineCallback.value())(linedata);
            }
            writer.push(std::move(linedata));
        }

        endCapture();
        writer.finish(); // Write the queued lines before the file is closed
        fclose(file);
        captureInProgress = false;
    }

    if(timingCsv){
        std::ofstream csv(timingCsv);
        csv << "line,readStart_ns,readEnd_ns,moveStart_ns,moveEnd_ns" << std::endl;
        for(size_t i = 0;i<lineTimings.size();i++){
            csv << i << ',' << lineTimings[i].readStart << ',' << lineTimings[i].readEnd << ',' << lineTimings[i].moveStart << ',' << lineTimings[i].moveEnd << std::endl;
        }
        recordTiming = recordTimingBefore;
    }
    //motors.slideMotor_p->moveToMM(curPos,moveSpeed,moveAccel);
}

// Returns last valid line
std::vector<ccdpixel> LinearCamCtrl::captureLine(int usDelay,bool nomovemode){
    CCD_Parameters params = ccd.getCurParams();
    uint32_t linelength = params.samplesPerLine; // Data to buffer.
    uint32_t tgperiod = linelength / SAMPLESPERBUFFER;
    //uint32_t buf[tgperiod]; // Number of samples per line
    uint32_t* buf = (uint32_t*)std::malloc(tgperiod * sizeof(uint32_t));
    //std::cout << "Startcapture "<< linelength << " TG " << tgperiod << std::endl;
    LineTiming timing;
    processThread = std::thread([&](){
        // Fixed line period: start each readout (TG) at an absolute time instead of sleeping after the processing
        const bool fixed = fixedLinePeriod && lineIndex >= periodCalibLines && lastLineStart && exposureStats.periodUs > 0;
        if(fixed){
            const auto deadline = *lastLineStart + std::chrono::microseconds(exposureStats.periodUs);
            std::this_thread::sleep_until(deadline - std::chrono::microseconds(LINEPERIOD_SPIN_US));
            while(std::chrono::steady_clock::now() < deadline){} // Spin the last part, sleep wakeup is too coarse
            lastLineStart = std::chrono::steady_clock::now();
            const int lateUs = std::chrono::duration_cast<std::chrono::microseconds>(*lastLineStart - deadline).count();
            exposureStats.lines++;
            if(lateUs > LINEPERIOD_LATE_US){
                exposureStats.lateLines++;
            }
            exposureStats.maxLateUs = std::max(exposureStats.maxLateUs,lateUs);
        }else{
            lastLineStart = std::chrono::steady_clock::now();
            if(lineIndex < periodCalibLines){
                calibStarts.push_back(*lastLineStart);
            }
        }
        if(recordTiming) timing.readStart = steadyNs();
        ccd.captureData(buf,tgperiod,tgperiod); // capture one line of raw data
        if(recordTiming) timing.readEnd = steadyNs();
    });

    if(!nomovemode){
        if(recordTiming) timing.moveStart = steadyNs();
        moveLines(params.moveLines); // Move while capture? faster?
        if(recordTiming) timing.moveEnd = steadyNs();
        uint32_t stride = params.lineOffsets.size(); // Offset previous sample
        for(int i = 0;i<stride;i++){
            imageCorrection->offsetLine(i,stride,-params.lineOffsets[i]);
        }
    }

    processThread.join(); // Join capture
    if(recordTiming){
        lineTimings.push_back(timing);
    }
    lineIndex++;
    if(lineIndex == periodCalibLines){
        finishPeriodCalibration();
    }

    // Maybe add delay to increase exposure time...
    if(usDelay){
        std::this_thread::sleep_for(std::chrono::microseconds(usDelay));
    }

    std::vector<ccdpixel> line = imageCorrection->getLine(1); // Get last line in buffer;
    // for(ccdpixel pxl : line){
    //     std::cout << pxl << " ";
    // }

    int32_t nextline = imageCorrection->incrementLine(); // Shift current pointer
    //std::cout << "Nextline " << nextline << std::endl;
    imageCorrection->addLineBuf(buf,tgperiod); // Add to next line buffer
    std::free(buf);
    return std::vector<ccdpixel>(line.begin() + params.validPixelBegin*3,line.begin() + params.validPixelEnd*3);
}

// The calibration lines ran as fast as possible, so their start intervals are the real cost of one line.
// The first interval is skipped, it includes thread and cache warm up
void LinearCamCtrl::finishPeriodCalibration(){
    int64_t slowestUs = 0;
    for(size_t i = 2;i<calibStarts.size();i++){
        slowestUs = std::max<int64_t>(slowestUs,std::chrono::duration_cast<std::chrono::microseconds>(calibStarts[i] - calibStarts[i-1]).count());
    }
    exposureStats.minPeriodUs = slowestUs * LINEPERIOD_MARGIN + LINEPERIOD_EXTRA_US;
    exposureStats.periodUs = std::max(exposureStats.minPeriodUs,linePeriodUs);
    std::cout << "Line period " << exposureStats.periodUs << "us (requested " << linePeriodUs << "us, shortest " << exposureStats.minPeriodUs << "us)" << std::endl;
}

void LinearCamCtrl::moveLines(float lines){
    //std::cout << "Movelines " << lines << " dist " << ccd.lineDistMM << std::endl;
    motors.slideMotor.moveMM(ccd.lineDistMM*lines,moveSpeed,moveAccel);
}
