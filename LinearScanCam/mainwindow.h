#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QPromise>
#include "cameracontrolqt.h"
#include "pcfioqt.h"
#include "confstore.h"
#include "galleryqt.h"

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
    void saveSettings(ConfStore& config);
    void applyCcdParams();
    void updateResEstimate();

    // Runs f on the GUI thread. Direct call if already on it, queued otherwise
    template<typename F>
    void runOnGui(F&& f){
        QMetaObject::invokeMethod(this,std::forward<F>(f),Qt::AutoConnection);
    }

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

    void statusMsgCb(QString status);

    void updateGallerySelection(QListWidgetItem* item);

private slots:
    void on_radioButton_mode_debug_clicked();

    void on_radioButton_mode_preview_clicked();

    void on_pushButton_quit_clicked();

    void on_radioButton_linearscan_clicked();

    void on_horizontalSlider_imagewidth_valueChanged(int value);

    void on_radioButton_gallery_clicked();

    void on_radioButton_linecam_clicked();

private:
    Ui::MainWindow *ui;

    //QThread *camerathread;
    CameraControllerQt *ctrl;

    QTimer progressbarHideTimer;

    float curMovementSize = 5;

    uint8_t pngCompression = 1;

    float moveBackPos = 0; // Store position before capturing for use in lambdas


    fs::path imagepath = "./";
    fs::path imagepathfullres = "./img";
    fs::path imagepathlowres = "./preview";
    fs::path previewFile = "preview.png";
    fs::path rawTempFile = "img.bin";
    static QThreadPool pool;

    const float nomovemult = 2.0f;

    static constexpr int baseDownsamplePreview = CCD_Ctrl::maxLineMovementSkip+1;
};
#endif // MAINWINDOW_H
