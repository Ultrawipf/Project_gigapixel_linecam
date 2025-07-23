#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include "../linearcam_pi/cameracontrol.h"
#include <QFuture>
#include <QFutureWatcher>

#include "cameracontrolqt.h"
#include "confstore.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , ctrl(new CameraControllerQt(this))
    , config(CONFFILE)
{
    ui->setupUi(this);
    ui->progressBar_scan->setVisible(false);
    ui->stackedWidget->setCurrentWidget(ui->page_boot);
    connect(ctrl, &CameraControllerQt::ready,this,&MainWindow::setReady);
    ui->frame_mode->setEnabled(false);

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
    connect(ctrl,&CameraControllerQt::ccdStatus,this,[=](QString t){ui->label_ccdstatus_debug->setText("CCD Status: " + t);});
    connect(ctrl,&CameraControllerQt::motorStatus,this,[=](QString t){ui->label_debug_motorstatus->setText("Motor Status: " + t);});
    connect(ctrl,&CameraControllerQt::ccdStatus,this,[=](QString t){ui->label_ccd_status_startup->setText("CCD Status: " + t);});
    connect(ctrl,&CameraControllerQt::motorStatus,this,[=](QString t){ui->label_motor_status_startup->setText("Motor Status: " + t);});


    restoreSettings(config);

    // Wait for init
    QFuture<bool> future = QtConcurrent::run(&CameraControllerQt::init,ctrl);
}

MainWindow::~MainWindow()
{
    std::cout << "Deleting mainwindow "<< std::endl;
    delete ui;
    delete ctrl;
    //delete camerathread;
}


void MainWindow::restoreSettings(ConfStore& config){
    json &c = config.config;

    ui->spinBox_bgain->setValue(c["ccd"]["bgain"]);
    ui->spinBox_ggain->setValue(c["ccd"]["ggain"]);
    ui->spinBox_rgain->setValue(c["ccd"]["rgain"]);
    ui->comboBox_resolution->setCurrentIndex(c["ccd"]["res"]);
}

void MainWindow::progressCb(int progress){

    if(progress != 1000){
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

