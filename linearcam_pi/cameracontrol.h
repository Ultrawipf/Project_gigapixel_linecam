#ifndef CAMERACONTROL_H
#define CAMERACONTROL_H
#include "tmc2209_motor.h"

#ifndef DUMMY
#include <wiringPi.h>
#else
#include "wiringpi_dummy/wiringPi.h"
#endif

class Motors
{
public:
    Motors();
    virtual ~Motors();
    TMC2209_motor slideMotor;
    static TMC2209_motor* slideMotor_p;
    static void homeSlide();
};

class PinCtrl{
public:
    PinCtrl();
    ~PinCtrl();

    void setEnablePin(bool active);
};

#endif // CAMERACONTROL_H
