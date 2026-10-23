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
#ifndef LINEPERIOD_MARGIN
#define LINEPERIOD_MARGIN 1.05f // Shortest period = slowest calibration line * margin + extra
#endif
#ifndef LINEPERIOD_EXTRA_US
#define LINEPERIOD_EXTRA_US 200
#endif
#ifndef CAPTURE_RT_PRIORITY
#define CAPTURE_RT_PRIORITY 50 // SCHED_FIFO priority of the capture threads, if permitted
#endif
#ifndef LINEPERIOD_LATE_US
#define LINEPERIOD_LATE_US 50 // A line starting later than this after its deadline counts as late
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
    uint8_t pngCompressionLevel = PNG_COMPRESSION_LEVEL; // Used when captureImage() encodes png directly

    // Exposure = time between two TG pulses. Set recordTiming to log every captured line into lineTimings
    bool recordTiming = false;
    std::vector<LineTiming> lineTimings;

    // Exposure control. The exposure of a line is the time between two line starts (TG pulses).
    // With fixedLinePeriod every line starts exactly one period after the previous one, so the exposure
    // no longer depends on processing time. The first periodCalibLines lines of a capture run as fast as
    // possible to measure the shortest possible period (readout, motor step, processing).
    // The period used is max(linePeriodUs, measured * LINEPERIOD_MARGIN + LINEPERIOD_EXTRA_US). 0 = shortest.
    bool fixedLinePeriod = true;
    int linePeriodUs = 0;
    static constexpr uint32_t periodCalibLines = 6;
    // Lines discarded at capture start on top of the line correction delay (lineBuffers). The sensor needs
    // about 5 lines to settle and the output must only contain lines taken with the fixed period
    static constexpr uint32_t startupSkipLines = periodCalibLines + 3;

    struct ExposureStats{
        int requestedUs = 0;     // linePeriodUs at capture start, 0 = shortest
        int minPeriodUs = 0;     // shortest safe period measured during the calibration lines
        int periodUs = 0;        // period used, this is the exposure time
        uint32_t lines = 0;      // lines started with the fixed period
        uint32_t lateLines = 0;  // lines started more than LINEPERIOD_LATE_US after their deadline
        int maxLateUs = 0;
    };
    ExposureStats exposureStats; // Of the current or last capture

    std::unique_ptr<CCD_ImageCorrection> imageCorrection;
    CCD_Ctrl& ccd;
    Motors& motors;
protected:
    void runOffsetLines();
    //CCD_Ctrl ccd;
    std::thread processThread;
    std::optional<std::chrono::steady_clock::time_point> lastLineStart;
    uint32_t lineIndex = 0; // Lines since startCapture()
    std::vector<std::chrono::steady_clock::time_point> calibStarts;
    void finishPeriodCalibration();
    std::atomic<bool> captureInProgress = false; // Cleared by abortImageCapture() from other threads
};

#endif // CAMERACONTROL_H
