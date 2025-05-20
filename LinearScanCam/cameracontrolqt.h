#ifndef CAMERACONTROLQT_H
#define CAMERACONTROLQT_H
#include <QTimer>
#include <QThread>
#include "cameracontrol.h"
#include <QFuture>
#include <QtConcurrent/QtConcurrent>


class FuturePromiseTimer : public QObject{
    Q_OBJECT
public:

    FuturePromiseTimer(const std::function<int64_t(void)>& progressFunc,bool autodelete = true);
    virtual ~FuturePromiseTimer();

    void setInterval(int interval);
    void setStartEndValues(int64_t start, int64_t end);

    QPromise<void>** run(const std::function<void(void)>& targetFunc);

signals:
    void busy(bool busy);
    void progress(int64_t val);

public slots:
    void emitProgress();
    void beginProcess();
    void endProcess();



protected:
    void runner(QPromise<void>& promise,const std::function<void(void)>& targetFunc);

    QTimer progressTimer;
    QPromise<void>* curpromise;

    const std::function<int64_t(void)>& progressFunc;

    int64_t start;
    int64_t end;

    bool autodelete;
};

//QFuture<void> future = QtConcurrent::run(aFunction);

// Helper class to control a motor directly from Qt
class MotorControlQt : public QObject{
    Q_OBJECT
public:
    //MotorControlQt();
    MotorControlQt(TMC2209_motor* motor);
    ~MotorControlQt();

    QFuture<void> home();
    //void homeRunner(QPromise<void> &promise);
    QFuture<void> move(int64_t steps,float speed, float accel);
    //void moveRunner(QPromise<void> &promise,int64_t steps,float speed, float accel);
    QFuture<void> moveTo(int64_t pos);
    //void run(QPromise<int> &promise);

    int64_t homesteps = -40000;
    int homePre = 1000;

    TMC2209_motor* motor;
signals:
    void position(int64_t val);
    void progress(float val);
    void idle(bool idle);
    void busy(bool busy);

public slots:
    void emitProgress();

    void motorBegin();
    void motorEnd();

protected:
    //const std::function<void(void)>& target;
    //const std::function<int64_t(void)>& progressTarget;

    QTimer progressTimer;
    bool running=false;

    QPromise<void>* curpromise = nullptr;

    int64_t startpos;
    int64_t targetpos;

    const float MAXPROGRESS = 1000.0f;

};

class LinCCDQt : public QObject{
    Q_OBJECT
public:
    LinCCDQt(){};
};

// Main control class
class CameraControllerQt : public QObject{
    Q_OBJECT
public:
    CameraControllerQt(QObject *parent);
    ~CameraControllerQt();

    bool init();

    MotorControlQt slideMotorQt;

    // libcamera cam0
signals:
    void ready(bool);
    void busy(bool);
    void progress(int);
};


#endif // CAMERACONTROLQT_H
