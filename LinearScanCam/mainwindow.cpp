#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include "../linearcam_pi/cameracontrol.h"
#include <QFuture>
#include <QFutureWatcher>

#include "cameracontrolqt.h"
#include "confstore.h"
#include "pcfioqt.h"
#include <memory>
#include <QColorSpace>

#include <QImageReader>
#include <QToolTip>
#include <QMetaObject>

#include "imagehelpers.hpp"

QThreadPool MainWindow::pool = QThreadPool();

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , ctrl(new CameraControllerQt(this,new LinCCDQt,new PcfIoQt))
    , config(CONFFILE)
{

    QImageReader::setAllocationLimit(512);
    pool.setMaxThreadCount(1);

    ui->setupUi(this);
    ui->progressBar_scan->setVisible(false);
    ui->stackedWidget->setCurrentWidget(ui->page_boot);
    connect(ctrl, &CameraControllerQt::ready,this,&MainWindow::setReady);
    updateControlsEnabled(); // Everything disabled until the camera is ready
    ui->pushButton_cancel->setVisible(false);
    connect(ui->pushButton_cancel,&QPushButton::clicked,this,&MainWindow::cancelOperation);

    qDebug() << "Startpos:" << ctrl->imageStartPosMM << "End:" << ctrl->imageEndPosMM << "Center:" << ctrl->imageCenterPosMM;

    // Setup progressbar
    progressbarHideTimer.moveToThread(this->thread());
    progressbarHideTimer.setSingleShot(true);
    progressbarHideTimer.callOnTimeout([this]{this->ui->progressBar_scan->setVisible(false);});
    progressbarHideTimer.setInterval(250);
    connect(ctrl, &CameraControllerQt::progress,this,&MainWindow::progressCb,Qt::QueuedConnection);

    // Motor controls are enabled/disabled by updateControlsEnabled()
    // Motor position debug
    connect(&ctrl->slideMotorQt, &MotorControlQt::position,ui->doubleSpinBox_motorpos_debug,&QDoubleSpinBox::setValue);
    connect(&ctrl->slideMotorQt, &MotorControlQt::position,ui->doubleSpinBox_motorpos_preview,&QDoubleSpinBox::setValue);

    // Cam control
    connect(ui->comboBox_resolution,&QComboBox::currentIndexChanged,this,&MainWindow::updateResEstimate);

    // Movement buttons
    connect(ui->pushButton_debug_home,&QPushButton::clicked,this,&MainWindow::home);
    connect(ui->pushButton_debug_left,&QPushButton::clicked,this,&MainWindow::moveLeft);
    connect(ui->pushButton_debug_right,&QPushButton::clicked,this,&MainWindow::moveRight);
    connect(ui->pushButton_preview_left,&QPushButton::clicked,this,&MainWindow::moveLeft);
    connect(ui->pushButton_preview_right,&QPushButton::clicked,this,&MainWindow::moveRight);
    connect(ui->pushButton_preview_center,&QPushButton::clicked,this,[this](){
        runMotorOperation("Moving sensor",[this](){return ctrl->slideMotorQt.moveToMM(ctrl->previewCamCenterMM,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);});
    });
    // Dist
    ui->doubleSpinBox_debug_dist->setValue(this->curMovementSize);
    ui->doubleSpinBox_preview_dist->setValue(this->curMovementSize);
    connect(ui->doubleSpinBox_debug_dist,&QDoubleSpinBox::valueChanged,ui->doubleSpinBox_preview_dist,&QDoubleSpinBox::setValue);
    connect(ui->doubleSpinBox_preview_dist,&QDoubleSpinBox::valueChanged,this,&MainWindow::changeMovementSize); // Done by UI signal connection
    // Debug info
    connect(ctrl,&CameraControllerQt::ccdStatus,this,[this](QString t){ui->label_ccdstatus_debug->setText("CCD Status: " + t);});
    connect(ctrl,&CameraControllerQt::motorStatus,this,[this](QString t){ui->label_debug_motorstatus->setText("Motor Status: " + t);});
    connect(ctrl,&CameraControllerQt::ccdStatus,this,[this](QString t){ui->label_ccd_status_startup->setText("CCD Status: " + t);});
    connect(ctrl,&CameraControllerQt::motorStatus,this,[this](QString t){ui->label_motor_status_startup->setText("Motor Status: " + t);});


    connect(ui->doubleSpinBox_gamma,&QDoubleSpinBox::valueChanged,this,[this](double value){ctrl->pnggamma = value;});
    // Preview line
    //connect(ctrl,&CameraControllerQt::previewLineCaptured,this,[this](const std::vector<ccdpixel>& line){qDebug() << line.size();});
    //connect(ctrl,&CameraControllerQt::previewLineCaptured,ui->graphicsView,&ImageviewQt::showLine);

    // Buttons
    connect(ctrl->pcfio,&PcfIoQt::buttonPressed,this,&MainWindow::buttonExtCb);

    // Camera controls
    connect(ui->pushButton_shutter,&QPushButton::clicked,this,&MainWindow::captureImage);
    connect(ui->pushButton_preview,&QPushButton::clicked,this,&MainWindow::capturePreviewImage);
    connect(ui->pushButton_savepreview,&QPushButton::clicked,this,&MainWindow::savePreview);


    restoreSettings(config);
    ui->horizontalSlider_imagewidth->setValue(500);

    // Wait for init
    QFuture<bool> future = QtConcurrent::run(&CameraControllerQt::init,ctrl);

    connect(&ctrl->slideMotorQt, &MotorControlQt::busy,this,&MainWindow::busyCb);

    ui->graphicsView->setScene(new QGraphicsScene);

    // Gallery
    connect(ui->gallerywidget, &GalleryQt::itemSelected,this,&MainWindow::updateGallerySelection);
    connect(ui->pushButton_galleryDelete, &QPushButton::clicked,ui->gallerywidget,&GalleryQt::deleteImage);
}

