#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QPromise>
#include "cameracontrolqt.h"

#include "confstore.h"

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

    ConfStore config;

    void restoreSettings(ConfStore& config);

public slots:
    void setReady(bool ready);
    void progressCb(int progress);

    void moveLeft();
    void moveRight();
    void home();
    void changeMovementSize(double v);

private slots:
    void on_radioButton_mode_debug_clicked();

    void on_radioButton_mode_preview_clicked();

    void on_pushButton_quit_clicked();

private:
    Ui::MainWindow *ui;

    //QThread *camerathread;
    CameraControllerQt *ctrl;

    QTimer progressbarHideTimer;

    float curMovementSize = 5;

};
#endif // MAINWINDOW_H
