#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QComboBox>
#include <QThread>
#include <vector>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void installThread();
    
private slots:
    void updateEditorStdout();

public slots:
    void openProjectButtonClick();
    void startEditorButtonClick();
    void installUpdatesButtonClick();
    void updateSourceChanged();

private:
    Ui::MainWindow *ui;
    QProcess editorProc;
    int progress;
    QString currentAction;
    std::vector<QProcess> tools;

    void updateProgress(int value);
    void loadHistory();
    void saveHistory();
    bool doesComboContain(QComboBox* box, QString string);
};

#endif // MAINWINDOW_H
