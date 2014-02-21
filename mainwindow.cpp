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

void MainWindow::createPluginClick()
{
    QString path = ui->lastUsedCombo->currentText();

    if(path == "Empty")
    {
        QMessageBox::information(this, tr("Error"), tr("You have to select a valid Maratis project!"));
        return;
    }

#ifndef WIN32
    path = path.replace(path.lastIndexOf("/"), path.size(), "");
    mkdir(QString(path + "/GamePlugin").toAscii(), S_IRUSR | S_IREAD | S_IWUSR | S_IXUSR);
    mkdir(QString(path + "/GamePlugin/src").toAscii(), S_IRUSR | S_IREAD | S_IWUSR | S_IXUSR);
    mkdir(QString(path + "/GamePlugin/build").toAscii(), S_IRUSR | S_IREAD | S_IWUSR | S_IXUSR);

    system(QString("touch '" + path + "/GamePlugin/src/main.cpp'").toAscii());

    QString prefix = QDir::homePath() + "/.maratis-manager/";
    QDir::setCurrent(prefix);

    system(QString("cp -rf Includes '" + path + "/GamePlugin'").toAscii());

    std::ofstream out;
    out.open(QString(path + "/GamePlugin/src/CMakeLists.txt").toAscii());

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

    out << "if(${CMAKE_SYSTEM_NAME} MATCHES \"Linux\")" << endl;
    out << "include_directories(${CMAKE_BINARY_DIR}/../Includes)" << endl;

    out << "endif(${CMAKE_SYSTEM_NAME} MATCHES \"Linux\")" << endl;

    out << "if(${CMAKE_SYSTEM_NAME} MATCHES \"Windows\")" << endl;

    out << "include_directories(${CMAKE_BINARY_DIR}/../Includes)" << endl;
    out << "link_directories(${CMAKE_BINARY_DIR}/../lib)" << endl;

    out << "foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} ) " << endl;
    out << "string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )" << endl;
    out << "set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/../..)" << endl;
    out << "set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/../..)" << endl;
    out << "set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/../..)" << endl;
    out << "endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )" << endl;

    out << "endif(${CMAKE_SYSTEM_NAME} MATCHES \"Windows\")" << endl;

    out << "add_library(Game SHARED ${all_SRC})" << endl;
    out << "set_target_properties(Game PROPERTIES PREFIX \"\")" << endl;
    out.close();

    QDir::setCurrent(path + "/GamePlugin/build");

    int status1 = 0;
    int status2 = 0;

    status1 = system("cmake ../src");
    status2 = system("cmake ../src -G 'CodeBlocks - Unix Makefiles'");

    if(status1 || status2)
    {
        QMessageBox::information(this, "Error", tr("Could not run CMake!"));
    }

#else

    QString appDir = QApplication::applicationDirPath();
    appDir += "/system/cmake/bin/";

    qDebug() << appDir;

    path = path.replace(path.lastIndexOf("/"), path.size(), "");
    rmdir(path + "/GamePlugin/build");
    QDir::root().mkpath(QString(path + "/GamePlugin"));
    QDir::root().mkpath(QString(path + "/GamePlugin/src"));
    QDir::root().mkpath(QString(path + "/GamePlugin/build"));
    QDir::root().mkpath(QString(path + "/GamePlugin/lib"));


    touchFile(path + "/GamePlugin/src/main.cpp");
    cp(QDir::current().absoluteFilePath("./Includes"), path + "/GamePlugin/Includes");
    //system(QString("cp -rf Includes '" + path + "/GamePlugin'").toAscii());

    QFile("./MSDK/MCore.lib").copy(path + "/GamePlugin/lib/MCore.lib");
    QFile("./MSDK/MEngine.lib").copy(path + "/GamePlugin/lib/MEngine.lib");

    std::ofstream out;
    QString cmakefile_path = QString(path + "/GamePlugin/src/CMakeLists.txt").replace("/", "\\");

    qDebug() << cmakefile_path;
    out.open(QString(path + "/GamePlugin/src/CMakeLists.txt").replace("/", "\\").toAscii());

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

    out << "if(${CMAKE_SYSTEM_NAME} MATCHES \"Linux\")" << endl;
    out << "include_directories(${CMAKE_BINARY_DIR}/../Includes)" << endl;

    out << "endif(${CMAKE_SYSTEM_NAME} MATCHES \"Linux\")" << endl;

    out << "if(${CMAKE_SYSTEM_NAME} MATCHES \"Windows\")" << endl;

    out << "include_directories(${CMAKE_BINARY_DIR}/../Includes)" << endl;
    out << "link_directories(${CMAKE_BINARY_DIR}/../lib)" << endl;

    out << "foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} ) " << endl;
    out << "string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )" << endl;
    out << "set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/../..)" << endl;
    out << "set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/../..)" << endl;
    out << "set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/../..)" << endl;
    out << "endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )" << endl;

    out << "endif(${CMAKE_SYSTEM_NAME} MATCHES \"Windows\")" << endl;

    out << "add_library(Game SHARED ${all_SRC})" << endl;
    out << "set_target_properties(Game PROPERTIES PREFIX \"\")" << endl;
    out.close();

    QDir::setCurrent(path + "/GamePlugin/build");

    int status1 = 0;
    int status2 = 0;

    status1 = QProcess::execute(appDir + "cmake.exe",   QStringList() << "../src");
    // status2 = QProcess::execute(appDir + "cmake.exe",   QStringList() << "../src" << "-G" << "'CodeBlocks'");

    if(status1 || status2)
    {
        QMessageBox::information(this, "Error", tr("Could not run CMake!"));
    }

#endif
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
    QDir::setCurrent(".\\Bin\\");
    QString currentPath = QDir::current().absolutePath();

#ifdef WIN32
    currentPath = currentPath.replace("/", "\\");
#endif
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
    QString prefix = "";

#ifndef WIN32
    prefix = QDir::homePath() + "/.maratis-manager/";
    QDir(QDir::homePath()).mkdir(prefix);
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
        if(system((QString("python scons.py -j") + QString::number(ui->threadsBox->value())).toAscii()) != 0)
        {
            QMessageBox::information(this, "Error", tr("Could not compile Maratis!"));
            currentAction = "Error!";
            progress = 100;
            return;
        }

        currentAction = "Cleaning up...";
        progress = 40;

        QDir::setCurrent("../../../");
        system("rm -r -f Bin");

        currentAction = "Install...";
        progress = 60;

        system("cp -r ./maratis-read-only/trunk/dev/prod/linux2/release/Maratis/Bin ./");
        system("cp -r ./maratis-read-only/trunk/dev/prod/linux2/release/MSDK ./");
        system("cp -r ./maratis-read-only/trunk/dev/MSDK/MCore/Includes ./");
        system("cp -r ./maratis-read-only/trunk/dev/MSDK/MEngine/Includes ./");

        progress = 80;
        currentAction = "Cleaning up...";

        if(!ui->keepCodeBox->checkState() == Qt::Checked)
        {
            system("rm -r -f maratis-read-only");
        }

        currentAction = "Done.";
        progress = 100;
#else
        QString appDir = QApplication::applicationDirPath();
        appDir += "\\system\\bin\\";

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
        int ret = QProcess::execute(appDir + "..\\Python27\\python.exe", QStringList() << "scons.py" << "-j" + QString::number(ui->threadsBox->value()).toAscii());

        if(ret != 0)
        {
            QMessageBox::information(this, "Error", tr("Could not compile Maratis!"));
            currentAction = "Error!";
            progress = 100;
            return;
        }

        progress = 40;

        currentAction = "Cleaning...";
        QDir::setCurrent("..\\..\\..\\");
        system("del /s /q Bin");
        system("mkdir Bin");

        progress = 60;

        cp(".\\maratis-read-only\\trunk\\dev\\prod\\win32_i386\\release\\Maratis\\Bin", ".\\Bin");
        cp(".\\maratis-read-only\\trunk\\dev\\prod\\win32_i386\\release\\MSDK", ".\\MSDK");
        cp(".\\maratis-read-only\\trunk\\dev\\MSDK\\MCore\\Inclueds", ".\\Includes");
        cp(".\\maratis-read-only\\trunk\\dev\\MSDK\\MEngine\\Inclueds", ".\\Includes");
        /*system("xcopy .\\maratis-read-only\\trunk\\dev\\prod\\win32_i386\\release\\Maratis\\Bin .\\Bin /e /i /h");
        system("xcopy .\\maratis-read-only\\trunk\\dev\\prod\\win32_i386\\release\\MSDK .\\MSDK /e /i /h");
        system("xcopy .\\maratis-read-only\\trunk\\dev\\MSDK\\MCore\\Includes .\\Includes /e /i /h");
        system("xcopy .\\maratis-read-only\\trunk\\dev\\MSDK\\MEngine\\Includes .\\Includes /e /i /h");*/

        progress = 80;

        if(!ui->keepCodeBox->checkState() == Qt::Checked)
        {
            rmdir("maratis-read-only");
            // system("del /s /q maratis-read-only");
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
#ifndef WIN32

        currentAction = "Downloading...";

        if(!QDir("maratis").exists())
        {
            system("git clone https://github.com/nistur/maratis.git");
            QDir::setCurrent(prefix + "./maratis");
        }
        else
        {
            QDir::setCurrent(prefix + "./maratis");
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
        SleepHelper::msleep(10);
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
