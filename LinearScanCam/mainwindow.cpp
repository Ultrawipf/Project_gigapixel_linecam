#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include "../linearcam_pi/cameracontrol.h"
#include <QFuture>
#include <QFutureWatcher>

#include "cameracontrolqt.h"
#include "confstore.h"

#include <QColorSpace>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , ctrl(new CameraControllerQt(this,new LinCCDQt))
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

    // Camera controls
    connect(ui->pushButton_shutter,&QPushButton::clicked,this,&MainWindow::captureImage);
    connect(ui->pushButton_preview,&QPushButton::clicked,this,&MainWindow::capturePreviewImage);


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

void MainWindow::busyCb(bool busy){
    ui->centralwidget->setEnabled(!busy);
}


void MainWindow::captureImage(){
    qDebug() << "SHUTTER";
    ctrl->ccd->setResolution((CCD_Resolution)ui->comboBox_resolution->currentIndex());
}

void MainWindow::capturePreviewImage(){
    qDebug() << "PREVIEW";
    if(ui->stackedWidget->currentWidget() != ui->page_graphic){
        on_radioButton_linearscan_clicked();
    }
    // Set preview parameters
    float curpos = ctrl->slideMotorQt.getCurPos();
    ctrl->ccd->setResolution(CCD_Resolution::Div16); // Lowest resolution
    auto future = ctrl->captureImageFuture(previewFile,ctrl->fullResLines / ctrl->ccd->getCurParams().moveLines);
    //future.waitForFinished();
    auto previewDone = [&](){

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
    if(progress != 1.0){
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
    ui->progressBar_scan->setVisible(false);
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
    ui->groupBox_camactions->setEnabled(true);


}


