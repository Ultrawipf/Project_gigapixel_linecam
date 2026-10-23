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
#include <vector>

#include <chrono>

#ifndef LINEPERIOD_SPIN_US
#define LINEPERIOD_SPIN_US 200 // Busy wait before a fixed line start
#endif

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


// Per line timestamps for exposure measurements. Steady clock, ns
struct LineTiming{
    int64_t readStart = 0; // Right before the data SM restart. The SM fires TG a few us later
    int64_t readEnd = 0;   // Line read from the PIO
    int64_t moveStart = 0;
    int64_t moveEnd = 0;
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

    // Exposure = time between two TG pulses. Set recordTiming to log every captured line into lineTimings
    bool recordTiming = false;
    std::vector<LineTiming> lineTimings;

    // > 0: start every line linePeriodUs after the previous one. Exposure then no longer depends on the processing time.
    // Must be longer than readout + processing of one line, otherwise lines start late
    int linePeriodUs = 0;

    std::unique_ptr<CCD_ImageCorrection> imageCorrection;
    CCD_Ctrl& ccd;
    Motors& motors;
protected:
    void runOffsetLines();
    //CCD_Ctrl ccd;
    std::thread processThread;
    std::optional<std::chrono::steady_clock::time_point> lastLineStart;
    std::atomic<bool> captureInProgress = false; // Cleared by abortImageCapture() from other threads
};

#endif // CAMERACONTROL_H