MainWindow::~MainWindow()
{
    saveSettings(config);
    std::cout << "Deleting mainwindow "<< std::endl;
    delete ui;
    delete ctrl;

    //delete camerathread;
}

void MainWindow::buttonExtCb(uint8_t buttons){
    qDebug() << "Ext BTN" << buttons;
    if(buttons & 0x1){ // Shutter
        captureImage();
    }else
    if(buttons & 0x2){ // Preview
        capturePreviewImage();
    }else
    if(buttons & 0x4){ // Focus
        ui->radioButton_mode_preview->click();
    }
}

// Thread safe. May be called from worker threads
void MainWindow::statusMsgCb(QString status){
    runOnGui([this,status](){ui->label_status->setText(status);});
}

void MainWindow::busyCb(bool busy){
    motorBusy = busy;
    updateControlsEnabled();
}

// Controls that can start motor movements or captures are only enabled when nothing is running
void MainWindow::updateControlsEnabled(){
    const bool enabled = cameraReady && !operationActive && !motorBusy;
    ui->groupBox_camactions->setEnabled(enabled);
    ui->frame_mode->setEnabled(enabled);
    ui->groupBox_previewctrl->setEnabled(enabled);
    ui->groupBox_motorcontrol_debug->setEnabled(enabled);
}

// Only one operation (capture, preview, motor move...) may run at a time.
// Every entry point (UI and external buttons) must go through this. GUI thread only
bool MainWindow::tryBeginOperation(const QString& name){
    if(!cameraReady){
        statusMsgCb("Camera not ready");
        return false;
    }
    if(operationActive){
        qDebug() << "Rejected" << name << "while busy with" << activeOperation;
        statusMsgCb("Busy: " + activeOperation);
        return false;
    }
    operationActive = true;
    activeOperation = name;
    abortRequested = false;
    updateControlsEnabled();
    return true;
}

