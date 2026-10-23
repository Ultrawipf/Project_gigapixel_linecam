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
        }else{
            emit ccdStatus("OK");
        }
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
void CameraControllerQt::captureImageBlocking(std::string filename,uint32_t lines,int usDelay,bool nomovemode){
    curLineNum = 0;
    curLines = lines;
    auto params = LinearCamCtrl::ccd.getCurParams();
    previewSkip = std::max<int>(1,previewDiv - (int)params.moveLines); // Never 0, used as divisor
    //std::vector<ccdpixel> previewLine;
    previewLine.clear();
    previewLine.reserve((params.validPixelEnd-params.validPixelBegin) / previewSkip);
    qDebug() << "Preview skip:" << previewSkip;

    qDebug() << "Capture image to file: "<< filename.c_str() << " lines " << lines;
    auto motorlock = slideMotorQt.lock(); // captureImage() moves the slide directly
    emit busy(true);
    auto callbackfunc = std::bind(&CameraControllerQt::captureCallback,this,std::placeholders::_1);
    try{
        LinearCamCtrl::captureImage(filename,lines,usDelay,callbackfunc,nomovemode);
    }catch(...){
        emit busy(false);
        throw;
    }
    emit busy(false);
    emit imageCaptureDone();
}

void CameraControllerQt::abortCapture(){
    LinearCamCtrl::abortImageCapture();
}

fs::path CameraControllerQt::getUnusedImageName(fs::path path){
    std::string basename = path.stem().string(); // image.png --> image
    std::string filename = path.filename().string(); // image.png
    std::string ext = path.extension().string();
    fs::path basepath = path.parent_path();
    fs::path newpath = basepath/fs::path(basename+"0"+ext);//path.replace_filename(fs::path(basename+"0"+ext));

    // Find first unused name in basepath
    for(uint32_t i = 0 ; i< INT32_MAX ; i++){
        newpath = basepath/fs::path(basename+std::to_string(i)+ext);//path.replace_filename(fs::path(basename+std::to_string(i)+ext));
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
        fs::create_directories(destpath.parent_path());
        fs::rename(sourcefile, newpath);
        return newpath;
    } catch (fs::filesystem_error& e) {
        std::cout << e.what() << '\n';
        return fs::path();
    }
}
QFuture<fs::path> CameraControllerQt::reencodePngFuture(fs::path sourcefile,fs::path destfile,uint32_t downsample,uint8_t compressionLevel,spng_format format,bool gammacorrect,QThreadPool *pool){
    //QFutureWatcher<fs::path>* watcher = new QFutureWatcher<fs::path>();
    //connect(watcher,&QFutureWatcher<fs::path>::finished,watcher,&QFutureWatcher<fs::path>::deleteLater,Qt::SingleShotConnection);
    QFuture<fs::path> future = QtConcurrent::run(pool,&CameraControllerQt::reencodePng,this,sourcefile,destfile,downsample,compressionLevel,format,gammacorrect);
    //watcher->setFuture(future);
    return future;
}
fs::path CameraControllerQt::reencodePng(fs::path sourcefile,fs::path destfile,uint32_t downsample,uint8_t compressionLevel,spng_format format,bool gammacorrect){
    //fs::path newpath = getUnusedImageName(fs::path(destpath.replace_filename(sourcefile.filename())));
    std::cout << "Filename: " << sourcefile.string() << " new path " << destfile.string() << " filename dest " << destfile.filename() << " filename source " << sourcefile.filename() << std::endl;

    auto progresscb = [this](float val){
        emit progress(val*MAXPROGRESS);
    };

    try {
        fs::create_directories(destfile.parent_path());
        FILE* infile = fopen(sourcefile.string().c_str(),"rb");
        FILE* outfile = fopen(destfile.string().c_str(),"wb");
        if(!infile || !outfile){
            std::cout << "Cannot open " << (infile ? destfile : sourcefile).string() << '\n';
            if(infile) fclose(infile);
            if(outfile) fclose(outfile);
            return fs::path();
        }
        png::pngReencode(infile,outfile,downsample,compressionLevel,format,gammacorrect,progresscb);
        fclose(infile);
        fclose(outfile);
        return destfile;
    } catch (std::exception& e) {
        std::cout << e.what() << '\n';
        return fs::path();
    }
}

QFuture<fs::path> CameraControllerQt::reencodeRawToPngFuture(fs::path sourcefile,fs::path destfile,uint32_t height, uint32_t width,uint8_t compressionLevel,QThreadPool *pool){
    QFuture<fs::path> future = QtConcurrent::run(pool,&CameraControllerQt::reencodeRawToPng,this,sourcefile,destfile,height,width,compressionLevel);
    return future;
}

