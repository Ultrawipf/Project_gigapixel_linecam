#ifndef LIBCAMVIDQT_H
#define LIBCAMVIDQT_H

#include <QWidget>
#include <QLabel>
#include <QProcess>


class LibCamVidQt : public QLabel

{
    Q_OBJECT
public:
    LibCamVidQt(QWidget * parent);
    LibCamVidQt();
    virtual ~LibCamVidQt();

    void start();
    void stop();

    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent  *event) override;
    void resizeEvent(QResizeEvent  *event) override;
    QPixmap getCurPixmap();

    static QString camtunefile;

public slots:
    void processFrame();
    void errorHandler(QProcess::ProcessError error);
    //bool saveFrame();

protected:
    QProcess libcameraProc;
    // Args for IMX219 camera
    const QStringList CAMERAARGS = {"-t","0" ,"-n","-v","0", "--inline","--framerate","30","--width","1640","--height","1232","--codec","mjpeg","-o","-"};
    const QString CAMERACMD = "/usr/bin/rpicam-vid";
    QByteArray streamBuf;
    const QString IDLETEXT = "Waiting for camera...";
    bool running = false;
    QPixmap curPixmap;
};

#endif // LIBCAMVIDQT_H