void MainWindow::endOperation(){
    operationActive = false;
    activeOperation.clear();
    setCancellable(false);
    updateControlsEnabled();
}

// Shows the cancel button for the running operation
void MainWindow::setCancellable(bool cancellable){
    ui->pushButton_cancel->setVisible(cancellable);
    ui->pushButton_cancel->setEnabled(cancellable);
}

void MainWindow::cancelOperation(){
    if(!operationActive){
        return;
    }
    qDebug() << "Cancel" << activeOperation;
    abortRequested = true;
    ctrl->abortCapture(); // Stops the line loop if a scan is running
    ui->pushButton_cancel->setEnabled(false);
    statusMsgCb("Cancelling...");
}

// Sleeps up to seconds, returns early on cancel. Job thread
void MainWindow::interruptibleSleep(int seconds){
    for(int i = 0;i < seconds*10 && !abortRequested;i++){
        QThread::msleep(100);
    }
}

// Starts a motor movement as an operation. doneMsg is shown when it finished
void MainWindow::runMotorOperation(const QString& name,std::function<QFuture<void>()> start,const QString& doneMsg){
    if(!tryBeginOperation(name)){
        return;
    }
    statusMsgCb(name + "...");
    // Continuation taking the future runs even if the move failed
    start().then(this,[this,doneMsg](QFuture<void> f){
        Q_UNUSED(f);
        endOperation();
        if(!doneMsg.isEmpty()){
            statusMsgCb(doneMsg);
        }
    });
}
// Runs a capture/processing job on the single job thread. Exceptions are reported in the status line
void MainWindow::runJob(std::function<void()> job){
    (void)QtConcurrent::run(&pool,[this,job](){
        try{
            job();
        }catch(const JobCancelled&){
            statusMsgCb("Cancelled");
        }catch(const std::exception& e){
            qDebug() << "Job failed:" << e.what();
            statusMsgCb(QString("Error: ") + e.what());
        }catch(...){
            qDebug() << "Job failed";
            statusMsgCb("Error: unknown failure");
        }
        ctrl->slideMotorQt.enableProgress = true;
        runOnGui([this](){endOperation();});
    });
}

// Thread safe. Loads the image in the calling thread and displays it on the GUI thread
void MainWindow::showImageFile(fs::path file){
    QImage img(QString::fromStdString(file.string()));
    runOnGui([this,img](){ui->graphicsView->showFullImage(img);});
}

void MainWindow::savePreview(){
    qDebug() << "Save preview";
    if(!tryBeginOperation("Saving preview")){
        return;
    }
    ui->pushButton_savepreview->setEnabled(false);
    uint32_t downsample = baseDownsamplePreview-ctrl->ccd->getCurParams().moveLines;

    runJob([this,downsample](){
        fs::path newfile = ctrl->getUnusedImageName(fs::path(imagepathfullres/"img.png"));
        fs::path lowresfile = imagepathlowres/(newfile.filename());
        try{
            fs::create_directories(newfile.parent_path());
            fs::rename(previewFile, newfile); // Move file
        }catch(...){
            runOnGui([this](){ui->pushButton_savepreview->setEnabled(true);}); // Preview is still there
            throw;
        }
        fs::path newpath = ctrl->reencodePng(newfile,lowresfile,downsample,pngCompression,SPNG_FMT_RGB8,true);
        if(newpath.has_filename()){
            statusMsgCb(QString("Saved image to: ") + QString::fromStdString(newpath.filename().string()));
        }else{
            statusMsgCb("Cannot save file...");
        }
    });
}