fs::path CameraControllerQt::reencodeRawToPng(fs::path sourcefile,fs::path destfile,uint32_t height, uint32_t width,uint8_t compressionLevel){
    auto progresscb = [this](float val){
        emit progress(val*MAXPROGRESS);
    };

    try {
        fs::create_directories(destfile.parent_path());
        FILE* infile = fopen(sourcefile.string().c_str(),"rb");
        FILE* outfile = fopen(destfile.string().c_str(),"wb");
        if(!infile || !outfile){
            std::cout << "Cannot open " << (infile ? destfile : sourcefile).string() << '\n';
            if(infile) fclose(infile);
            if(outfile) fclose(outfile);
            return fs::path();
        }
        png::rawToPng(infile,outfile,height,width,sizeof(ccdpixel),pnggamma,compressionLevel,progresscb);
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
    // No QFutureWatcher here: init() is called from a worker thread without an event loop
    return QtConcurrent::run(&CCD_Ctrl::initCCD,this);
}


// ----------------------------------------

MotorControlQt::MotorControlQt(TMC2209_motor* motor)
    : motor(motor)
{
    motorPool.setMaxThreadCount(1); // Execute queued moves strictly in order
    progressTimer.setInterval(32);
    progressTimer.callOnTimeout(this,&MotorControlQt::emitProgress);
}

MotorControlQt::~MotorControlQt(){
    motorPool.waitForDone();
    std::cout << "Deleting motorcontroller qt" << std::endl;
}

double MotorControlQt::emitPosition(){
    double curpos = motor->getActualPos();
    emit position(curpos);
    return curpos;
}

void MotorControlQt::emitProgress(){
    if(!enableProgress) return;
    double curpos = emitPosition();
    const double start = startpos;
    const double target = targetpos;
    if(target != start){
        float progressVal = std::abs(MAXPROGRESS*(curpos-start) / (target-start));
        emit progress(progressVal);
    }
}

std::unique_lock<std::mutex> MotorControlQt::lock(){
    return std::unique_lock<std::mutex>(motorMutex);
}

// Begin/end handlers touch the timer and are connected to widgets. Always run them on the GUI thread
void MotorControlQt::notifyBegin(){
    QMetaObject::invokeMethod(this,&MotorControlQt::motorBegin,Qt::QueuedConnection);
}

void MotorControlQt::notifyEnd(){
    QMetaObject::invokeMethod(this,&MotorControlQt::motorEnd,Qt::QueuedConnection);
}

void MotorControlQt::moveMMLocked(float mm,float speed, float accel){
    // Position is read under the lock so queued moves start from the real position
    startpos = motor->getActualPos();
    mm = std::min<double>(maxPos-startpos,std::max<double>(minPos-startpos,mm));
    targetpos = startpos + mm;
    notifyBegin();
    try{
        motor->moveMM(mm,speed,accel);
    }catch(...){
        notifyEnd();
        throw;
    }
    notifyEnd();
}

void MotorControlQt::moveMMBlocking(float mm,float speed, float accel){
    if(!motor) return;
    std::lock_guard<std::mutex> guard(motorMutex);
    moveMMLocked(mm,speed,accel);
}

void MotorControlQt::moveToMMBlocking(float pos,float speed, float accel){
    if(!motor) return;
    std::lock_guard<std::mutex> guard(motorMutex);
    std::cout << "Moving to mm QT "<< pos << std::endl;
    moveMMLocked(pos - motor->getActualPos(),speed,accel);
}

void MotorControlQt::homeBlocking(){
    if(!motor) return;
    std::lock_guard<std::mutex> guard(motorMutex);
    targetpos = 0;
    startpos = motor->getActualPos();
    notifyBegin();
    try{
        Motors::homeSlide();
    }catch(...){
        notifyEnd();
        throw;
    }
    notifyEnd();
}

QFuture<void> MotorControlQt::moveMM(float mm,float speed, float accel,QThreadPool *pool){
    if(!motor) return QtFuture::makeReadyFuture();
    return QtConcurrent::run(pool ? pool : &motorPool,[this,mm,speed,accel]{moveMMBlocking(mm,speed,accel);});
}

QFuture<void> MotorControlQt::moveToMM(float pos,float speed, float accel,QThreadPool *pool){
    if(!motor) return QtFuture::makeReadyFuture();
    // Target is converted to a relative move only when the move actually starts
    return QtConcurrent::run(pool ? pool : &motorPool,[this,pos,speed,accel]{moveToMMBlocking(pos,speed,accel);});
}

QFuture<void> MotorControlQt::home(QThreadPool *pool){
    if(!motor) return QtFuture::makeReadyFuture();
    return QtConcurrent::run(pool ? pool : &motorPool,[this]{homeBlocking();});
}

float MotorControlQt::getCurPos(){
    return motor->getActualPos();
}

void MotorControlQt::motorEnd(){
    emit progress(MAXPROGRESS);
    progressTimer.stop();
    emit idle(true);
    emit busy(false);
    emitPosition();
}

void MotorControlQt::motorBegin(){
    progressTimer.start();
    emit progress(0);
    emit idle(false);
    emit busy(true);
}
