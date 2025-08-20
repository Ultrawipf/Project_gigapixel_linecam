#include "cameracontrolqt.h"

#include <iostream>
#include <QFutureWatcher>
#include "cameracontrol.h"
#include <spng.h>
#include <imagehelpers.hpp>
#include <algorithm>

#ifndef DUMMY
#include "ccd_ctrl.h"
#endif
Motors motordrivers; // Contains the motor drivers
PinCtrl pinctrl;



CameraControllerQt::CameraControllerQt(QObject *parent,LinCCDQt *ccd,PcfIoQt *pcfio) :
    QObject(parent),
    slideMotorQt(MotorControlQt(motordrivers.slideMotor_p)),
    LinearCamCtrl(motordrivers,*static_cast<CCD_Ctrl*>(ccd)),
    ccd(ccd),
    pcfio(pcfio)
{
    connect(&slideMotorQt, &MotorControlQt::progress,this,&CameraControllerQt::progress);
    pcfio->setMask(0b111);
}

CameraControllerQt::~CameraControllerQt()
{
    delete ccd;
}

auto callbackfunc = [](const std::vector<ccdpixel>& line){
    qDebug() << "Got line with len: " << line.size();
};

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


void CameraControllerQt::captureCallback(const std::vector<ccdpixel>& line){

    int progressval = MAXPROGRESS*((float)curLineNum/(float)curLines);
    // qDebug() << "Got line with len: " << line.size() << "Progress" << progressval;
    curLineNum++;
    if(enableLineSignal && curLineNum % previewSkip == 0){
        // qDebug() << "Sending preview line...";
        previewLine.clear();
        for(int i = 0;i<line.size();i+=previewSkip){
            previewLine.push_back(line[i]);
        }
        // std::copy_if(line.begin(),line.end(),std::back_inserter(previewLine),[this](ccdpixel i){return curLineNum % previewSkip == 0;});
        emit previewLineCaptured(previewLine);
        // qDebug() << "PVL" << previewLine.size();
    }

    emit this->progress(progressval);


}
// Scan an image and save to file
// TODO: Also return lines for a preview image
QFuture<void> CameraControllerQt::captureImageFuture(std::string filename,uint32_t lines,int usDelay,QThreadPool *pool){
    curLineNum = 0;
    curLines = lines;
    auto params = LinearCamCtrl::ccd.getCurParams();
    previewSkip = std::max<uint32_t>(0,previewDiv - params.moveLines);
    //std::vector<ccdpixel> previewLine;
    previewLine.clear();
    previewLine.reserve((params.validPixelEnd-params.validPixelEnd) / previewSkip);
    qDebug() << "Preview skip:" << previewSkip;
    // static std::function<void(const std::vector<ccdpixel>&)>callback{ [&,this,previewSkip](const std::vector<ccdpixel>& line){
    //     int progressval = MAXPROGRESS*((float)curLineNum/(float)curLines);
    //     // qDebug() << "Got line with len: " << line.size() << "Progress" << progressval;
    //     curLineNum++;
    //     if(curLineNum % previewSkip == 0){
    //         qDebug() << "Sending preview line...";
    //         std::transform(line.begin(),line.end(),std::back_inserter(previewLine),[this,previewSkip](ccdpixel i){return true;});
    //         //emit previewLineCaptured(previewLine);
    //         qDebug() << "PVL" << previewLine.size();
    //     }

    //     emit this->progress(progressval);
    // }
    // };
    qDebug() << "Capture image to file: "<< filename.c_str() << " lines " << lines;
    // emit busy(true);

    QFutureWatcher<void>* watcher = new QFutureWatcher<void>();
    connect(watcher,&QFutureWatcher<void>::finished,watcher,&QFutureWatcher<void>::deleteLater,Qt::SingleShotConnection);
    // Connect start and end functions
    connect(watcher,&QFutureWatcher<void>::started,[this](){emit busy(true);});
    connect(watcher,&QFutureWatcher<void>::finished,[this](){emit busy(false);emit imageCaptureDone();});
    // Start task
    auto callbackfunc = std::bind(&CameraControllerQt::captureCallback,this,std::placeholders::_1);
    QFuture<void> future = QtConcurrent::run(pool,&LinearCamCtrl::captureImage,this,filename,lines,usDelay,(callbackfunc));
    //QFuture<void> future = QtConcurrent::run(&MotorControlQt::homeRunner,this); // [this](QPromise<void> &promise){this->homeRunner(promise);}
    watcher->setFuture(future);


    //LinearCamCtrl::captureImage(filename,lines,usDelay);
    //emit busy(false);
    return future;
}

void CameraControllerQt::abortCapture(){
    LinearCamCtrl::abortImageCapture();
}

fs::path CameraControllerQt::getUnusedImageName(fs::path path){
    std::string basename = path.stem().string(); // image.png --> image
    std::string filename = path.filename().string(); // image.png
    std::string ext = path.extension().string();
    fs::path basepath = path.parent_path();
    fs::path newpath = path.replace_filename(fs::path(basename+"0"+ext));

    // Find first unused name in basepath
    for(uint32_t i = 0 ; i< INT32_MAX ; i++){
        newpath = path.replace_filename(fs::path(basename+std::to_string(i)+ext));
        if(!fs::exists(newpath)){
            break;
        }
    }
    std::cout << "Inpath: " << path.string() << " new path " << newpath.string() << " filename " << filename << std::endl;
    return newpath;
}

