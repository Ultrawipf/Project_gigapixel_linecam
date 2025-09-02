#ifndef IMAGEVIEWQT_H
#define IMAGEVIEWQT_H

#include <QObject>
#include <QWidget>
#include <QGraphicsView>
#include <QEvent>
#include "cameracontrol.h"


class ImageviewQt : public QGraphicsView
{
    Q_OBJECT
public:
    ImageviewQt(QWidget* parent = nullptr);
public slots:
    bool viewportEvent(QEvent *event);
    void zoomToFit();

    void showFullImage(QImage image);
    //void showLine(const std::vector<ccdpixel>& line);
    //void setImageSize(int x, int y);


private:
    qreal totalScaleFactor = 1;
    //QImage curimage;
    //uint32_t curline = 0;
};

#endif // IMAGEVIEWQT_H
