#ifndef GALLERYQT_H
#define GALLERYQT_H

#include <QWidget>
#include <QListWidget>
#include <filesystem>

namespace fs = std::filesystem;

class GalleryQt : public QListWidget
{
    Q_OBJECT
public:
    explicit GalleryQt(QWidget *parent = nullptr);

    fs::path imagepathfullres = "./img";
    fs::path imagepathlowres = "./preview";

    void showEvent( QShowEvent* event ) override;


public slots:
    void editItem(QListWidgetItem *item);
    void updateContents();
    void deleteImage();
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;

signals:
    void itemSelected(QListWidgetItem* selected);
};

#endif // GALLERYQT_H
