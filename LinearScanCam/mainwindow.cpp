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
    ui->frame_mode->setEnabled(false);
    ui->groupBox_camactions->setEnabled(false);

    qDebug() << "Startpos:" << ctrl->imageStartPosMM << "End:" << ctrl->imageEndPosMM << "Center:" << ctrl->imageCenterPosMM;

    // Setup progressbar
    progressbarHideTimer.moveToThread(this->thread());
    progressbarHideTimer.setSingleShot(true);
    progressbarHideTimer.callOnTimeout([this]{this->ui->progressBar_scan->setVisible(false);});
    progressbarHideTimer.setInterval(250);
    connect(ctrl, &CameraControllerQt::progress,this,&MainWindow::progressCb,Qt::QueuedConnection);

    // Disable debug controls when motor running
    connect(&ctrl->slideMotorQt, &MotorControlQt::idle,ui->groupBox_motorcontrol_debug,&QGroupBox::setEnabled);
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
    connect(ui->pushButton_preview_center,&QPushButton::clicked,this,[this](){ctrl->slideMotorQt.moveToMM(ctrl->previewCamCenterMM,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);});
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

void MainWindow::statusMsgCb(QString status){
    ui->label_status->setText(status);
}

void MainWindow::busyCb(bool busy){
    //ui->centralwidget->setEnabled(!busy);
    bool enabled = !busy;
    // Disable controls when busy
    ui->groupBox_camactions->setEnabled(enabled);
    ui->frame_mode->setEnabled(enabled);
}
void MainWindow::savePreview(){
    qDebug() << "Save preview";
    ui->pushButton_savepreview->setEnabled(false);
    std::filesystem::path newfile = ctrl->getUnusedImageName(fs::path(imagepathfullres/"img.png"));
    fs::path lowresfile = imagepathlowres/(newfile.filename());
    fs::rename(previewFile, newfile); // Move file
    auto future = ctrl->reencodePngFuture(newfile,lowresfile,baseDownsamplePreview-ctrl->ccd->getCurParams().moveLines,pngCompression,SPNG_FMT_RGB8,true);
//ctrl->moveTempImage(previewFile,imagepathfullres);

    auto done = [this](fs::path newpath){
        QString infotext;
        qDebug() << "Saving preview";
        if(newpath.has_filename()){
            infotext = QString("Saved image to: ") + QString(newpath.filename().c_str());

        }else{
            infotext = "Cannot save file...";
        }
        this->ui->label_status->setText(infotext);
        //QToolTip::showText(ui->pushButton_savepreview->pos(),infotext, ui->pushButton_savepreview,{},1000);
    };
    future.then(done);

}

