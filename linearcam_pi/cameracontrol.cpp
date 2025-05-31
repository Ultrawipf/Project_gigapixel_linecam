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
    slideMotor.invdir = true;
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
    bool inited = CCD_Ctrl::initCCD();
    std::cout << "Init?: "<< inited << std::endl;
    std::cout << "State: "<< CCD_Ctrl::readData(0x61) << std::endl;

}

LinearCamCtrl::~LinearCamCtrl(){
    CCD_Ctrl::setClock(false);
}

void LinearCamCtrl::startCapture(){
    std::cout << "Capture start "<< std::endl;
    CCD_Ctrl::startCapture();
    // clear ccd


}

std::vector<uint8_t> LinearCamCtrl::captureLine(){
    moveLines(1*4);
    uint32_t linelength = curLinelength / 4;
    uint32_t buf[linelength];
    std::cout << "Startcapture "<< linelength << std::endl;
    captureData(buf,linelength,linelength); // capture one line
    std::cout << "Captured "<< std::endl;
    std::vector<uint8_t> line = decodeData(buf,linelength);
    return line;
}

void LinearCamCtrl::moveLines(float lines){
    motors.slideMotor.moveMM(lineDistMM*lines,moveSpeed,moveAccel);
}
