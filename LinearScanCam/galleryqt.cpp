#include "galleryqt.h"
#include <QMessageBox>
#include <QVariant>
#include <string>
#include <iostream>
GalleryQt::GalleryQt(QWidget *parent)
    : QListWidget{parent}
{

    setViewMode(QListWidget::IconMode);
    setIconSize(QSize(400,400));
    setResizeMode(QListWidget::Adjust);
    //connect(this,&QListWidget::itemDoubleClicked,this,&GalleryQt::editItem);
    setAttribute(Qt::WA_AcceptTouchEvents,true);
}

void GalleryQt::showEvent( QShowEvent* event ) {
    QWidget::showEvent( event );

    updateContents();
}


void GalleryQt::updateContents(){
    this->clear();
    // Get images in path
    for (const auto & entry : fs::directory_iterator(imagepathlowres)){
        if(entry.path().extension() == ".png"){
            QPixmap img(entry.path().c_str());
            img = img.transformed(QTransform().rotate(90));
            QListWidgetItem* newItem = new QListWidgetItem(QIcon(img),entry.path().filename().c_str());
            newItem->setFlags(newItem->flags() | Qt::ItemIsEditable);
            newItem->setData(Qt::UserRole,QVariant::fromValue(QString(entry.path().filename().c_str())));
            addItem(newItem);
        }
    }
}

void GalleryQt::editItem(QListWidgetItem *item){
    qDebug() << "Edit" << item->text();
}

void GalleryQt::deleteImage(){
    QListWidgetItem* item = currentItem();
    if(!item){
        return;
    }
    QMessageBox msg(QMessageBox::Question,QString("Delete image"),QString("Do you want to delete "+item->text() + "?"),QMessageBox::Ok | QMessageBox::Cancel,this,Qt::WindowStaysOnTopHint|Qt::FramelessWindowHint);
    int ret = msg.exec();

    if(ret == QMessageBox::Ok){

        try{
            std::string filename = item->data(Qt::UserRole).value<QString>().toStdString();
            qDebug() << "Delete" << filename.c_str();

            fs::remove(imagepathlowres/filename);
            fs::remove(imagepathfullres/filename);

        }catch (std::exception& e){
            std::cout << e.what() << '\n';
        }
        updateContents();
    }

}

void GalleryQt::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected){
    emit itemSelected(selected.empty() ? nullptr : currentItem());
}
