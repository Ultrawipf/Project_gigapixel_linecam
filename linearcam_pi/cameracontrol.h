#ifndef CAMERACONTROL_H
#define CAMERACONTROL_H
#include "tmc2209_motor.h"
#include "ccd_ctrl.h"
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


class LinearCamCtrl : public CCD_Ctrl
{
public:
    LinearCamCtrl(Motors& motors);
    ~LinearCamCtrl();
    void startCapture();
    //void endCapture();
    //std::vector<std::tuple<uint16_t,uint16_t,uint16_t>> captureLine(); // Should return last valid line or use a callback
    std::vector<uint8_t> captureLine(); // Should return last valid line or use a callback
    void moveLines(float lines);

    //uint32_t setResolution(CCD_Resolution res);
    const float moveAccel = 10;
    const float moveSpeed = 500;
    const float lineDistMM = 0.004; // Pixel size
    const float mainLineDistMM = 0.048; // 12 lines
    const float subLineDistMM = 0.008; // 2 lines

protected:
    //CCD_Ctrl ccd;
    Motors& motors;
};

#endif // CAMERACONTROL_H
