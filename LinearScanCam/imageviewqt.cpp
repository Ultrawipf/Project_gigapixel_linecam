#include "imageviewqt.h"
#include <QTouchEvent>

ImageviewQt::ImageviewQt(QWidget* parent)
    : QGraphicsView(parent)
{
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    //setDragMode(ScrollHandDrag);
}



bool ImageviewQt::viewportEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    {
        QTouchEvent *touchEvent = static_cast<QTouchEvent *>(event);
        const auto touchPoints = touchEvent->points();
        if (touchPoints.count() == 2) {
            // determine scale factor
            const QEventPoint &touchPoint0 = touchPoints.first();
            const QEventPoint &touchPoint1 = touchPoints.last();
            qreal currentScaleFactor =
                QLineF(touchPoint0.position(), touchPoint1.position()).length()
                / QLineF(touchPoint0.pressPosition(), touchPoint1.pressPosition()).length();
            if (touchEvent->touchPointStates() & QEventPoint::Released) {
                // if one of the fingers is released, remember the current scale
                // factor so that adding another finger later will continue zooming
                // by adding new scale factor to the existing remembered value.
                totalScaleFactor *= currentScaleFactor;
                currentScaleFactor = 1;
            }
            setTransform(QTransform::fromScale(totalScaleFactor * currentScaleFactor,
                                               totalScaleFactor * currentScaleFactor));
            return true;
        }

    }
    default:
        break;
    }
    return QGraphicsView::viewportEvent(event);
}


void ImageviewQt::zoomToFit(){
    scene()->setSceneRect(scene()->itemsBoundingRect());
    fitInView(scene()->itemsBoundingRect(), Qt::KeepAspectRatio);
}

// void ImageviewQt::setImageSize(int x, int y){
//     curimage = QImage(x,y,QImage::Format::Format_RGB32);
//     //showFullImage(curimage);
//     curline = 0;
// }


void ImageviewQt::showFullImage(QImage image){
    qDebug() << "Showing new image";
    auto curSceneRect = scene()->sceneRect();
    scene()->clear();
    //setAlignment(Qt::AlignCenter);
    image = image.transformed(QTransform().rotate(90));
    auto imageitem = QPixmap::fromImage(image);
    auto imagerect = imageitem.rect().toRectF();

    scene()->addPixmap(imageitem);
    if(curSceneRect.width() != imagerect.width() && curSceneRect.height() != imagerect.height()){
        resetTransform();
        this->centerOn(scene()->itemsBoundingRect().center());
        zoomToFit();
    }

    //qDebug() << "Scene rect" << scene()->sceneRect();
}

// void ImageviewQt::showLine(const std::vector<ccdpixel>& line){
//     // qDebug() << "Got line" << line.size();
//     QRgb *scanline = reinterpret_cast<QRgb*>(curimage.scanLine(curline));
//     uint32_t lineptr = 0;
//     for (int x = 0; x < curimage.width(); ++x) {
//         scanline[x] = qRgb(line[lineptr]/255,line[lineptr+1]/255,line[lineptr+2]/255);
//         lineptr+=3;
//     }
//     curline++;
//     scene()->clear();
//     scene()->addPixmap(QPixmap::fromImage(curimage));
// }