// Reads everything a capture needs from the widgets. GUI thread only
MainWindow::CaptureSettings MainWindow::readCaptureSettings(CCD_Resolution res){
    CaptureSettings s;
    s.res = res;
    s.gainOfs = gainOfsFromUi();
    s.nomovemode = ui->radioButton_linecam->isChecked();
    s.exposureUs = ui->spinBox_exposuretime->value()*1000;
    s.delayS = ui->spinBox_delay->value();
    s.widthscale = (float)ui->horizontalSlider_imagewidth->value() / (float)ui->horizontalSlider_imagewidth->maximum();
    if(s.nomovemode) s.widthscale *= nomovemult;
    return s;
}

void MainWindow::captureImage(){
    qDebug() << "SHUTTER";

    if(ui->stackedWidget->currentWidget() != ui->page_graphic){
        ui->radioButton_linearscan->click();
        return;
        // ui->stackedWidget->setCurrentWidget(ui->page_graphic);
        // ctrl->slideMotorQt.moveToMM(ctrl->imageStartPosMM,500,5,&pool);
        // ui->groupBox_camactions->setVisible(true);
    }
    if(!tryBeginOperation("Capturing image")){
        return;
    }
    setCancellable(true);
    statusMsgCb("Starting image capture...");
    CaptureSettings s = readCaptureSettings((CCD_Resolution)ui->comboBox_resolution->currentIndex());
    fs::path recordimagename = rawTempFile;

    int pngCompressionOvr = pngCompression;
    if((unsigned int)s.res < 3){
        pngCompressionOvr = 0; // No compression for high res
    }
    runJob([this,s,recordimagename,pngCompressionOvr](){
        fs::path newimagename = ctrl->getUnusedImageName(imagepathfullres/"img.png");
        captureImageJob(s,newimagename,recordimagename,pngCompressionOvr);
    });
}

// Full image capture sequence. Runs on the job thread
void MainWindow::captureImageJob(CaptureSettings s,fs::path newimagename,fs::path recordimagename,int pngCompressionOvr){
    MotorControlQt& motor = ctrl->slideMotorQt;
    // CCD setup sends hundreds of control words. Keep it off the GUI thread
    ctrl->ccd->setResolution(s.res);
    ctrl->ccd->setRgbGainOfs(s.gainOfs);
    const CCD_Parameters params = ctrl->ccd->getCurParams();
    const uint32_t lines = (ctrl->fullResLines / params.moveLines)*s.widthscale;

    float scanstartpos = ctrl->imageCenterPosMM;
    if(!s.nomovemode){
        scanstartpos = (ctrl->imageCenterPosMM - ctrl->imageStartPosMM) * (1.0-s.widthscale) + ctrl->imageStartPosMM;
        scanstartpos -= (params.lineBuffers * params.moveLines * ctrl->ccd->lineDistMM);
    }
    statusMsgCb("Moving to start pos...");
    motor.moveToMMBlocking(scanstartpos,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);
    interruptibleSleep(s.delayS);
    const float moveBackPos = motor.getCurPos();
    if(abortRequested){
        throw JobCancelled();
    }

    // Capture the image
    statusMsgCb("Capturing image...");
    motor.enableProgress = false;
    ctrl->captureImageBlocking(recordimagename.string(),lines,s.exposureUs,s.nomovemode);
    if(abortRequested){
        // Partial scan. Discard it and return to the start position
        std::error_code ec;
        fs::remove(recordimagename,ec);
        motor.enableProgress = true;
        statusMsgCb("Cancelled, moving back...");
        motor.moveToMMBlocking(moveBackPos,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);
        throw JobCancelled();
    }

    // Move back while encoding and show image
    statusMsgCb("Processing image...");
    auto mover = motor.moveToMM(moveBackPos,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);
    fs::path img = ctrl->reencodeRawToPng(recordimagename,newimagename,lines,params.validPixelEnd - params.validPixelBegin,3);
    std::error_code ec;
    fs::remove(recordimagename,ec); // delete temp file
    if(img.empty()){
        mover.waitForFinished();
        throw std::runtime_error("Encoding image failed");
    }

    statusMsgCb("Processing preview...");
    fs::path lowresfile = imagepathlowres/(newimagename.filename());
    ctrl->reencodePng(img,lowresfile,baseDownsamplePreview-params.moveLines,pngCompressionOvr,SPNG_FMT_RGB8,true);
    statusMsgCb("Preview Done");
    showImageFile(lowresfile);

    motor.enableProgress = true;
    statusMsgCb("Moving back...");
    mover.waitForFinished();
    statusMsgCb("Done");
}

