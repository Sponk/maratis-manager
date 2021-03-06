#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QStringList>
#include <QCheckBox>
#include <QTextStream>
#include <QMessageBox>
#include <guithread.h>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <fstream>
#include <QDebug>

#ifndef WIN32
    #include <sys/stat.h>
#endif

#define PROGRAM_VERSION_STRING "0.2.1"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->progressBar->setVisible(false);

    ui->maratisManagerVersionLabel->setText(PROGRAM_VERSION_STRING);
    ui->weblinkLabel->setOpenExternalLinks(true);

    connect(&editorProc, SIGNAL(readyReadStandardOutput()), this, SLOT(updateEditorStdout()));
    connect(&editorProc, SIGNAL(readyReadStandardError()), this, SLOT(updateEditorStdout()));

    loadHistory();
    editorProc.setReadChannelMode(QProcess::MergedChannels);
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::rmdir(QString dirName)
{
    bool result = true;
    QDir dir(dirName);

    if (dir.exists(dirName))
    {
        Q_FOREACH(QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files, QDir::DirsFirst))
        {
            if (info.isDir())
            {
                result = rmdir(info.absoluteFilePath());
            }
            else
            {
                result = QFile::remove(info.absoluteFilePath());
            }

            if (!result)
            {
                return result;
            }
        }
        result = dir.rmdir(dirName);
    }
    return result;
}

void MainWindow::cp(QString src, QString dest)
{
    // qDebug() << "Source: " << src << " Dest: " << dest;
    QDir dir(src);
    if (!dir.exists())
        return;

    dest = QDir(dest).absolutePath();
    src = QDir(src).absolutePath();
    QDir::root().mkpath(dest);

    foreach (QString d, dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        QString dest_path = dest + QDir::separator() + d;
        dir.mkpath(dest_path);
        cp(src + QDir::separator() + d, dest_path);
    }

    foreach (QString f, dir.entryList(QDir::Files))
    {
        // qDebug() << "Copy: " << src + QDir::separator() + f;
        // qDebug() << "To: " << dest + QDir::separator() + f;
        QFile::copy(src + QDir::separator() + f, dest + QDir::separator() + f);
    }
}

void MainWindow::touchFile(QString file)
{
    QFile f(file);

    if(f.exists())
        return;

    f.open(QIODevice::WriteOnly);
    f.close();
}

void MainWindow::openWithFileManager()
{
    QString path = ui->lastUsedCombo->currentText();
    if(path == "Empty")
    {
        QMessageBox::information(this, tr("Warning"), tr("You have to select a valid Maratis project!"));
        return;
    }

    path = path.replace(path.lastIndexOf("/"), path.size(), "");

    QDesktopServices::openUrl("file:///" + path);
}

void MainWindow::rerunCMake()
{
    QString path = ui->lastUsedCombo->currentText();

    if(path == "Empty")
    {
        QMessageBox::information(this, tr("Error"), tr("You have to select a valid Maratis project!"));
        return;
    }

    path = path.replace(path.lastIndexOf("/"), path.size(), "");
    QDir::setCurrent(path + "/GamePlugin/build");

#ifdef WIN32
    QString appDir = QApplication::applicationDirPath();
    appDir += "/system/cmake/bin/";
#endif

    int status1 = 0;
    int status2 = 0;

#ifdef WIN32
    status1 = QProcess::execute(appDir + "cmake.exe",   QStringList() << "../src");
#else
    status1 = QProcess::execute("/usr/bin/cmake",   QStringList() << "../src");
    status2 = QProcess::execute("/usr/bin/cmake",   QStringList() << "../src" << "-G" << "CodeBlocks - Unix Makefiles");
#endif

    if(status1 || status2)
    {
        QMessageBox::information(this, "Error", tr("Could not run CMake!"));
    }
    else
    {
        QMessageBox::information(this, tr("Information"), tr("CMake build system succesfully updated!"));
    }
}