fs::path CameraControllerQt::moveTempImage(fs::path sourcefile,fs::path destpath){
    //qDebug() << "Dest path" << destpath.string().c_str();
    std::cout << "Filename: " << sourcefile.string() << " new path " << destpath.string() << " filename dest " << destpath.filename() << " filename source " << sourcefile.filename() << std::endl;
    fs::path newpath = getUnusedImageName(fs::path(destpath.replace_filename(sourcefile.filename())));

    try {
        fs::create_directories(destpath.stem());
        fs::rename(sourcefile, newpath);
        return newpath;
    } catch (fs::filesystem_error& e) {
        std::cout << e.what() << '\n';
        return fs::path();
    }
}
QFuture<fs::path> CameraControllerQt::reencodePngFuture(fs::path sourcefile,fs::path destfile,uint32_t downsample,uint8_t compressionLevel,spng_format format,bool gammacorrect,QThreadPool *pool){
    QFutureWatcher<fs::path>* watcher = new QFutureWatcher<fs::path>();
    connect(watcher,&QFutureWatcher<fs::path>::finished,watcher,&QFutureWatcher<fs::path>::deleteLater,Qt::SingleShotConnection);
    QFuture<fs::path> future = QtConcurrent::run(pool,&CameraControllerQt::reencodePng,this,sourcefile,destfile,downsample,compressionLevel,format,gammacorrect);
    watcher->setFuture(future);
    return future;
}
fs::path CameraControllerQt::reencodePng(fs::path sourcefile,fs::path destfile,uint32_t downsample,uint8_t compressionLevel,spng_format format,bool gammacorrect){
    //fs::path newpath = getUnusedImageName(fs::path(destpath.replace_filename(sourcefile.filename())));
    auto progresscb = [this](float val){
        emit progress(val*MAXPROGRESS);
    };

    try {
        fs::create_directories(destfile.stem());
        FILE* infile = fopen(sourcefile.string().c_str(),"rb");
        FILE* outfile = fopen(destfile.string().c_str(),"wb");
        png::pngReencode(infile,outfile,downsample,compressionLevel,format,gammacorrect,progresscb);
        fclose(infile);
        fclose(outfile);
        return destfile;
    } catch (std::exception& e) {
        std::cout << e.what() << '\n';
        return fs::path();
    }
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
    connect(watcher,&QFutureWatcher<bool>::finished,watcher,&QFutureWatcher<bool>::deleteLater,Qt::SingleShotConnection);
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

QFuture<void> MotorControlQt::moveMM(float mm,float speed, float accel,QThreadPool *pool){
    if(!motor) return QtFuture::makeReadyFuture();
    startpos = motor->getActualPos();
    targetpos = startpos + mm;

    QFutureWatcher<void>* watcher = new QFutureWatcher<void>();
    connect(watcher,&QFutureWatcher<void>::finished,watcher,&QFutureWatcher<void>::deleteLater,Qt::SingleShotConnection);
    // Connect start and end functions
    connect(watcher,&QFutureWatcher<void>::started,this,&MotorControlQt::motorBegin,Qt::SingleShotConnection);
    connect(watcher,&QFutureWatcher<void>::finished,this,&MotorControlQt::motorEnd,Qt::SingleShotConnection);
    // Start task
    QFuture<void> future = QtConcurrent::run(pool,[this,mm,speed,accel]{this->motor->moveMM(mm,speed,accel);});
    //QFuture<void> future = QtConcurrent::run(&MotorControlQt::moveRunner,this,steps,speed,accel); // [this](QPromise<void> &promise){this->homeRunner(promise);}
    watcher->setFuture(future);
    return future;
}
QFuture<void> MotorControlQt::moveToMM(float mm,float speed, float accel,QThreadPool *pool){
    //std::cout << "Moving to mm QT "<< mm << std::endl;
    return moveMM(mm - motor->getActualPos(),speed,accel,pool);
}

float MotorControlQt::getCurPos(){
    return motor->getActualPos();
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

QFuture<void> MotorControlQt::home(QThreadPool *pool){
    if(!motor) return QtFuture::makeReadyFuture();

    targetpos = 0;
    startpos = motor->getActualPos();

    // Create future watcher
    QFutureWatcher<void>* watcher = new QFutureWatcher<void>();
    connect(watcher,&QFutureWatcher<void>::finished,watcher,&QFutureWatcher<void>::deleteLater,Qt::SingleShotConnection);
    // Connect start and end functions
    connect(watcher,&QFutureWatcher<void>::started,this,&MotorControlQt::motorBegin,Qt::SingleShotConnection);
    connect(watcher,&QFutureWatcher<void>::finished,this,&MotorControlQt::motorEnd,Qt::SingleShotConnection);
    // Start task
    QFuture<void> future = QtConcurrent::run(pool,&Motors::homeSlide);
    //QFuture<void> future = QtConcurrent::run(&MotorControlQt::homeRunner,this); // [this](QPromise<void> &promise){this->homeRunner(promise);}
    watcher->setFuture(future);
    return future;

}
