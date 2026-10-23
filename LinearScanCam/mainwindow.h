#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QPromise>
#include <functional>
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
    // Values read from the UI on the GUI thread before a job starts
    struct CaptureSettings{
        CCD_Resolution res = CCD_Resolution::Preview;
        CCD_GainOfs gainOfs;
        bool nomovemode = false;
        int exposureUs = 0;
        int delayS = 0;
        float widthscale = 1;
    };

    CCD_GainOfs gainOfsFromUi();
    CaptureSettings readCaptureSettings(CCD_Resolution res);
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
    bool tryBeginOperation(const QString& name);
    void endOperation();
    void updateControlsEnabled();
    void runMotorOperation(const QString& name,std::function<QFuture<void>()> start,const QString& doneMsg = QString());

    void runJob(std::function<void()> job);
    void captureImageJob(CaptureSettings s,fs::path newimagename,fs::path recordimagename,int pngCompressionOvr);
    void capturePreviewJob(CaptureSettings s);
    void showImageFile(fs::path file);

    Ui::MainWindow *ui;

    //QThread *camerathread;
    CameraControllerQt *ctrl;

    QTimer progressbarHideTimer;

    // Operation guard state. GUI thread only
    bool cameraReady = false;
    bool motorBusy = false;
    bool operationActive = false;
    QString activeOperation;

    float curMovementSize = 5;

    uint8_t pngCompression = 1;


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