void MainWindow::createPluginClick()
{
    QString path = ui->lastUsedCombo->currentText();

    if(path == "Empty")
    {
        QMessageBox::information(this, tr("Error"), tr("You have to select a valid Maratis project!"));
        return;
    }

#ifdef WIN32
    QString appDir = QApplication::applicationDirPath();
    appDir += "/system/cmake/bin/";
    QString prefix = qgetenv("APPDATA") + "/maratis-manager/";
    prefix = prefix.replace("\\", "/");
#else
    QString appDir = "";
    QString prefix = QDir::homePath() + "/.maratis-manager/";
#endif

    path = path.replace(path.lastIndexOf("/"), path.size(), "");
    rmdir(path + "/GamePlugin/build");
    QDir::root().mkpath(QString(path + "/GamePlugin"));
    QDir::root().mkpath(QString(path + "/GamePlugin/src"));
    QDir::root().mkpath(QString(path + "/GamePlugin/build"));
    QDir::root().mkpath(QString(path + "/GamePlugin/lib"));

    touchFile(path + "/GamePlugin/src/main.cpp");
    cp(prefix + "./Includes", path + "/GamePlugin/Includes");

#ifdef WIN32
    QFile::copy(prefix + "MSDK/MCore.lib", path + "/GamePlugin/lib/MCore.lib");
    QFile::copy(prefix + "MSDK/MEngine.lib", path + "/GamePlugin/lib/MEngine.lib");
#endif

    std::ofstream out;

#ifdef WIN32
    QString cmakefile_path = QString(path + "/GamePlugin/src/CMakeLists.txt").replace("/", "\\");
#else
    QString cmakefile_path = QString(path + "/GamePlugin/src/CMakeLists.txt");
#endif

    out.open(cmakefile_path.toLatin1());

    if(!out.is_open())
    {
        QMessageBox::information(this, "Error", tr("Could not open file: ") + path + "/GamePlugin/src/CMakeLists.txt");
        return;
    }

    using std::endl;

    out << "cmake_minimum_required(VERSION 2.6)" << endl;
    out << "project(Plugin)" << endl;
    out << "file(GLOB all_SRC \"*.h\" \"*.cpp\")" << endl;
    out << "set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../..)" << endl;
    out << "set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../..)" << endl;
    out << "set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../..)" << endl;

    out << "if(${CMAKE_SYSTEM_NAME} MATCHES \"Windows\")" << endl;
    out << "foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} ) " << endl;
    out << "string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )" << endl;
    out << "set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/../..)" << endl;
    out << "set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/../..)" << endl;
    out << "set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/../..)" << endl;
    out << "endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )" << endl;
    out << "endif(${CMAKE_SYSTEM_NAME} MATCHES \"Windows\")" << endl;

    out << "if(${CMAKE_SYSTEM_NAME} MATCHES \"Linux\")" << endl;
    out << "include_directories(${CMAKE_BINARY_DIR}/../Includes)" << endl;
    out << "endif(${CMAKE_SYSTEM_NAME} MATCHES \"Linux\")" << endl;

    out << "add_library(Game SHARED ${all_SRC})" << endl;
    out << "set_target_properties(Game PROPERTIES PREFIX \"\")" << endl;

    out << "if(${CMAKE_SYSTEM_NAME} MATCHES \"Windows\")" << endl;
    out << "include_directories(${CMAKE_BINARY_DIR}/../Includes)" << endl;

    out << "TARGET_LINK_LIBRARIES(Game ${CMAKE_BINARY_DIR}/../lib/MEngine.lib ${CMAKE_BINARY_DIR}/../lib/MCore.lib)" << endl;
    out << "endif(${CMAKE_SYSTEM_NAME} MATCHES \"Windows\")" << endl;
    out.close();

    rerunCMake();
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
    QFile history(QDir::homePath() + "/.maratis-manager/history.conf");
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
    QFile history(QDir::homePath() + "/.maratis-manager/history.conf");
#endif

    if(!history.open(QIODevice::WriteOnly))
    {
        QMessageBox::information(this, "Error", tr("Could not save history: Could not open ") + history.fileName());
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

    saveHistory();
}

void MainWindow::startEditorButtonClick()
{
    QStringList args;
    QString project = ui->lastUsedCombo->currentText();

#ifdef WIN32
    project = project.replace("/", "\\");
#endif

    if(project.length() != 0 && project != "Empty")
        args.append(project);

#ifndef WIN32
    QString prefix = QDir::homePath() + "/.maratis-manager/";
    QDir::setCurrent(prefix + "/Bin/");
    QString currentPath = QDir::current().absolutePath();

    if(!QFile(currentPath + "/MaratisEditor").exists())
    {
        QMessageBox::information(this, tr("Error"), tr("The Maratis3D editor is not yet installed. Please install it via the 'Install' button on the 'Update' tab."));

    }

    editorProc.start(currentPath + "/MaratisEditor", args);
    QDir::setCurrent("../");
#else
    QString prefix = qgetenv("APPDATA") + "/maratis-manager/";
    prefix = prefix.replace("\\", "/");
    QDir::setCurrent(prefix + "Bin");

    editorProc.start(prefix + "Bin/MaratisEditor.exe", args);
    QDir::setCurrent(QApplication::applicationDirPath());
#endif

    saveHistory();
    ui->consoleOutput->clear();
    ui->tabWidget->setCurrentIndex(2);
}

void MainWindow::updateProgress(int value)
{
    ui->progressBar->setValue(value);
    repaint();
}

void MainWindow::installThread()
{
    int idx = ui->sourceCombo->currentIndex();
    QString prefix = "";

#ifndef WIN32
    prefix = QDir::homePath() + "/.maratis-manager/";
    QDir(QDir::homePath()).mkdir(prefix);
    QDir::setCurrent(prefix);
#else
    prefix = qgetenv("APPDATA") + "\\maratis-manager\\";
    QDir::root().mkdir(prefix);
    QDir::setCurrent(prefix);
#endif

    switch(idx)
    {
    // Sourcecode from SVN
    case 0:
    {
#ifndef WIN32

        currentAction = "Downloading...";

        if(!QDir("maratis-read-only").exists())
        {  
            QProcess::execute("/usr/bin/svn", QStringList() << "checkout" << "http://maratis.googlecode.com/svn/" << "maratis-read-only");
            QDir::setCurrent("./maratis-read-only");
        }
        else
        {
            QDir::setCurrent("./maratis-read-only");
            QProcess::execute("/usr/bin/svn", QStringList() << "update");
        }

        currentAction = "Compiling...";
        progress = 20;

        QDir::setCurrent("./trunk/dev");

        int ret = QProcess::execute("/usr/bin/python", QStringList() << "scons.py" << "-j" + QString::number(ui->threadsBox->value()).toLatin1());

        if(ret != 0)
        {
            currentAction = tr("Could not compile Maratis!");
            progress = 100;
            return;
        }

        currentAction = "Cleaning up...";
        progress = 40;

        QDir::setCurrent("../../../");
        //system("rm -r -f Bin");
        rmdir("Bin");

        currentAction = "Install...";
        progress = 60;

        cp("./maratis-read-only/trunk/dev/prod/linux2/release/Maratis/Bin", "./Bin");
        cp("./maratis-read-only/trunk/dev/prod/linux2/release/MSDK", "./MSDK");
        cp("./maratis-read-only/trunk/dev/MSDK/MCore/Includes", "./Includes");
        cp("./maratis-read-only/trunk/dev/MSDK/MEngine/Includes", "./Includes");

        progress = 80;
        currentAction = "Cleaning up...";

        if(!ui->keepCodeBox->checkState() == Qt::Checked)
        {
            rmdir("maratis-read-only");
        }

        currentAction = tr("Maratis installation was succesfully!");
        progress = 100;
#else
        QString appDir = QApplication::applicationDirPath();
        appDir += "/system/bin/";

        // qDebug() << "prefix = " << prefix;
        // qDebug() << "appDir = " << appDir;

        currentAction = "Downloading...";
        if(!QDir("maratis-read-only").exists())
        {
            QProcess::execute(appDir + "svn.exe", QStringList() << "checkout" << "http://maratis.googlecode.com/svn/" << "maratis-read-only");
            //system("svn checkout http://maratis.googlecode.com/svn/ maratis-read-only");
            QDir::setCurrent(".\\maratis-read-only");
        }
        else
        {
            QDir::setCurrent(".\\maratis-read-only");
            QProcess::execute(appDir + "svn.exe", QStringList() << "update");
        }

        progress = 20;
        currentAction = "Compiling...";

        QDir::setCurrent(".\\trunk\\dev");

        // qDebug() << "pwd = " << QDir::currentPath();
        // qDebug() << "python = " << appDir + "../Python27/python.exe";

        int ret = QProcess::execute(appDir + "../Python27/python.exe", QStringList() << "scons.py" << "-j" + QString::number(ui->threadsBox->value()).toAscii());

        if(ret != 0)
        {
            currentAction = tr("Could not compile Maratis! Failed to launch Python!");
            progress = 100;
            return;
        }

        progress = 40;

        currentAction = "Cleaning...";
        QDir::setCurrent("..\\..\\..\\");
        //system("del /s /q Bin");
        rmdir("Bin");
        QDir::current().mkdir("Bin");
        //system("mkdir Bin");

        progress = 60;

        cp(".\\maratis-read-only\\trunk\\dev\\prod\\win32_i386\\release\\Maratis\\Bin", ".\\Bin");
        cp(".\\maratis-read-only\\trunk\\dev\\prod\\win32_i386\\release\\MSDK", ".\\MSDK");
        cp(".\\maratis-read-only\\trunk\\dev\\MSDK\\MCore\\Inclueds", ".\\Includes");
        cp(".\\maratis-read-only\\trunk\\dev\\MSDK\\MEngine\\Inclueds", ".\\Includes");

        progress = 80;

        if(!ui->keepCodeBox->checkState() == Qt::Checked)
        {
            rmdir("maratis-read-only");
            // system("del /s /q maratis-read-only");
        }

        currentAction = tr("Maratis installation was succesfully!");
        progress = 100;
#endif
    }
        break;

    // Binary release
    case 1:

    break;
    }
}

void MainWindow::cleanInstallation()
{
    if(ui->sourceCombo->currentIndex() == 1)
    {
        QMessageBox::information(this, tr("Sorry"), tr("Sorry. This feature is not yet implemented."));
        return;
    }

    QString prefix = "";

#ifndef WIN32
    prefix = QDir::homePath() + "/.maratis-manager/";
    QDir::setCurrent(prefix);
#endif

    rmdir(prefix + "/maratis-read-only");
    rmdir(prefix + "/Bin");
    rmdir(prefix + "/Includes");
    rmdir(prefix + "/MSDK");

    QMessageBox::information(this, tr("Done."), tr("Your installation was succesfully deleted."));
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
        SleepHelper::msleep(10);
    }

    ui->progressBar->setVisible(false);
    ui->progressLabel->setVisible(false);

    QMessageBox::information(this, "Done.", currentAction);
}

void MainWindow::updateSourceChanged()
{
    bool svn = (ui->sourceCombo->currentIndex() == 0 || ui->sourceCombo->currentIndex() == 2);

    ui->keepCodeBox->setEnabled(svn);
    ui->threadsBox->setEnabled(svn);
}

void MainWindow::currentTabChanged(int idx)
{
#ifndef WIN32
    if(QFile(QDir::homePath() + "/.maratis-manager/Bin/MaratisEditor").exists())
        ui->editorStatusLabel->setText(tr("The Maratis editor is installed!"));
#else
    QString prefix = qgetenv("APPDATA") + "/maratis-manager/";
    prefix = prefix.replace("\\", "/");

    if(QFile(prefix + "Bin/MaratisEditor.exe").exists())
        ui->editorStatusLabel->setText(tr("The Maratis editor is installed!"));
#endif
    else
        ui->editorStatusLabel->setText(tr("The Maratis editor is NOT installed!"));
}
