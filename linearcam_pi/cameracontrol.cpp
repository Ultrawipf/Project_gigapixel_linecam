#include "cameracontrol.h"
#include "constants_pi.h"
#include <iostream>
#include <vector>
#include <tuple>

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
    motors.slideMotor_p->setMicrostep(TMC2209_motor::microstep::ms16);
    CCD_Ctrl::startCapture();
    // clear ccd
    // Clear last buffer

}

std::vector<CCDPixel> LinearCamCtrl::captureLine(){
    moveLines(curParams.moveLines);
    uint32_t linelength = curParams.samplesPerLine; // Data to buffer.
    uint32_t tgperiod = linelength / 3;
    uint32_t buf[tgperiod];
    //std::cout << "Startcapture "<< linelength << " TG " << tgperiod << std::endl;
    captureData(buf,tgperiod,tgperiod); // capture one line
    std::cout << "Captured "<< std::endl;
    std::vector<CCDPixel> line = decodeData(buf,linelength);
    return line;
}

void LinearCamCtrl::moveLines(float lines){
    motors.slideMotor.moveMM(lineDistMM*lines,moveSpeed,moveAccel);
}