void MainWindow::captureImage(){
    qDebug() << "SHUTTER";

    bool nomovemode = ui->radioButton_linecam->isChecked();
    int exposuretime = ui->spinBox_exposuretime->value();

    if(ui->stackedWidget->currentWidget() != ui->page_graphic){
        ui->radioButton_linearscan->click();
        return;
        // ui->stackedWidget->setCurrentWidget(ui->page_graphic);
        // ctrl->slideMotorQt.moveToMM(ctrl->imageStartPosMM,500,5,&pool);
        // ui->groupBox_camactions->setVisible(true);
    }
    statusMsgCb("Starting image capture...");
    ctrl->ccd->setResolution((CCD_Resolution)ui->comboBox_resolution->currentIndex());
    applyCcdParams();
    statusMsgCb("Moving to start pos...");
    float widthscale = (float)ui->horizontalSlider_imagewidth->value() / (float)ui->horizontalSlider_imagewidth->maximum();
    if(nomovemode) widthscale *= nomovemult;

    fs::path newimagename = ctrl->getUnusedImageName(imagepathfullres/"img.png");
    moveBackPos = ctrl->slideMotorQt.getCurPos();
    fs::path recordimagename = rawTempFile;

    auto updateposfunc = [this](){
        moveBackPos = ctrl->slideMotorQt.getCurPos();
    };
    auto capturefunc = [this,widthscale,recordimagename,nomovemode,exposuretime](){
        statusMsgCb("Capturing image...");
        ctrl->slideMotorQt.enableProgress = false;
        ctrl->captureImageFuture(recordimagename,(ctrl->fullResLines / ctrl->ccd->getCurParams().moveLines)*widthscale,exposuretime,&pool,nomovemode).waitForFinished();
    };
    auto finishedfunc = [this,newimagename,recordimagename,widthscale](){
            statusMsgCb("Processing image...");
            auto params = ctrl->ccd->getCurParams();
            auto mover = ctrl->slideMotorQt.moveToMM(moveBackPos,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel); // Move back
            auto pngfuture = ctrl->reencodeRawToPngFuture(recordimagename,newimagename,(ctrl->fullResLines / params.moveLines)*widthscale,params.validPixelEnd - params.validPixelBegin,3);

            pngfuture.then([this,newimagename,recordimagename](fs::path img){
                fs::path lowresfile = imagepathlowres/(newimagename.filename());
                // delete temp file
                fs::remove(recordimagename);
                statusMsgCb("Processing preview...");
                auto pngfuturepreview = ctrl->reencodePngFuture(img,lowresfile,baseDownsamplePreview-ctrl->ccd->getCurParams().moveLines,pngCompression,SPNG_FMT_RGB8,true,&pool);
                pngfuturepreview.waitForFinished();
                statusMsgCb("Preview Done");
                QImage previewImg(lowresfile.c_str());
                QMetaObject::invokeMethod(ui->graphicsView,"showFullImage" ,Qt::QueuedConnection,Q_ARG(QImage,previewImg));
            });

            pngfuture.waitForFinished();
            ctrl->slideMotorQt.enableProgress = true;
            statusMsgCb("Moving back...");
            mover.waitForFinished();
            statusMsgCb("Done");
    };
    // Do the stuff
    float scanstartpos = ctrl->imageCenterPosMM;
    if(!nomovemode){
        scanstartpos = (ctrl->imageCenterPosMM - ctrl->imageStartPosMM) * (1.0-widthscale) + ctrl->imageStartPosMM;
    }
    ctrl->slideMotorQt.moveToMM(scanstartpos,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel,&pool)
    .then([this](){QThread::sleep(ui->spinBox_delay->value());})
    .then(updateposfunc)
    // Capture the image
    .then(capturefunc)
    // Move back and show image
    .then(finishedfunc);
    ctrl->slideMotorQt.enableProgress = true;
}

void MainWindow::capturePreviewImage(){
    //static QThreadPool pool;

    qDebug() << "PREVIEW";
    bool nomovemode = ui->radioButton_linecam->isChecked();
    int exposuretime = ui->spinBox_exposuretime->value();

    if(ui->stackedWidget->currentWidget() != ui->page_graphic){
        statusMsgCb("Preview mode");
        ui->radioButton_linearscan->click();
        return;
    }


    statusMsgCb("Starting preview capture...");
    ui->pushButton_savepreview->setEnabled(false);
    // Set preview parameters

    float widthscale = (float)ui->horizontalSlider_imagewidth->value() / (float)ui->horizontalSlider_imagewidth->maximum();
    if(nomovemode) widthscale *= nomovemult;
    ctrl->ccd->setResolution(CCD_Resolution::Preview); // Lowest resolution
    applyCcdParams(); // Set gains
    CCD_Parameters params =  ctrl->ccd->getCurParams();

    uint32_t lines = (ctrl->fullResLines / params.moveLines)*widthscale;
    qDebug() << "scale" << widthscale << "lines" << lines;
    statusMsgCb("Moving to start pos...");
    moveBackPos = ctrl->slideMotorQt.getCurPos();

    auto updateposfunc = [this](){
        moveBackPos = ctrl->slideMotorQt.getCurPos();
        //qDebug() << "Curpos" << moveBackPos;
    };

    auto captureimagefunc = [this,lines,nomovemode,exposuretime](){
        statusMsgCb("Capturing preview...");
        qDebug() << "Capture image" << lines;
        ctrl->slideMotorQt.enableProgress = false;
        ctrl->captureImageFuture(previewFile,lines,exposuretime,&pool,nomovemode).waitForFinished();
    };


    auto previewDone = [this](){
        //qDebug() << "Move back" << moveBackPos;
        ui->pushButton_savepreview->setEnabled(true);
        auto movefut = ctrl->slideMotorQt.moveToMM(moveBackPos,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);
        // Show preview
        //ui->progressBar_scan->setVisible(false); // Hide progress bar quickly
        qDebug() << "Show image";
        QImage previewImg(previewFile.c_str());
        //QMetaObject::invokeMethod(ui->graphicsView,"showFullImage",previewImg,Qt::QueuedConnection);
        QMetaObject::invokeMethod(ui->graphicsView,"showFullImage" ,Qt::QueuedConnection,Q_ARG(QImage,previewImg));
        //ui->graphicsView->showFullImage(previewImg);
        statusMsgCb("Preview capture done");
        ctrl->slideMotorQt.enableProgress = true;
        movefut.waitForFinished();
        statusMsgCb("Done");

    };
    // Do the stuff
    float scanstartpos = ctrl->imageCenterPosMM;
    if(!nomovemode){
        scanstartpos = (ctrl->imageCenterPosMM - ctrl->imageStartPosMM) * (1.0-widthscale) + ctrl->imageStartPosMM;
    }
    ctrl->slideMotorQt.moveToMM(scanstartpos,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel,&pool)
    .then(updateposfunc)
    .then(captureimagefunc)
    .then(previewDone);
    ctrl->slideMotorQt.enableProgress = true;
}

