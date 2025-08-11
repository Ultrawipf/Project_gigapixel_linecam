#ifndef IMAGEVIEWQT_H
#define IMAGEVIEWQT_H

#include <QObject>
#include <QWidget>
#include <QGraphicsView>
#include <QEvent>


class ImageviewQt : public QGraphicsView
{
    Q_OBJECT
public:
    ImageviewQt(QWidget* parent = nullptr);
    bool viewportEvent(QEvent *event);
    void zoomToFit();
private:
    qreal totalScaleFactor = 1;
};

#endif // IMAGEVIEWQT_H
