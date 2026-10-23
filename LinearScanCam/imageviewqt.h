#ifndef IMAGEVIEWQT_H
#define IMAGEVIEWQT_H

#include <QObject>
#include <QWidget>
#include <QGraphicsView>
#include <QEvent>
#include "cameracontrol.h"
#include <QImage>
#include <filesystem>


class ImageviewQt : public QGraphicsView
{
    Q_OBJECT
public:
    ImageviewQt(QWidget* parent = nullptr);

    // Loads an image for display. A png with a gAMA chunk stores linear data and gets the gamma
    // curve applied here. Files without gAMA (older previews) are shown as stored. Thread safe
    static QImage loadImage(const std::filesystem::path& file);
    static QImage applyGamma(const QImage& image,double fileGamma);
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
