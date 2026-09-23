#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "backendclient.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onReloadTriggered();
    void onExitTriggered();
    void onAboutTriggered();
    void onBackendConnectionStateChanged(bool connected);

private:
    void updateConnectionStatus();

    Ui::MainWindow *ui;
    BackendClient *backend;
};
#endif // MAINWINDOW_H
