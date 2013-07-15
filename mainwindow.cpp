#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QStringList>
#include <QCheckBox>
#include <QTextStream>
#include <QMessageBox>
#include <guithread.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->progressBar->setVisible(false);

    connect(&editorProc, SIGNAL(readyReadStandardOutput()), this, SLOT(updateEditorStdout()));
    connect(&editorProc, SIGNAL(readyReadStandardError()), this, SLOT(updateEditorStdout()));

    loadHistory();
    editorProc.setReadChannelMode(QProcess::MergedChannels);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateEditorStdout()
{
    QProcess* proc = (QProcess*) sender();

    ui->consoleOutput->append(proc->readAll());
}

bool MainWindow::doesComboContain(QComboBox* box, QString string)
{
    for(int i = 0; i < ui->lastUsedCombo->count(); i++)
    {
        QString line = ui->lastUsedCombo->itemText(i);
        if(line == string)
            return true;
    }

    return false;
}

void MainWindow::loadHistory()
{
#ifdef WIN32
    QFile history(QDir::homePath() + "\\Documents\\maratis-project-history.conf");
#else
    QFile history(QDir::homePath() + "/.maratis-project-history.conf");
#endif

    if(!history.open(QIODevice::ReadOnly))
    {
        return;
    }

    QTextStream in(&history);
    while(!in.atEnd())
    {
        QString line = in.readLine();

        if(line.length() == 0)
            continue;

        ui->lastUsedCombo->addItem(line);
    }

    history.close();
}

void MainWindow::saveHistory()
{
#ifdef WIN32
    QFile history(QDir::homePath() + "\\Documents\\maratis-project-history.conf");
#else
    QFile history(QDir::homePath() + "/.maratis-project-history.conf");
#endif

    if(!history.open(QIODevice::WriteOnly))
    {
        return;
    }

    for(int i = 1; i < ui->lastUsedCombo->count(); i++)
    {
        QString line = ui->lastUsedCombo->itemText(i) + "\n";
        history.write(line.toUtf8());
    }

    history.close();
}

void MainWindow::openProjectButtonClick()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Open File"), tr("*.mproj"));

    if(filename.length() != 0 || !doesComboContain(ui->lastUsedCombo, filename))
        ui->lastUsedCombo->addItem(filename);
}

void MainWindow::startEditorButtonClick()
{
    QStringList args;
    QString project = ui->lastUsedCombo->currentText();

    if(project.length() != 0 && project != "Empty")
        args.append(project);

#ifdef linux
    QDir::setCurrent("./Bin/");
    QString currentPath = QDir::current().absolutePath();

    editorProc.start(currentPath + "/MaratisEditor", args);
    QDir::setCurrent("../");
#else
    QDir::setCurrent(".\\Bin\\");
    QString currentPath = QDir::current().absolutePath();

    editorProc.start(currentPath + "\\MaratisEditor.exe", args);
    QDir::setCurrent("..\\");
#endif

    saveHistory();
    ui->consoleOutput->clear();
    ui->tabWidget->setCurrentIndex(3);
}

void MainWindow::updateProgress(int value)
{
    ui->progressBar->setValue(value);
    repaint();
}

