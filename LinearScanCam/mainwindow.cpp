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
    ui->progressBar_scan->setVisible(true);
    ui->stackedWidget->setCurrentWidget(ui->page_boot);
    connect(ctrl, &CameraControllerQt::ready,this,&MainWindow::setReady);


    ui->groupBox_mode->setEnabled(false);

    //QThread *camerathread = new QThread;
    //camerathread->start();
    //slideMotorQt.moveToThread(camerathread);

    //std::cout << "Mainthread: " << QThread::currentThread() << std::endl;
/*
    connect(&slideMotorQt, &MotorControlQt::progress, [](int progress){
        std::cout << "current progress:" << progress << std::endl;
    });
*/
    // Disable debug controls when motor running
    connect(&ctrl->slideMotorQt, &MotorControlQt::idle,ui->groupBox_motorcontrol_debug,&QGroupBox::setEnabled);
    /*
    connect(&slideMotorQt, &MotorControlQt::idle,ui->groupBox_motorcontrol_debug,&QGroupBox::setEnabled);

    connect(&slideMotorQt, &MotorControlQt::busy,ui->progressBar_scan,&QProgressBar::setVisible);
    connect(&slideMotorQt, &MotorControlQt::progress,ui->progressBar_scan,&QProgressBar::setValue);
*/
    connect(ctrl, &CameraControllerQt::progress,ui->progressBar_scan,&QProgressBar::setValue);

    //ctrl->init();
    QFuture<bool> future = QtConcurrent::run(&CameraControllerQt::init,ctrl);
}

MainWindow::~MainWindow()
{
    std::cout << "Deleting mainwindow "<< std::endl;
    delete ui;
    delete ctrl;
    //delete camerathread;
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

