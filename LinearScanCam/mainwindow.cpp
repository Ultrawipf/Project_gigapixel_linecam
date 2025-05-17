#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "../linearcam_pi/motors.h"

Motors motors;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->progressBar_scan->setVisible(false);
    ui->stackedWidget->setCurrentIndex(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_radioButton_mode_debug_clicked()
{
    ui->stackedWidget->setCurrentIndex(1); // Debug page
}


void MainWindow::on_radioButton_mode_preview_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}

// Debug functions:
void MainWindow::on_pushButton_debug_home_clicked()
{
    motors.homeSlide();
}


void MainWindow::on_pushButton_debug_left_clicked()
{
    motors.slideMotor.move(-100,1000,10);
}


void MainWindow::on_pushButton_debug_right_clicked()
{
    motors.slideMotor.move(100,1000,10);
}

