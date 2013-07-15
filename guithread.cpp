#include "guithread.h"
#include <QApplication>

GuiThread::GuiThread(QObject *parent) :
    QObject(parent)
{
}

void GuiThread::process()
{
    qDebug("Installing");
    window->installThread();
    emit finished();
}