void MainWindow::imageCaptureBusyCb(bool busy){

}

void MainWindow::applyCcdParams(){
    CCD_GainOfs ofsGains = ctrl->ccd->getCurGainOfs();
    double scaler = ui->doubleSpinBox_commongain->value();
    ofsGains.bGain = ui->spinBox_bgain->value() * scaler;
    ofsGains.gGain = ui->spinBox_ggain->value() * scaler;
    ofsGains.rGain = ui->spinBox_rgain->value() * scaler;

    json &c = config.config;
    ofsGains.rOfs = c["ccd"]["rofs"];
    ofsGains.gOfs = c["ccd"]["gofs"];
    ofsGains.bOfs = c["ccd"]["bofs"];
    ctrl->ccd->setRgbGainOfs(ofsGains);
    //ctrl->ccd->setResolution
}

void MainWindow::restoreSettings(ConfStore& config){
    json &c = config.config;

    ui->spinBox_bgain->setValue(c["ccd"]["bgain"]);
    ui->spinBox_ggain->setValue(c["ccd"]["ggain"]);
    ui->spinBox_rgain->setValue(c["ccd"]["rgain"]);
    ui->comboBox_resolution->setCurrentIndex(c["ccd"]["res"]);
    applyCcdParams();

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
    ui->frame_mode->setEnabled(ready);
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
    statusMsgCb("Moving sensor...");
    ui->stackedWidget->setCurrentWidget(ui->page_preview);
    //ui->progressBar_scan->setVisible(false);
    auto motfut = ctrl->slideMotorQt.moveToMM(ctrl->previewCamCenterMM,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);
    motfut.then([this](){statusMsgCb("Live preview mode");});
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
    ctrl->slideMotorQt.home();

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
    ctrl->slideMotorQt.moveMM(-curMovementSize,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);
}


void MainWindow::moveRight()
{
    ctrl->slideMotorQt.moveMM(curMovementSize,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);
}


void MainWindow::on_pushButton_quit_clicked()
{
    saveSettings(config);
    qApp->exit();
}


void MainWindow::on_radioButton_linearscan_clicked()
{
    ui->horizontalSlider_imagewidth2->setVisible(true);
    statusMsgCb("Moving sensor...");
    ui->stackedWidget->setCurrentWidget(ui->page_graphic);
    auto motfut = ctrl->slideMotorQt.moveToMM(ctrl->imageStartPosMM,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);
    motfut.then([this](){statusMsgCb("Ready");});
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
    statusMsgCb("Moving sensor...");
    ui->horizontalSlider_imagewidth2->setVisible(false);
    ui->stackedWidget->setCurrentWidget(ui->page_graphic);
    auto motfut = ctrl->slideMotorQt.moveToMM(ctrl->imageCenterPosMM,ctrl->stepperMoveSpeed,ctrl->stepperMoveAccel);
    motfut.then([this](){statusMsgCb("Ready");});
    ui->groupBox_camactions->setVisible(true);
}

