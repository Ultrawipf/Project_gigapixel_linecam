#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QPromise>
#include "cameracontrolqt.h"
#include "pcfioqt.h"
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
    void applyCcdParams();
    void updateResEstimate();

public slots:
    void setReady(bool ready);
    void progressCb(int progress);

    void moveLeft();
    void moveRight();
    void home();
    void changeMovementSize(double v);

    void captureImage();
    void capturePreviewImage();

    void busyCb(bool busy);
    void imageCaptureBusyCb(bool busy);
    void savePreview();

    void buttonExtCb(uint8_t buttons);

private slots:
    void on_radioButton_mode_debug_clicked();

    void on_radioButton_mode_preview_clicked();

    void on_pushButton_quit_clicked();

    void on_radioButton_linearscan_clicked();

    void on_horizontalSlider_imagewidth_valueChanged(int value);

private:
    Ui::MainWindow *ui;

    //QThread *camerathread;
    CameraControllerQt *ctrl;

    QTimer progressbarHideTimer;

    float curMovementSize = 5;


    std::string imagepath = "./";
    std::string previewFile = "preview.png";
};
#endif // MAINWINDOW_H
