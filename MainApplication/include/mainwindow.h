#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class ApplicationController;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(ApplicationController *controller, QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void clearFileSessionUi();
    void refreshUiState();

    Ui::MainWindow *ui;
    ApplicationController *m_controller;
    bool m_refreshCurrentDirectoryAfterMutation = false;
    bool m_transferInProgress = false;
    bool m_hadActiveFileSession = false;
    bool m_userInitiatedDisconnect = false;
};
#endif // MAINWINDOW_H
