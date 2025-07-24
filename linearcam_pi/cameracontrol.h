#ifndef CAMERACONTROL_H
#define CAMERACONTROL_H
#include "tmc2209_motor.h"
#include "ccd_ctrl.h"
#ifndef DUMMY
#include <wiringPi.h>
#else
#include "wiringpi_dummy/wiringPi.h"
#endif
#include <memory>

#include <thread>
#include <functional>

#ifndef PNG_COMPRESSION_LEVEL
#define PNG_COMPRESSION_LEVEL 0
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
    void endCapture();
    //std::vector<std::tuple<uint16_t,uint16_t,uint16_t>> captureLine(); // Should return last valid line or use a callback
    std::vector<ccdpixel> captureLine(int usDelay = 0); // Should return last valid line or use a callback
    void moveLines(float lines);

    void captureImage(std::string filename,uint32_t lines,int usDelay=0,bool png=false,std::function<void(const std::vector<ccdpixel>&)> *lineCallback = nullptr);

    //uint32_t setResolution(CCD_Resolution res);

    const float imageStartPosMM = 25; // Begin of valid image area
    const float previewCamCenterMM = 25; // Position when preview camera is centered in lens
    const float imageEndPosMM = 85; // End of valid image area

    std::unique_ptr<CCD_ImageCorrection> imageCorrection;

protected:
    void runOffsetLines();
    //CCD_Ctrl ccd;
    Motors& motors;
    std::thread processThread;
};

#endif // CAMERACONTROL_H
