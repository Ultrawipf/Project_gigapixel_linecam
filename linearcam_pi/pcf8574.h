#ifndef PCF8574_H
#define PCF8574_H

#include <string>
#define PCF8574DEF
#ifndef DUMMY
#define PCF8574DEF
#include <wiringPi.h>
#include <wiringPiI2C.h>
#else
#include "wiringpi_dummy/wiringPi.h"
//#include "wiringpi_dummy/wiringSerial.h"
#endif

class PCFCallbackHandler{
public:
    PCFCallbackHandler();
    virtual void pcfCallback();
};

class PCF8574
{
public:
    PCF8574(int isrpin,int address = 0x20);
    ~PCF8574();

    void writeReg(uint8_t data);
    uint8_t readReg();

    static void interruptCbStatic();
    void interruptCb();
    void registerCallbackHandler(PCFCallbackHandler* cls = nullptr);

private:
    PCFCallbackHandler* callbackcls = nullptr;
    static PCF8574* instance;
    int dev;
    long lastTime = 0;
};



#endif // PCF8574_H
