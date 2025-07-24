#include "cameracontrol.h"
#include "constants_pi.h"
#include <iostream>
#include <vector>
// #include <tuple>
// #include <memory>
#include <chrono>
#include <bitmap.hpp>
#include <fstream>
#include <iterator>

#include <spng.h>

TMC2209_motor* Motors::slideMotor_p;
//TMC2209_motor Motors::slideMotor(MOTLIN_STEPPIN,MOTLIN_DIRPIN,MOTLIN_ENPIN,MOTLIN_DIAGPIN,MOTOR_SERIAL);
Motors::Motors()
: slideMotor(MOTLIN_STEPPIN,MOTLIN_DIRPIN,MOTORLIN_MMTOSTEPS,MOTLIN_ENPIN,MOTLIN_DIAGPIN,MOTOR_SERIAL)
{
    //TMC2209_motor Motors::slideMotor(MOTLIN_STEPPIN,MOTLIN_DIRPIN,MOTLIN_ENPIN,MOTLIN_DIAGPIN,MOTOR_SERIAL);
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
    slideMotor_p->home(-40000);
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


LinearCamCtrl::LinearCamCtrl(Motors& motors)
    : CCD_Ctrl(), motors(motors)
{
    /*
    bool inited = CCD_Ctrl::initCCD();
    std::cout << "Init?: "<< inited << std::endl;
    std::cout << "State: "<< CCD_Ctrl::readReg(0x61) << std::endl;
    */
    motors.slideMotor_p->setMicrostep(TMC2209_motor::microstep::ms8);

}

LinearCamCtrl::~LinearCamCtrl(){
    CCD_Ctrl::setClock(false);
}

void LinearCamCtrl::startCapture(){
    std::cout << "Capture start "<< std::endl;
    motors.slideMotor_p->setMicrostep(TMC2209_motor::microstep::ms32);
    this->imageCorrection = std::make_unique<CCD_ImageCorrection>(curParams.samplesPerLine,curParams.lineBuffers);

    CCD_Ctrl::startCapture();
    // clear ccd
    // Clear last buffer

}

void LinearCamCtrl::endCapture(){
    CCD_Ctrl::endCapture();
    std::cout << "Capture stop "<< std::endl;
    if(processThread.joinable()){
        processThread.join();
    }
}

// Saves image into a file and optionally returns last line as callback argument
void LinearCamCtrl::captureImage(std::string filename,uint32_t lines,int usDelay,bool png,std::function<void(const std::vector<ccdpixel>&)> *lineCallback){
    CCD_Parameters params = curParams;

    float curPos = motors.slideMotor_p->getActualPos();
    std::cout << "Expected image size:" << params.samplesPerLine/3 << "x" << lines << std::endl;

    if(png){

        std::cout << "Save as PNG" << std::endl;
        struct spng_ihdr ihdr = {0}; /* zero-initialize to set valid defaults */
        /* Creating an encoder context requires a flag */
        FILE* file = fopen(filename.c_str(),"wb");
        spng_ctx* ctx = spng_ctx_new(SPNG_CTX_ENCODER);
        spng_set_png_file(ctx, file);

        ihdr.width = params.samplesPerLine/3;
        ihdr.height = lines-params.lineBuffers;
        ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR; // Truecolor 16b
        ihdr.bit_depth = 16;
        uint64_t imgsize = ihdr.width * ihdr.height;
        spng_set_ihdr(ctx, &ihdr);
        spng_set_option(ctx,SPNG_IMG_COMPRESSION_LEVEL,PNG_COMPRESSION_LEVEL);
        spng_set_option(ctx,SPNG_IMG_MEM_LEVEL,9); // Max memory
        spng_phys phys = {52638,52638,1};
        spng_set_phys(ctx,&phys);
        spng_set_srgb(ctx,0);
        spng_set_gama(ctx,0.4);
        spng_encode_chunks(ctx);
        spng_encode_image(ctx, 0, imgsize, SPNG_FMT_PNG, SPNG_ENCODE_PROGRESSIVE | SPNG_ENCODE_FINALIZE); // Will finalize on last line

        std::thread pngthread;
        startCapture();

        for(int line = 0;line < lines;line++){
            std::vector<ccdpixel> linedata = captureLine(usDelay);
            if(line < params.lineBuffers){
                continue; // Ignore first lines
            }
            if(lineCallback){
                (*lineCallback)(linedata);
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

        fclose(file);
    }else{
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

        for(int line = 0;line < lines;line++){
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

                file.write((char *) bmpline.data(),bmpline.size());
                //std::copy(bmpline.rbegin(),bmpline.rend(),file_it); // Write line in reverse for BMP
            }else{
                std::copy(linedata.rbegin(),linedata.rend(),file_it); // Write line in reverse for BMP
            }
        }
        file.close();
    }

    motors.slideMotor_p->moveToMM(curPos,moveSpeed,moveAccel);
}

// Returns last valid line
std::vector<ccdpixel> LinearCamCtrl::captureLine(int usDelay){
    // moveLines(curParams.moveLines); // Move
    uint32_t linelength = curParams.samplesPerLine; // Data to buffer.
    uint32_t tgperiod = linelength / SAMPLESPERBUFFER;
    uint32_t buf[tgperiod]; // Number of samples per line
    //std::cout << "Startcapture "<< linelength << " TG " << tgperiod << std::endl;
    processThread = std::thread([&](){
        captureData(buf,tgperiod,tgperiod); // capture one line of raw data
    });
    moveLines(curParams.moveLines); // Move while capture? faster?

    uint32_t stride = curParams.lineOffsets.size(); // Offset previous sample
    for(int i = 0;i<stride;i++){
        imageCorrection->offsetLine(i,stride,-curParams.lineOffsets[i]);
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
    return line;
}

void LinearCamCtrl::moveLines(float lines){
    motors.slideMotor.moveMM(lineDistMM*lines,moveSpeed,moveAccel);
}
