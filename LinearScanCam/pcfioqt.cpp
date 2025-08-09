#include "pcfioqt.h"
#include "constants_pi.h"
#include "iostream"
#include <chrono>
#include <QDebug>
//PCF8574 pcfIoExpander(PCFISRPIN,0x20);

PcfIoQt::PcfIoQt()
    : pcf(std::make_unique<PCF8574>(PCFISRPIN,0x20))
{
    if(pcf){
        pcf->registerCallbackHandler(this);
        lastVal = pcf->readReg();
        lastTime = std::chrono::high_resolution_clock::now(); // Init
    }
}
PcfIoQt::~PcfIoQt(){
    pcf->registerCallbackHandler(nullptr);
}

void PcfIoQt::setMask(uint8_t mask){
    signalMask = mask;
}

void PcfIoQt::pcfCallback(){
    auto time = (std::chrono::high_resolution_clock::now());
    uint8_t data = pcf->readReg();

    uint8_t changed = data ^ lastVal;
    lastVal = data;
    if( std::chrono::duration_cast<std::chrono::milliseconds>(time - lastTime)  > debounceMs){
        lastTime = time;


        if(changed & signalMask & ~data){
            emit buttonPressed(~data & signalMask & changed);
        }else if (changed & signalMask & data){
            emit buttonReleased(data & signalMask & changed);
        }

    }
}
