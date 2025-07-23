#include "cameracontrol.h"
#include "constants_pi.h"
#include <iostream>
#include <vector>
#include <tuple>
#include <memory>

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
    if(processThread.joinable()){
        processThread.join();
    }
}

// Returns last valid line
std::vector<ccdpixel> LinearCamCtrl::captureLine(){
    // moveLines(curParams.moveLines); // Move
    uint32_t linelength = curParams.samplesPerLine; // Data to buffer.
    uint32_t tgperiod = linelength / 3;
    uint32_t buf[tgperiod];
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

    std::vector<ccdpixel> line = imageCorrection->getLine(1); // Get last line in buffer;

    int32_t nextline = imageCorrection->incrementLine(); // Shift current pointer
    //std::cout << "Nextline " << nextline << std::endl;
    imageCorrection->addLineBuf(buf,tgperiod); // Add to next line buffer
    return line;
}

void LinearCamCtrl::moveLines(float lines){
    motors.slideMotor.moveMM(lineDistMM*lines,moveSpeed,moveAccel);
}
