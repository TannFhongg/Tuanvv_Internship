#ifndef SERVERWINDOW_H
#define SERVERWINDOW_H

#include <QMainWindow>
#include <QString>

namespace MiniCloud::Server
{
    class LicenseManager;
}

QT_BEGIN_NAMESPACE

namespace Ui
{
    class ServerWindow;
}
QT_END_NAMESPACE

class ServerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ServerWindow(MiniCloud::Server::LicenseManager &licenseManager, QWidget *parent = nullptr);

    ~ServerWindow() override;

private slots:
    void createLicense();
    void enableSelectedLicense();
    void disableSelectedLicense();

private:
    void refreshLicenses(const QString &preferredProductKey = {});
    void refreshSelectionActions();

    Ui::ServerWindow *ui;
    MiniCloud::Server::LicenseManager *m_licenseManager;
};
#endif // SERVERWINDOW_H
