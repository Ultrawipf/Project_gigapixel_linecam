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
    motors.slideMotor_p->setMicrostep(TMC2209_motor::microstep::ms8);

}

LinearCamCtrl::~LinearCamCtrl(){
    ccd.setClock(false);
}

void LinearCamCtrl::startCapture(){
    std::cout << "Capture start "<< std::endl;
    motors.slideMotor_p->setMicrostep(TMC2209_motor::microstep::ms64);
    this->imageCorrection = std::make_unique<CCD_ImageCorrection>(ccd.getCurParams().samplesPerLine,ccd.getCurParams().lineBuffers);

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
}

// Can be called from a different thread if a capture process is running. Might break the saved file!
void LinearCamCtrl::abortImageCapture(){
    captureInProgress = false;
}

// Saves image into a file and optionally returns last line as callback argument
void LinearCamCtrl::captureImage(std::string filename,uint32_t lines,int usDelay,std::optional<std::function<void(const std::vector<ccdpixel>&)>> lineCallback){
    CCD_Parameters params = ccd.getCurParams();

    //float curPos = motors.slideMotor_p->getActualPos();
    uint32_t linewidth = params.validPixelEnd - params.validPixelBegin;
    std::cout << "Expected image size:" << linewidth << "x" << lines << std::endl;

    std::string ext = filename.substr(filename.find_last_of(".") + 1);

    if(ext == "png"){

        std::cout << "Save as PNG" << std::endl;
        const int extraskip = 3;
        /* Creating an encoder context requires a flag */
        FILE* pngfile = fopen(filename.c_str(),"wb");
        spng_ctx* ctx = spng_ctx_new(SPNG_CTX_ENCODER);
        spng_set_png_file(ctx, pngfile);

        png::pngSetDefaultOptionsProgressive(ctx,linewidth,lines-params.lineBuffers-extraskip,16,pnggamma,PNG_COMPRESSION_LEVEL);

        std::thread pngthread;
        startCapture();
        captureInProgress = true;
        for(int line = 0;line < lines && captureInProgress;line++){
            std::vector<ccdpixel> linedata = captureLine(usDelay);
            if(line < params.lineBuffers+extraskip){
                continue; // Ignore first lines
            }
            if(lineCallback.has_value()){
                (lineCallback.value())(linedata);
            }
            //spng_encode_row(ctx, linedata.data(), linedata.size() * sizeof(ccdpixel));
            if(pngthread.joinable()){
                pngthread.join();
            }
            pngthread = std::thread([&ctx,&linedata](){
                spng_encode_row(ctx, linedata.data(), linedata.size() * sizeof(ccdpixel));
            });
        }
        endCapture();
        pngthread.join();

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

        startCapture();
        captureInProgress = true;
        std::thread thread;
        for(int line = 0;line < lines && captureInProgress;line++){
            std::vector<ccdpixel> linedata = captureLine(usDelay);
            if(lineCallback){
                (*lineCallback)(linedata);
            }

            uint8_t bmpoffset = 2;
            if(sizeof(ccdpixel) == 2){
                std::vector<uint8_t> bmpline;
                bmpline.resize(linedata.size() ,0); // 8b, reserve
                for(int i = 0;i<linedata.size()-bmpoffset;i++){
                    bmpline[linedata.size()-i+bmpoffset] = (linedata[i] >> 8);
                }

                // file.write((char *) bmpline.data(),bmpline.size());
                thread = std::thread([&](){
                    file.write((char *) bmpline.data(),bmpline.size());
                });
                if(thread.joinable()){
                    thread.join();
                }
                //std::copy(bmpline.rbegin(),bmpline.rend(),file_it); // Write line in reverse for BMP
            }else{
                std::copy(linedata.rbegin(),linedata.rend(),file_it); // Write line in reverse for BMP
            }
        }
        file.close();
        captureInProgress = false;
    }else{
        // Raw
        std::thread thread;
        std::cout << "Save raw" << std::endl;
        const int extraskip = 3;
        /* Creating an encoder context requires a flag */
        FILE* file = fopen(filename.c_str(),"wb");
        startCapture();
        captureInProgress = true;
        for(int line = 0;line < lines && captureInProgress;line++){
            std::vector<ccdpixel> linedata = captureLine(usDelay);
            if(line < params.lineBuffers+extraskip){
                continue; // Ignore first lines
            }
            if(lineCallback.has_value()){
                (lineCallback.value())(linedata);
            }
            // Save line
            thread = std::thread([&](){
                fwrite(linedata.data(), linedata.size(), sizeof(ccdpixel), file);
            });
            if(thread.joinable()){
                thread.join();
            }
            //fwrite(linedata.data(), linedata.size(), sizeof(ccdpixel), file);

        }

        endCapture();
        fclose(file);
        captureInProgress = false;
    }

    //motors.slideMotor_p->moveToMM(curPos,moveSpeed,moveAccel);
}

// Returns last valid line
std::vector<ccdpixel> LinearCamCtrl::captureLine(int usDelay){
    CCD_Parameters params = ccd.getCurParams();
    uint32_t linelength = params.samplesPerLine; // Data to buffer.
    uint32_t tgperiod = linelength / SAMPLESPERBUFFER;
    //uint32_t buf[tgperiod]; // Number of samples per line
    uint32_t* buf = (uint32_t*)std::malloc(tgperiod * sizeof(uint32_t));
    //std::cout << "Startcapture "<< linelength << " TG " << tgperiod << std::endl;
    processThread = std::thread([&](){
        ccd.captureData(buf,tgperiod,tgperiod); // capture one line of raw data
    });
    moveLines(params.moveLines); // Move while capture? faster?

    uint32_t stride = params.lineOffsets.size(); // Offset previous sample
    for(int i = 0;i<stride;i++){
        imageCorrection->offsetLine(i,stride,-params.lineOffsets[i]);
    }

    processThread.join(); // Join capture

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

void LinearCamCtrl::moveLines(float lines){
    //std::cout << "Movelines " << lines << " dist " << ccd.lineDistMM << std::endl;
    motors.slideMotor.moveMM(ccd.lineDistMM*lines,moveSpeed,moveAccel);
}