void MainWindow::capturePreviewImage(){
    qDebug() << "PREVIEW";

    if(ui->stackedWidget->currentWidget() != ui->page_graphic){
        statusMsgCb("Preview mode");
        ui->radioButton_linearscan->click();
        return;
    }

    if(!tryBeginOperation("Capturing preview")){
        return;
    }
    setCancellable(true);
    statusMsgCb("Starting preview capture...");
    ui->pushButton_savepreview->setEnabled(false);
    CaptureSettings s = readCaptureSettings(CCD_Resolution::Preview); // Lowest resolution
    runJob([this,s](){capturePreviewJob(s);});
}

// Preview capture sequence. Runs on the job thread
void MainWindow::capturePreviewJob(CaptureSettings s){
    MotorControlQt& motor = ctrl->slideMotorQt;
    ctrl->ccd->setResolution(s.res);
    ctrl->ccd->setRgbGainOfs(s.gainOfs);
    const CCD_Parameters params = ctrl->ccd->getCurParams();
    const uint32_t lines = (ctrl->fullResLines / params.moveLines)*s.widthscale;
    qDebug() << "scale" << s.widthscale << "lines" << lines;

    float scanstartpos = ctrl->imageCenterPosMM;
    if(!s.nomovemode){
        scanstartpos = (ctrl->imageCenterPosMM - ctrl->imageStartPosMM) * (1.0-s.widthscale) + ctrl->imageStartPosMM;
    }
    statusMsgCb("Moving to start pos...");
    motor.moveToMMBlocking(scanstartpos,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);
    const float moveBackPos = motor.getCurPos();
    if(abortRequested){
        throw JobCancelled();
    }

    statusMsgCb("Capturing preview...");
    motor.enableProgress = false;
    ctrl->captureImageBlocking(previewFile.string(),lines,s.exposureUs,s.nomovemode);
    if(abortRequested){
        // Truncated preview file is not a valid image
        std::error_code ec;
        fs::remove(previewFile,ec);
        motor.enableProgress = true;
        statusMsgCb("Cancelled, moving back...");
        motor.moveToMMBlocking(moveBackPos,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);
        throw JobCancelled();
    }

    runOnGui([this](){ui->pushButton_savepreview->setEnabled(true);});
    auto mover = motor.moveToMM(moveBackPos,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);
    // Show preview
    showImageFile(previewFile);
    statusMsgCb("Preview capture done");
    motor.enableProgress = true;
    mover.waitForFinished();
    statusMsgCb("Done");
}

void MainWindow::imageCaptureBusyCb(bool busy){

}

// Gains from the widgets, offsets from the config. GUI thread only
CCD_GainOfs MainWindow::gainOfsFromUi(){
    CCD_GainOfs ofsGains = ctrl->ccd->getCurGainOfs();
    double scaler = ui->doubleSpinBox_commongain->value();
    ofsGains.bGain = ui->spinBox_bgain->value() * scaler;
    ofsGains.gGain = ui->spinBox_ggain->value() * scaler;
    ofsGains.rGain = ui->spinBox_rgain->value() * scaler;

    json &c = config.config;
    ofsGains.rOfs = c["ccd"]["rofs"];
    ofsGains.gOfs = c["ccd"]["gofs"];
    ofsGains.bOfs = c["ccd"]["bofs"];
    return ofsGains;
}

