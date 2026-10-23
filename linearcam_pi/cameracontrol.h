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
#include <optional>
#include <atomic>

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


class LinearCamCtrl
{
public:
    LinearCamCtrl(Motors& motors,CCD_Ctrl& ccd);
    ~LinearCamCtrl();
    void startCapture(bool nomovemode = false);
    void endCapture();

    std::vector<ccdpixel> captureLine(int usDelay = 0,bool nomovemode = false); // Should return last valid line or use a callback
    void moveLines(float lines);
    void abortImageCapture();
    void captureImage(std::string filename,uint32_t lines,int usDelay=0,std::optional<std::function<void(const std::vector<ccdpixel>&)>> lineCallback = std::nullopt,bool nomovemode = false);
    void captureRawLines(std::string filename,uint32_t lines,int usDelay=0); // No moving, dump lines to raw file

    const float imageStartPosMM = 25; // Begin of valid image area
    const float previewCamCenterMM = 25; // Position when preview camera is centered in lens
    const float imageEndPosMM = 85; // End of valid image area

    static constexpr float moveAccel = 50;
    static constexpr float moveSpeed = 350;

    float pnggamma = 1.0;

    std::unique_ptr<CCD_ImageCorrection> imageCorrection;
    CCD_Ctrl& ccd;
    Motors& motors;
protected:
    void runOffsetLines();
    //CCD_Ctrl ccd;
    std::thread processThread;
    std::atomic<bool> captureInProgress = false; // Cleared by abortImageCapture() from other threads
};

#endif // CAMERACONTROL_H
