#include "cameracontrolqt.h"

#include <iostream>
#include <QFutureWatcher>
#include "cameracontrol.h"

#ifndef DUMMY
#include "ccd_ctrl.h"
#endif
Motors motors; // Contains the motor drivers
PinCtrl pinctrl;

CameraControllerQt::CameraControllerQt(QObject *parent) :
    QObject(parent),
    slideMotorQt(MotorControlQt(motors.slideMotor_p))
#ifndef DUMMY
   ,ccd(new LinCCDQt)
#endif
{
    connect(&slideMotorQt, &MotorControlQt::progress,this,&CameraControllerQt::progress);
}

CameraControllerQt::~CameraControllerQt()
{
    delete ccd;
}

bool CameraControllerQt::init(){
    // Home motor
    bool flag = true;
    if(!slideMotorQt.motor->ping()){
        qDebug() << "Can't ping motor";
        emit ready(false);
        return false;
    }
    // Enable reset pin
    pinctrl.setEnablePin(true);
    emit motorStatus("STARTING");
    emit ccdStatus("STARTING");
    QFuture<void> motorfuture = slideMotorQt.home();
    emit motorStatus("HOMING");
    if(ccd){ // Init CCD while motor homes
        ccd->setClock(true);
        QFuture<bool> ccdfuture = ccd->init();
        ccdfuture.waitForFinished();
        if(!ccdfuture.result()){
            qDebug() << "CCD init failed";
            flag = false;
            ccd->setClock(false); // Disable clock if failed
            emit ccdStatus("FAILED");
        }
        emit ccdStatus("OK");
    }
    motorfuture.waitForFinished();
    emit motorStatus("OK");
    emit ready(flag);
    return flag;
}

// ----------------------------------------
LinCCDQt::LinCCDQt()
{
    qDebug() << "Starting CCD CTRL";
}
LinCCDQt::~LinCCDQt(){
    setClock(false);
}

void LinCCDQt::setClock(bool enable){
    qDebug() << "Setting CCD clock: "<<enable;
    CCD_Ctrl::setClock(enable);
}

QFuture<bool> LinCCDQt::init(){
    QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>();
    connect(watcher,&QFutureWatcher<bool>::finished,watcher,&QFutureWatcher<bool>::deleteLater);
    // Connect start and end functions
    // Start task
    QFuture<bool> future = QtConcurrent::run(&CCD_Ctrl::initCCD,this);
    //QFuture<void> future = QtConcurrent::run(&MotorControlQt::moveRunner,this,steps,speed,accel); // [this](QPromise<void> &promise){this->homeRunner(promise);}
    watcher->setFuture(future);
    return future;
}


// ----------------------------------------

MotorControlQt::MotorControlQt(TMC2209_motor* motor)
    : motor(motor)
{
    progressTimer.setInterval(32);
    progressTimer.callOnTimeout(this,&MotorControlQt::emitProgress);
}

MotorControlQt::~MotorControlQt(){
    std::cout << "Deleting motorcontroller qt" << std::endl;
}

double MotorControlQt::emitPosition(){
    double curpos = motor->getActualPos();
    emit position(curpos);
    return curpos;
}

void MotorControlQt::emitProgress(){
    double curpos = emitPosition();
    if(targetpos != startpos){
        float progressVal = std::abs(MAXPROGRESS*(curpos-startpos) / (targetpos-startpos));
        emit progress(progressVal);
        if(curpromise){
            curpromise->setProgressValue(progressVal);
        }
    }
}

QFuture<void> MotorControlQt::moveMM(float mm,float speed, float accel){
    if(!motor) return QtFuture::makeReadyFuture();
    startpos = motor->getActualPos();
    targetpos = startpos + mm;

    QFutureWatcher<void>* watcher = new QFutureWatcher<void>();
    connect(watcher,&QFutureWatcher<void>::finished,watcher,&QFutureWatcher<void>::deleteLater);
    // Connect start and end functions
    connect(watcher,&QFutureWatcher<void>::started,this,&MotorControlQt::motorBegin);
    connect(watcher,&QFutureWatcher<void>::finished,this,&MotorControlQt::motorEnd);
    // Start task
    QFuture<void> future = QtConcurrent::run([this,mm,speed,accel]{this->motor->moveMM(mm,speed,accel);});
    //QFuture<void> future = QtConcurrent::run(&MotorControlQt::moveRunner,this,steps,speed,accel); // [this](QPromise<void> &promise){this->homeRunner(promise);}
    watcher->setFuture(future);
    return future;
}
QFuture<void> MotorControlQt::moveToMM(float mm,float speed, float accel){
    return moveMM(mm - motor->getActualPos(),speed,accel);
}


/*
void MotorControlQt::moveRunner(QPromise<void> &promise,int64_t steps,float speed, float accel){
    promise.setProgressRange(0,100);
    curpromise = &promise;
    motor->move(steps,speed,accel);
}

void MotorControlQt::homeRunner(QPromise<void> &promise){
    promise.setProgressRange(0,100);
    curpromise = &promise;
    Motors::homeSlide();
}
*/
void MotorControlQt::motorEnd(){
    emit progress(MAXPROGRESS);
    progressTimer.stop();
    emit idle(true);
    emit busy(false);
    curpromise = nullptr;
    emitPosition();
}

void MotorControlQt::motorBegin(){
    progressTimer.start();
    emit progress(0);
    emit idle(false);
    emit busy(true);
}

QFuture<void> MotorControlQt::home(){
    if(!motor) return QtFuture::makeReadyFuture();

    targetpos = 0;
    startpos = motor->getActualPos();

    // Create future watcher
    QFutureWatcher<void>* watcher = new QFutureWatcher<void>();
    connect(watcher,&QFutureWatcher<void>::finished,watcher,&QFutureWatcher<void>::deleteLater);
    // Connect start and end functions
    connect(watcher,&QFutureWatcher<void>::started,this,&MotorControlQt::motorBegin);
    connect(watcher,&QFutureWatcher<void>::finished,this,&MotorControlQt::motorEnd);
    // Start task
    QFuture<void> future = QtConcurrent::run(&Motors::homeSlide);
    //QFuture<void> future = QtConcurrent::run(&MotorControlQt::homeRunner,this); // [this](QPromise<void> &promise){this->homeRunner(promise);}
    watcher->setFuture(future);
    return future;

}
