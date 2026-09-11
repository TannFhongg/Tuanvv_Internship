#include "serverwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>

#include "licensemanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    const QString licenseRepositoryPath =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("licenses.json"));

    MiniCloud::Server::LicenseManager licenseManager(licenseRepositoryPath);
    licenseManager.initialize();

    ServerWindow s(licenseManager);
    s.show();
    return QApplication::exec();
}
