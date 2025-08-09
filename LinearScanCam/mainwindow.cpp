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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , ctrl(new CameraControllerQt(this,new LinCCDQt,new PcfIoQt))
    , config(CONFFILE)
{
    ui->setupUi(this);
    ui->progressBar_scan->setVisible(false);
    ui->stackedWidget->setCurrentWidget(ui->page_boot);
    connect(ctrl, &CameraControllerQt::ready,this,&MainWindow::setReady);
    ui->frame_mode->setEnabled(false);
    ui->groupBox_camactions->setEnabled(false);

    // Setup progressbar
    progressbarHideTimer.setSingleShot(true);
    progressbarHideTimer.callOnTimeout([this]{this->ui->progressBar_scan->setVisible(false);});
    progressbarHideTimer.setInterval(1000);
    connect(ctrl, &CameraControllerQt::progress,this,&MainWindow::progressCb);

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
    // Dist
    ui->doubleSpinBox_debug_dist->setValue(this->curMovementSize);
    ui->doubleSpinBox_preview_dist->setValue(this->curMovementSize);
    connect(ui->doubleSpinBox_debug_dist,&QDoubleSpinBox::valueChanged,this,&MainWindow::changeMovementSize);
    //connect(ui->doubleSpinBox_preview_dist,&QDoubleSpinBox::valueChanged,this,&MainWindow::changeMovementSize); // Done by UI signal connection
    // Debug info
    connect(ctrl,&CameraControllerQt::ccdStatus,this,[this](QString t){ui->label_ccdstatus_debug->setText("CCD Status: " + t);});
    connect(ctrl,&CameraControllerQt::motorStatus,this,[this](QString t){ui->label_debug_motorstatus->setText("Motor Status: " + t);});
    connect(ctrl,&CameraControllerQt::ccdStatus,this,[this](QString t){ui->label_ccd_status_startup->setText("CCD Status: " + t);});
    connect(ctrl,&CameraControllerQt::motorStatus,this,[this](QString t){ui->label_motor_status_startup->setText("Motor Status: " + t);});

    // Buttons
    connect(ctrl->pcfio,&PcfIoQt::buttonPressed,this,&MainWindow::buttonExtCb);

    // Camera controls
    connect(ui->pushButton_shutter,&QPushButton::clicked,this,&MainWindow::captureImage);
    connect(ui->pushButton_preview,&QPushButton::clicked,this,&MainWindow::capturePreviewImage);
    connect(ui->pushButton_savepreview,&QPushButton::clicked,this,&MainWindow::savePreview);


    restoreSettings(config);

    // Wait for init
    QFuture<bool> future = QtConcurrent::run(&CameraControllerQt::init,ctrl);

    connect(&ctrl->slideMotorQt, &MotorControlQt::busy,this,&MainWindow::busyCb);

    ui->graphicsView->setScene(new QGraphicsScene);
}

MainWindow::~MainWindow()
{
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
}

void MainWindow::captureImage(){
    qDebug() << "SHUTTER";
    if(ui->stackedWidget->currentWidget() != ui->page_graphic){
        ui->radioButton_linearscan->click();
    }
    ctrl->ccd->setResolution((CCD_Resolution)ui->comboBox_resolution->currentIndex());
}

void MainWindow::capturePreviewImage(){
    qDebug() << "PREVIEW";
    if(ui->stackedWidget->currentWidget() != ui->page_graphic){
        ui->radioButton_linearscan->click();
    }
    ui->pushButton_savepreview->setEnabled(false);
    // Set preview parameters

    float widthscale = (float)ui->horizontalSlider_imagewidth->value() / (float)ui->horizontalSlider_imagewidth->maximum();
    ctrl->ccd->setResolution(CCD_Resolution::Preview); // Lowest resolution

    ctrl->slideMotorQt.moveToMM(ctrl->imageCenterPosMM * (1.0-widthscale),500,5).waitForFinished(); // TODO run outside main thread
    float curpos = ctrl->slideMotorQt.getCurPos();

    auto future = ctrl->captureImageFuture(previewFile,(ctrl->fullResLines / ctrl->ccd->getCurParams().moveLines)*widthscale);
    //future.waitForFinished();
    auto previewDone = [&](){
        ui->pushButton_savepreview->setEnabled(true);
        ctrl->slideMotorQt.moveToMM(curpos,500,5);
        // Show preview
        ui->graphicsView->scene()->clear();
        QImage previewImg(previewFile.c_str());

        QColorSpace customColor(QColorSpace::Primaries::SRgb,0.4);
        previewImg = previewImg.convertedToColorSpace(customColor);
        ui->graphicsView->scene()->addPixmap(QPixmap::fromImage(previewImg));
    };
    connect(ctrl,&CameraControllerQt::imageCaptureDone,this,previewDone,Qt::SingleShotConnection);

}

void MainWindow::imageCaptureBusyCb(bool busy){

}

void MainWindow::applyCcdParams(){
    CCD_GainOfs ofsGains = ctrl->ccd->getCurGainOfs();
    ofsGains.bGain = ui->spinBox_bgain->value();
    ofsGains.gGain = ui->spinBox_ggain->value();
    ofsGains.rGain = ui->spinBox_rgain->value();
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

    imagepath = c["imagepath"];
    previewFile = imagepath + std::string(c["previewfile"]);
    std::string camtunefile = c["camtunefile"];
    LibCamVidQt::camtunefile = camtunefile.c_str();

}

void MainWindow::progressCb(int progress){
    //qDebug() << "PROGRESS" << progress;
    if(progress != MAXPROGRESS){
        progressbarHideTimer.stop();
        ui->progressBar_scan->setVisible(true);
    }else{
        progressbarHideTimer.start();
    }
    ui->progressBar_scan->setValue(progress);
}

void MainWindow::setReady(bool ready){
    ui->frame_mode->setEnabled(ready);
    if(ready){
        ui->label_boot1->setText("Camera ready");
        ui->radioButton_mode_debug->click();
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
    ctrl->slideMotorQt.moveToMM(25,500,5);
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


void MainWindow::moveLeft()
{
    ctrl->slideMotorQt.moveMM(-curMovementSize,500,5);
}


void MainWindow::moveRight()
{
    ctrl->slideMotorQt.moveMM(curMovementSize,500,5);
}


void MainWindow::on_pushButton_quit_clicked()
{
    qApp->exit();
}


void MainWindow::on_radioButton_linearscan_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->page_graphic);
    ctrl->slideMotorQt.moveToMM(ctrl->imageStartPosMM,500,5);
    ui->groupBox_camactions->setVisible(true);


}


void MainWindow::updateResEstimate(){
    CCD_Parameters nextparams = ctrl->ccd->getParamsFromResolution((CCD_Resolution)ui->comboBox_resolution->currentIndex());
    int height = nextparams.validPixelEnd - nextparams.validPixelBegin;
    int width = ((ui->horizontalSlider_imagewidth->value() / 1000.0f) * ctrl->fullResLines) / nextparams.moveLines;
    ui->label_resolution->setText(QString("%1 x %2").arg(width).arg(height));
}


void MainWindow::on_horizontalSlider_imagewidth_valueChanged(int value)
{
    updateResEstimate();
}