void MainWindow::restoreSettings(ConfStore& config){
    json &c = config.config;

    ui->spinBox_bgain->setValue(c["ccd"]["bgain"]);
    ui->spinBox_ggain->setValue(c["ccd"]["ggain"]);
    ui->spinBox_rgain->setValue(c["ccd"]["rgain"]);
    ui->comboBox_resolution->setCurrentIndex(c["ccd"]["res"]);
    // Gains are sent to the CCD at the start of each capture

    imagepath.assign(c["imagepath"]);
    imagepathfullres.assign(c["imagepathfullres"]);
    imagepathlowres.assign(c["imagepathlowres"]);
    previewFile = (imagepath / fs::path(c["previewfile"]));
    std::string camtunefile = c["camtunefile"];
    LibCamVidQt::camtunefile = camtunefile.c_str();
    rawTempFile = imagepath/"img.bin";

    ctrl->pnggamma = c["img"]["pnggamma"];
    ui->doubleSpinBox_gamma->setValue(ctrl->pnggamma);

    // Motor
    ctrl->slideMotorQt.motor->homeSG = c["mot"]["slidesg"];
    ctrl->stepperMoveSpeed = c["mot"]["slidespeed"];
    ctrl->stepperMoveAccel = c["mot"]["slideaccel"];

    //ui->horizontalSlider_imagewidth->setValue((float)ui->horizontalSlider_imagewidth->maximum() * (float)c["img"]["widthscale"]);

    ui->gallerywidget->imagepathfullres = imagepathfullres;
    ui->gallerywidget->imagepathlowres = imagepathlowres;
}

void MainWindow::saveSettings(ConfStore& config){
    json &c = config.config;

    c["ccd"]["bgain"] = ui->spinBox_bgain->value();
    c["ccd"]["ggain"] = ui->spinBox_ggain->value();
    c["ccd"]["rgain"] = ui->spinBox_rgain->value();
    c["ccd"]["res"] = ui->comboBox_resolution->currentIndex();
    //c["img"]["widthscale"] = (float)ui->horizontalSlider_imagewidth->value() / (float)ui->horizontalSlider_imagewidth->maximum();
    c["img"]["pnggamma"] = ctrl->pnggamma;
    config.saveSettings(CONFFILE);
}

void MainWindow::progressCb(int progress){
    //qDebug() << "PROGRESS" << progress;
    if(progress != MAXPROGRESS){
        progressbarHideTimer.start(progressbarHideTimer.interval());
        //progressbarHideTimer.stop();
        // if(progressbarHideTimer.isActive())
        //     progressbarHideTimer.stop();
            //QMetaObject::invokeMethod(&progressbarHideTimer,"stop",Qt::QueuedConnection);
        ui->progressBar_scan->setVisible(true);
    }else{
        //QMetaObject::invokeMethod(&progressbarHideTimer,"start",Qt::QueuedConnection);
        //progressbarHideTimer.start();
    }
    ui->progressBar_scan->setValue(progress);
}

void MainWindow::setReady(bool ready){
    cameraReady = ready;
    updateControlsEnabled();
    if(ready){
        ui->label_boot1->setText("Camera ready");
        ui->radioButton_linearscan->click();
    }
    // ui->pushButton_shutter->setEnabled(true);
}

void MainWindow::on_radioButton_mode_debug_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->page_debug);
}


void MainWindow::on_radioButton_mode_preview_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->page_preview);
    //ui->progressBar_scan->setVisible(false);
    runMotorOperation("Moving sensor",[this](){
        return ctrl->slideMotorQt.moveToMM(ctrl->previewCamCenterMM,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);
    },"Live preview mode");
}

void MainWindow::changeMovementSize(double v){
    this->curMovementSize = v;
    ui->doubleSpinBox_debug_dist->blockSignals(true);
    ui->doubleSpinBox_preview_dist->blockSignals(true);
    ui->doubleSpinBox_debug_dist->setValue(v);
    ui->doubleSpinBox_preview_dist->setValue(v);
    ui->doubleSpinBox_debug_dist->blockSignals(false);
    ui->doubleSpinBox_preview_dist->blockSignals(false);
}

