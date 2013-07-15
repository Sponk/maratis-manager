#ifndef GUITHREAD_H
#define GUITHREAD_H

#include <QObject>
#include "mainwindow.h"

class GuiThread : public QObject
{
    Q_OBJECT
public:
    explicit GuiThread(QObject *parent = 0);
    GuiThread(MainWindow* window) {this->window = window;}
    
signals:
    void finished();
    void error(QString error);
    
public slots:
    void process();

private:
    MainWindow* window;
    
};

#endif // GUITHREAD_H
