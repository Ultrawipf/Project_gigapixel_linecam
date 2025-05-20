#include "cameracontrolqt.h"

#include <iostream>
#include <QFutureWatcher>
#include "cameracontrol.h"

Motors motors; // Contains the motor drivers
PinCtrl pinctrl;

CameraControllerQt::CameraControllerQt(QObject *parent) :
    QObject(parent),
    slideMotorQt(MotorControlQt(motors.slideMotor_p))
{
    connect(&slideMotorQt, &MotorControlQt::progress,this,&CameraControllerQt::progress);
}

CameraControllerQt::~CameraControllerQt()
{

}

bool CameraControllerQt::init(){
    // Home motor
    if(!slideMotorQt.motor->ping()){
        qDebug() << "Can't ping motor";
        emit ready(false);
        return false;
    }
    // Enable reset pin
    pinctrl.setEnablePin(true);
    QFuture<void> future = slideMotorQt.home();
    future.waitForFinished();
    emit ready(true);
    return true;
}



MotorControlQt::MotorControlQt(TMC2209_motor* motor)
    : motor(motor)
{
    progressTimer.setInterval(32);
    progressTimer.callOnTimeout(this,&MotorControlQt::emitProgress);
}

MotorControlQt::~MotorControlQt(){
    std::cout << "Deleting motorcontroller qt" << std::endl;
}

void MotorControlQt::emitProgress(){
    int64_t curpos = motor->getPos();
    emit position(curpos);
    if(targetpos != startpos){
        float progressVal = std::abs(MAXPROGRESS*(curpos-startpos) / (targetpos-startpos));
        emit progress(progressVal);
        if(curpromise){
            curpromise->setProgressValue(progressVal);
        }
    }
}

QFuture<void> MotorControlQt::move(int64_t steps,float speed, float accel){
    if(!motor) return QtFuture::makeReadyFuture();
    startpos = motor->getPos();
    targetpos = startpos + steps;

    QFutureWatcher<void>* watcher = new QFutureWatcher<void>();
    connect(watcher,&QFutureWatcher<void>::finished,watcher,&QFutureWatcher<void>::deleteLater);
    // Connect start and end functions
    connect(watcher,&QFutureWatcher<void>::started,this,&MotorControlQt::motorBegin);
    connect(watcher,&QFutureWatcher<void>::finished,this,&MotorControlQt::motorEnd);
    // Start task
    QFuture<void> future = QtConcurrent::run([this,steps,speed,accel]{this->motor->move(steps,speed,accel);});
    //QFuture<void> future = QtConcurrent::run(&MotorControlQt::moveRunner,this,steps,speed,accel); // [this](QPromise<void> &promise){this->homeRunner(promise);}
    watcher->setFuture(future);
    return future;
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
    startpos = -30000;

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

/*
    FuturePromiseTimer* promiseHelper = new FuturePromiseTimer([this]()->int64_t{return this->motor->getPos();});
    connect(promiseHelper,&FuturePromiseTimer::progress,this,&MotorControlQt::emitProgress);
    promiseHelper->run([]{Motors::homeSlide();});

*/
}


FuturePromiseTimer::FuturePromiseTimer(const std::function<int64_t(void)>& progressFunc,bool autodelete)
    : progressFunc(progressFunc),autodelete(autodelete)
{

    progressTimer.setInterval(32);
    progressTimer.callOnTimeout(this,&FuturePromiseTimer::emitProgress);
}

FuturePromiseTimer::~FuturePromiseTimer(){
    std::cout << "Kill me" << std::endl;
}

void FuturePromiseTimer::setInterval(int interval){
    this->progressTimer.setInterval(interval);
}

void FuturePromiseTimer::emitProgress(){
    int64_t val = progressFunc();
    emit progress(val);
}

void FuturePromiseTimer::endProcess(){
    std::cout << "Done" << std::endl;
    progressTimer.stop();
    emit busy(false);
    curpromise = nullptr;
    if(autodelete){
        this->deleteLater();
    }
}

void FuturePromiseTimer::beginProcess(){
    std::cout << "Start" << std::endl;
    progressTimer.start();
    emit progress(0);
    emit busy(true);
}

QPromise<void>** FuturePromiseTimer::run(const std::function<void(void)>& targetFunc){
    QFutureWatcher<void>* watcher = new QFutureWatcher<void>();
    QObject::connect(watcher,&QFutureWatcher<void>::finished,watcher,&QFutureWatcher<void>::deleteLater);
    // Connect start and end functions
    QObject::connect(watcher,&QFutureWatcher<void>::started,this,&FuturePromiseTimer::beginProcess);
    QObject::connect(watcher,&QFutureWatcher<void>::finished,this,&FuturePromiseTimer::endProcess);
    // Start task
    QFuture<void> future = QtConcurrent::run(&FuturePromiseTimer::runner,this,targetFunc); // [this](QPromise<void> &promise){this->homeRunner(promise);}
    watcher->setFuture(future);
    return &curpromise;
}

void FuturePromiseTimer::runner(QPromise<void> &promise,const std::function<void(void)>& targetFunc){
    curpromise = &promise;
    targetFunc(); // Execute function
}



