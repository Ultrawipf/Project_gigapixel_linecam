#ifndef MOTORS_H
#define MOTORS_H
#include "tmc2209_motor.h"
class Motors
{
public:
    Motors();

    TMC2209_motor slideMotor;
    void homeSlide();
};

#endif // MOTORS_H
