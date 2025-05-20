#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include "../linearcam_pi/cameracontrol.h"
#include <QFuture>
#include <QFutureWatcher>

#include "cameracontrolqt.h"



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , ctrl(new CameraControllerQt(this))
{
    ui->setupUi(this);
    ui->progressBar_scan->setVisible(false);
    ui->stackedWidget->setCurrentWidget(ui->page_boot);
    connect(ctrl, &CameraControllerQt::ready,this,&MainWindow::setReady);
    ui->groupBox_mode->setEnabled(false);

    // Setup progressbar
    progressbarHideTimer.setSingleShot(true);
    progressbarHideTimer.callOnTimeout([this]{this->ui->progressBar_scan->setVisible(false);});
    progressbarHideTimer.setInterval(1000);
    connect(ctrl, &CameraControllerQt::progress,this,&MainWindow::progressCb);

    // Disable debug controls when motor running
    connect(&ctrl->slideMotorQt, &MotorControlQt::idle,ui->groupBox_motorcontrol_debug,&QGroupBox::setEnabled);


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
    ui->groupBox_mode->setEnabled(ready);
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
}


// Debug functions:
void MainWindow::on_pushButton_debug_home_clicked()
{
    ctrl->slideMotorQt.home();

}


void MainWindow::on_pushButton_debug_left_clicked()
{
    ctrl->slideMotorQt.move(-ui->spinBox_debug_steps->value(),2000,10);
}


void MainWindow::on_pushButton_debug_right_clicked()
{
    ctrl->slideMotorQt.move(ui->spinBox_debug_steps->value(),2000,10);
}


void MainWindow::on_pushButton_quit_clicked()
{
    qApp->exit();
}

