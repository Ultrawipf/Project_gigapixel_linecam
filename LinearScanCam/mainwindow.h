#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QPromise>
#include "cameracontrolqt.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void setReady(bool ready);

private slots:
    void on_radioButton_mode_debug_clicked();

    void on_radioButton_mode_preview_clicked();

    void on_pushButton_debug_home_clicked();

    void on_pushButton_debug_left_clicked();

    void on_pushButton_debug_right_clicked();

    void on_pushButton_quit_clicked();

private:
    Ui::MainWindow *ui;

    //QThread *camerathread;
    CameraControllerQt *ctrl;


};
#endif // MAINWINDOW_H
