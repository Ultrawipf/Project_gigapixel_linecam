#include "libcamvidqt.h"
#include <QStyle>
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
    QByteArray buf = libcameraProc.readAllStandardOutput();
    streamBuf.append(buf);
    int ind_start_bytes = streamBuf.indexOf(QByteArray::fromHex("ffd8"));
    int ind_end_bytes = streamBuf.indexOf(QByteArray::fromHex("ffd9"));
    if(ind_start_bytes != -1 && ind_end_bytes != -1) {
        if(ind_start_bytes < ind_end_bytes){
            QByteArray image_data = streamBuf.mid(ind_start_bytes, ind_end_bytes + 2);
            streamBuf = streamBuf.mid(ind_end_bytes+2);
            if(curPixmap.loadFromData(image_data, "JPG")) {
                this->setPixmap(curPixmap.transformed(QTransform().rotate(90)).scaled(this->width(),this->height(), Qt::KeepAspectRatio));
            }else{
                this->setText("Error loading pixmap");
            }
        }else{
            this->setText("Error finding jpeg" + QString::number(ind_start_bytes) + ":" +  QString::number(ind_end_bytes));
        }
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
        this->setPixmap(curPixmap.transformed(QTransform().rotate(90)).scaled(this->width(),this->height(), Qt::KeepAspectRatio));
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
    streamBuf.clear();
    qDebug() << "Starting camera";
    libcameraProc.setProcessChannelMode(QProcess::MergedChannels);
    libcameraProc.start(CAMERACMD,CAMERAARGS);
    running = true;
}

void LibCamVidQt::stop(){
    qDebug() << "Stopping camera";
    libcameraProc.terminate();
    streamBuf.clear();
    this->setText(IDLETEXT);
    running = false;
}

#endif