// Debug functions:
void MainWindow::home()
{
    runMotorOperation("Homing",[this](){return ctrl->slideMotorQt.home();},"Homed");

}

void MainWindow::updateGallerySelection(QListWidgetItem* item){
    ui->groupBox_galleryActions->setEnabled(item != nullptr);
    if(!item){
        ui->label_gallery_info->setText("");
        return;
    }
    std::string filename = item->data(Qt::UserRole).value<QString>().toStdString();
    fs::path fullresimgpath = imagepathfullres/filename;
    FILE* f = fopen(fullresimgpath.string().c_str(),"rb");
    png::PngImageInfo imginfo = png::getPngInfo(f);
    fclose(f);

    QString infotext = QString("Info:\n\n%1\nx\n%2\n\n%3b\n\n%4MB").arg(imginfo.ihdr.height).arg(imginfo.ihdr.width).arg(imginfo.ihdr.bit_depth).arg(std::round(((double)fs::file_size(fullresimgpath)/1000000.0)));

    ui->label_gallery_info->setText(infotext);

}


void MainWindow::moveLeft()
{
    runMotorOperation("Moving",[this](){return ctrl->slideMotorQt.moveMM(-curMovementSize,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);},"Ready");
}


void MainWindow::moveRight()
{
    runMotorOperation("Moving",[this](){return ctrl->slideMotorQt.moveMM(curMovementSize,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);},"Ready");
}


void MainWindow::on_pushButton_quit_clicked()
{
    saveSettings(config);
    qApp->exit();
}


void MainWindow::on_radioButton_linearscan_clicked()
{
    ui->horizontalSlider_imagewidth2->setVisible(true);
    ui->stackedWidget->setCurrentWidget(ui->page_graphic);
    runMotorOperation("Moving sensor",[this](){
        return ctrl->slideMotorQt.moveToMM(ctrl->imageStartPosMM,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);
    },"Ready");
    ui->groupBox_camactions->setVisible(true);
}


void MainWindow::updateResEstimate(){
    CCD_Parameters nextparams = ctrl->ccd->getParamsFromResolution((CCD_Resolution)ui->comboBox_resolution->currentIndex());
    int height = nextparams.validPixelEnd - nextparams.validPixelBegin;
    int width = ((ui->horizontalSlider_imagewidth->value() / 1000.0f) * ctrl->fullResLines) / nextparams.moveLines;
    if(ui->radioButton_linecam->isChecked())
        width *= nomovemult;
    ui->label_resolution->setText(QString("%1 x %2").arg(width).arg(height));
}


void MainWindow::on_horizontalSlider_imagewidth_valueChanged(int value)
{
    updateResEstimate();
}



void MainWindow::on_radioButton_gallery_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->page_gallery);
    // Update free memory
    fs::space_info spaceinfo = fs::space(imagepathfullres);
    QString freespacetext = QString("Free:\n%1MB\nTotal:\n%2MB").arg(spaceinfo.free / 1000000).arg(spaceinfo.capacity / 1000000);
    ui->label_gallery_freespace->setText(freespacetext);
    ui->progressBar_freespace->setMaximum(spaceinfo.capacity/1000000);
    ui->progressBar_freespace->setMinimum(0);
    ui->progressBar_freespace->setValue((spaceinfo.capacity/ 1000000) - (spaceinfo.free / 1000000));
}


void MainWindow::on_radioButton_linecam_clicked()
{
    ui->horizontalSlider_imagewidth2->setVisible(false);
    ui->stackedWidget->setCurrentWidget(ui->page_graphic);
    runMotorOperation("Moving sensor",[this](){
        return ctrl->slideMotorQt.moveToMM(ctrl->imageCenterPosMM,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);
    },"Ready");
    ui->groupBox_camactions->setVisible(true);
}

