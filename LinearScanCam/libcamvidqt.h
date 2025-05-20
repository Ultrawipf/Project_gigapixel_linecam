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

public slots:
    void processFrame();
    void errorHandler(QProcess::ProcessError error);

protected:
    QProcess libcameraProc;
    //const QString CAMERACMD = "echo";
    //const QStringList CAMERAARGS = {"stuff"};
    //const QStringList CAMERAARGS = {"-t 0 -n --inline --framerate 30 --width 1920 --height 1080 --libav-format mjpeg -v 0 -o -"};
    const QStringList CAMERAARGS = {"-t","0" ,"-n","-v","0", "--inline" ,"--framerate","30","--width","1920","--height","1080","--codec","mjpeg","-o","-"};
    const QString CAMERACMD = "/usr/bin/libcamera-vid";
    QByteArray streamBuf;
    const QString IDLETEXT = "Waiting for camera...";
};

#endif // LIBCAMVIDQT_H
