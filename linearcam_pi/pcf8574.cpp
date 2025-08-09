#include "pcf8574.h"
#include "iostream"
#include "constants_pi.h"

#ifdef PCF8574DEF

PCF8574* PCF8574::instance = nullptr;

PCF8574::PCF8574(int isrpin,int address) {
    instance = this;
    wiringPiSetup();
    dev = wiringPiI2CSetupInterface(I2C_DEV,address);
    wiringPiISR(isrpin, INT_EDGE_FALLING, &interruptCbStatic);
    // Set pins as input
    writeReg(0xff);
}

PCF8574::~PCF8574(){
    instance = nullptr;
}

void PCF8574::interruptCbStatic(){
    if(!instance){
        return;
    }
    instance->interruptCb();
}

void PCF8574::registerCallbackHandler(PCFCallbackHandler* cls){
    this->callbackcls = cls;
}

void PCF8574::writeReg(uint8_t data){
    wiringPiI2CWrite(dev,data);
}

uint8_t PCF8574::readReg(){
    return wiringPiI2CRead(dev) & 0xff;
}

void PCF8574::interruptCb(){
    // std::cout << "PCF ISR" << callbackcls << std::endl;
    if(!callbackcls){
        return;
    }
    callbackcls->pcfCallback();

}

PCFCallbackHandler::PCFCallbackHandler(){

}
void PCFCallbackHandler::pcfCallback(){

}

#endif
