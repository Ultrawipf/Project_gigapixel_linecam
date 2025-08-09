#ifndef CAMERACONTROLQT_H
#define CAMERACONTROLQT_H
#include <QTimer>
#include <QThread>
#include "cameracontrol.h"
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include "ccd_ctrl.h"
#include "pcfioqt.h"

#define MAXPROGRESS 1000.0f

// Helper class to control a motor directly from Qt
class MotorControlQt : public QObject{
    Q_OBJECT
public:
    //MotorControlQt();
    MotorControlQt(TMC2209_motor* motor);
    virtual ~MotorControlQt();

    QFuture<void> home();
    //void homeRunner(QPromise<void> &promise);
    QFuture<void> moveMM(float mm,float speed, float accel);
    //void moveRunner(QPromise<void> &promise,int64_t steps,float speed, float accel);
    QFuture<void> moveToMM(float pos,float speed, float accel);
    //void run(QPromise<int> &promise);
    float getCurPos();

    int64_t homesteps = -40000;
    int homePre = 1000;

    TMC2209_motor* motor;
signals:
    void position(double val);
    void progress(float val);
    void idle(bool idle);
    void busy(bool busy);

    void lineCaptured(std::vector<ccdpixel> line);

public slots:
    void emitProgress();
    double emitPosition();

    void motorBegin();
    void motorEnd();

protected:
    QTimer progressTimer;
    bool running=false;

    QPromise<void>* curpromise = nullptr;

    double startpos;
    double targetpos;
};

class LinCCDQt : public QObject, public CCD_Ctrl{
    Q_OBJECT
public:
    LinCCDQt();
    virtual ~LinCCDQt();
    QFuture<bool> init();

signals:
    void ready(bool);
    //void newLine(uint16_t * buf, uint32_t len);

public slots:
    void setClock(bool enable);

};

// Main control class
class CameraControllerQt : public QObject, public LinearCamCtrl{
    Q_OBJECT
public:
    CameraControllerQt(QObject *parent,LinCCDQt *ccd,PcfIoQt *pcfio);
    virtual ~CameraControllerQt();

    bool init();

    MotorControlQt slideMotorQt;
    LinCCDQt* ccd;
    PcfIoQt* pcfio;


    QFuture<void> captureImageFuture(std::string filename,uint32_t lines,int usDelay=0);
    void abortCapture();

    const float imageStartPosMM = 15; // Begin of valid image area
    const float imageCenterPosMM = 52.5; // Center of valid image area
    const float previewCamCenterMM = 25; // Position when preview camera is centered in lens
    //const float imageEndPosMM = 85; // End of valid image area
    const int fullResLines = 70000; // Begin of valid image area
    bool captureInProgress = false;

    // libcamera cam0
    //void captureCallback(const std::vector<ccdpixel>& line);
signals:
    void ready(bool);
    void busy(bool);
    void imageCaptureDone();
    void progress(int);
    void ccdStatus(QString);
    void motorStatus(QString);

private:
    uint32_t curLineNum = 0;
    uint32_t curLines = 0;
};


#endif // CAMERACONTROLQT_H