void MainWindow::installThread()
{
    int idx = ui->sourceCombo->currentIndex();

    switch(idx)
    {
    // Sourcecode from SVN
    case 0:
    {
#ifdef linux

        currentAction = "Downloading...";

        if(!QDir("maratis-read-only").exists())
        {
            system("svn checkout http://maratis.googlecode.com/svn/ maratis-read-only");
            QDir::setCurrent("./maratis-read-only");
        }
        else
        {
            QDir::setCurrent("./maratis-read-only");
            system("svn update");
        }

        currentAction = "Compiling...";
        progress = 20;

        QDir::setCurrent("./trunk/dev");
        system((QString("python scons.py -j") + QString::number(ui->threadsBox->value())).toAscii());

        currentAction = "Cleaning up...";
        progress = 40;

        QDir::setCurrent("../../../");
        system("rm -r -f Bin");

        currentAction = "Install...";
        progress = 60;

        system("cp -r ./maratis-read-only/trunk/dev/prod/linux2/release/Maratis/Bin ./");

        progress = 80;
        currentAction = "Cleaning up...";

        if(!ui->keepCodeBox->checkState() == Qt::Checked)
        {
            system("rm -r -f maratis-read-only");
        }

        currentAction = "Done.";
        progress = 100;
#else
        if(!QDir("maratis-read-only").exists())
        {
            system("svn checkout http://maratis.googlecode.com/svn/ maratis-read-only");
            QDir::setCurrent(".\\maratis-read-only");
        }
        else
        {
            QDir::setCurrent(".\\maratis-read-only");
            system("svn update");
        }

        progress = 20;

        QDir::setCurrent(".\\trunk\\dev");
        system((QString("python scons.py -j") + QString::number(ui->threadsBox->value())).toAscii());

        progress = 40;

        QDir::setCurrent("..\\..\\..\\");
        system("del /s /q Bin");
        system("mkdir Bin");

        progress = 60;

        system("xcopy .\\maratis-read-only\\trunk\\dev\\prod\\win32_i386\\release\\Maratis\\Bin .\\Bin /e /i /h");

        progress = 80;

        if(!ui->keepCodeBox->checkState() == Qt::Checked)
        {
            system("del /s /q maratis-read-only");
        }

        progress = 100;
#endif
    }
        break;

    // Binary release
    case 1:

    break;

    // Nisturs Maratis via Git
    case 2:
    {
#ifdef linux

        currentAction = "Downloading...";

        if(!QDir("maratis").exists())
        {
            system("git clone https://github.com/nistur/maratis.git");
            QDir::setCurrent("./maratis");
        }
        else
        {
            QDir::setCurrent("./maratis");
            system("git pull");
        }

        currentAction = "Compiling...";
        progress = 20;

        QDir::setCurrent("./trunk/dev");
        system((QString("python scons.py -j") + QString::number(ui->threadsBox->value())).toAscii());

        currentAction = "Cleaning up...";
        progress = 40;

        QDir::setCurrent("../../../");
        system("rm -r -f Bin");

        currentAction = "Install...";
        progress = 60;

        system("cp -r ./maratis/trunk/dev/prod/linux2/release/Maratis/Bin ./");

        progress = 80;
        currentAction = "Cleaning up...";

        if(!ui->keepCodeBox->checkState() == Qt::Checked)
        {
            system("rm -r -f maratis");
        }

        currentAction = "Done.";
        progress = 100;
#else
        if(!QDir("maratis").exists())
        {
            system("git clone https://github.com/nistur/maratis.git");
            QDir::setCurrent(".\\maratis");
        }
        else
        {
            QDir::setCurrent(".\\maratis");
            system("git pull");
        }

        progress = 20;

        QDir::setCurrent(".\\trunk\\dev");
        system((QString("python scons.py -j") + QString::number(ui->threadsBox->value())).toAscii());

        progress = 40;

        QDir::setCurrent("..\\..\\..\\");
        system("del /s /q Bin");
        system("mkdir Bin");

        progress = 60;

        system("xcopy .\\maratis\\trunk\\dev\\prod\\win32_i386\\release\\Maratis\\Bin .\\Bin /e /i /h");

        progress = 80;

        if(!ui->keepCodeBox->checkState() == Qt::Checked)
        {
            system("del /s /q maratis");
        }

        progress = 100;
#endif
    }
        break;
    }
}

void MainWindow::installUpdatesButtonClick()
{
    if(ui->sourceCombo->currentIndex() == 1)
    {
        QMessageBox::information(NULL, tr("Sorry"), tr("Sorry. This feature is not yet implemented."));
        return;
    }

    QThread* thread;
    thread = new QThread();

    GuiThread* worker = new GuiThread(this);
    worker->moveToThread(thread);

    connect(worker, SIGNAL(error(QString)), this, SLOT(errorString(QString)));
    connect(thread, SIGNAL(started()), worker, SLOT(process()));
    connect(worker, SIGNAL(finished()), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

    thread->start();

    ui->progressLabel->setVisible(true);
    ui->progressBar->setVisible(true);
    ui->progressBar->setValue(0);
    progress = 0;

    while(thread->isRunning())
    {
        QApplication::processEvents();
        updateProgress(progress);
        ui->progressLabel->setText(currentAction);
    }

    ui->progressBar->setVisible(false);
    ui->progressLabel->setVisible(false);
}

void MainWindow::updateSourceChanged()
{
    bool svn = (ui->sourceCombo->currentIndex() == 0 || ui->sourceCombo->currentIndex() == 2);

    ui->keepCodeBox->setEnabled(svn);
    ui->threadsBox->setEnabled(svn);
}
