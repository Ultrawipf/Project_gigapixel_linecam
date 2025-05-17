#include "motors.h"
#include "constants_pi.h"


Motors::Motors()
: slideMotor(MOTLIN_STEPPIN,MOTLIN_DIRPIN,MOTLIN_ENPIN,MOTLIN_DIAGPIN,MOTOR_SERIAL)
{
    slideMotor.invdir = true;
    slideMotor.updateConfigFromChip();
}

void Motors::homeSlide(){
    slideMotor.setEnable(true);
    slideMotor.setMicrostep(TMC2209_motor::microstep::ms8);
    slideMotor.move(1000,5000,10);
    slideMotor.home(-40000);
    slideMotor.setEnable(false);
}
