#include "libcamvidqt.h"
#include <QStyle>

QString LibCamVidQt::camtunefile = "";

LibCamVidQt::LibCamVidQt() : LibCamVidQt(nullptr)
{}
LibCamVidQt::LibCamVidQt(QWidget * parent)
{
    setParent(parent);
    connect(&libcameraProc,&QProcess::readyReadStandardOutput,this,&LibCamVidQt::processFrame);

    connect(&libcameraProc,&QProcess::errorOccurred,this,&LibCamVidQt::errorHandler);
    this->setText(IDLETEXT);
}

LibCamVidQt::~LibCamVidQt(){
    stop();
}

void LibCamVidQt::errorHandler(QProcess::ProcessError error){
    if(running){
        qDebug() << "Camera error" << error;
        running = false;
    }
}

/**
 * @brief LibCamVidQt::getCurPixmap
 * @return Unscaled last pixmap read from camera
 */
QPixmap LibCamVidQt::getCurPixmap(){
    return this->curPixmap;
}

/**
 * @brief LibCamVidQt::processFrame
 * Process an mjpeg stream and display in this label
 */
void LibCamVidQt::processFrame(){
    static const QByteArray SOI = QByteArray::fromHex("ffd8");
    static const QByteArray EOI = QByteArray::fromHex("ffd9");
    constexpr qsizetype MAXBUFFER = 16*1024*1024; // Never let a broken stream grow without bound

    streamBuf.append(libcameraProc.readAllStandardOutput());
    // Extract all complete frames but only decode the newest one
    QByteArray image_data;
    while(true){
        qsizetype start = streamBuf.indexOf(SOI);
        if(start < 0){
            // No frame start. Keep the last byte in case it is the first half of a marker
            streamBuf = streamBuf.right(1);
            break;
        }
        if(start > 0){
            streamBuf.remove(0,start); // Drop bytes before the frame start (resync)
        }
        qsizetype end = streamBuf.indexOf(EOI,SOI.size());
        if(end < 0){
            break; // Frame not complete yet
        }
        image_data = streamBuf.left(end + EOI.size());
        streamBuf.remove(0,end + EOI.size());
    }
    if(streamBuf.size() > MAXBUFFER){
        streamBuf.clear();
    }
    if(image_data.isEmpty()){
        return;
    }
    if(curPixmap.loadFromData(image_data, "JPG")) {
        this->setPixmap(curPixmap.scaled(this->width(),this->height(), Qt::KeepAspectRatio));
    }else{
        this->setText("Error loading pixmap");
    }
}

void LibCamVidQt::hideEvent(QHideEvent *event){
    this->stop();
    QLabel::hideEvent(event);
}


void LibCamVidQt::showEvent(QShowEvent  *event){
    this->start();
    QLabel::showEvent(event);
}

void LibCamVidQt::resizeEvent(QResizeEvent  *event){
    if(!curPixmap.isNull()){
        this->setPixmap(curPixmap.scaled(this->width(),this->height(), Qt::KeepAspectRatio));
    }
    QLabel::resizeEvent(event);
}

#ifdef DUMMY
void LibCamVidQt::start(){
    this->setPixmap(QPixmap(":/images/res/nyanhalflong.png"));
}

void LibCamVidQt::stop(){
}
#else
void LibCamVidQt::start(){
    if(libcameraProc.state() != QProcess::NotRunning){
        return; // Still running or stopping
    }
    streamBuf.clear();
    qDebug() << "Starting camera";
    // stdout carries the MJPEG stream. Don't mix log output into it
    libcameraProc.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    QStringList args = CAMERAARGS;
    if(!camtunefile.isEmpty()){
        args << "--tuning-file" << camtunefile;
    }
    libcameraProc.start(CAMERACMD,args);
    running = true;
}

void LibCamVidQt::stop(){
    qDebug() << "Stopping camera";
    running = false; // Errors caused by stopping are expected
    if(libcameraProc.state() != QProcess::NotRunning){
        libcameraProc.terminate();
        if(!libcameraProc.waitForFinished(1000)){
            libcameraProc.kill();
            libcameraProc.waitForFinished(1000);
        }
    }
    streamBuf.clear();
    this->setText(IDLETEXT);
}

#endif
