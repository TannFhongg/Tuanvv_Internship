#include "serverwindow.h"
#include "serverapplicationruntime.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QHostAddress>
#include <QStandardPaths>

#include <limits>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Mini Cloud Server"));
    parser.addHelpOption();

    const QCommandLineOption dataDirectoryOption(
        QStringLiteral("data-dir"),
        QStringLiteral("Directory containing licenses.json and storage."),
        QStringLiteral("directory"));

    const QCommandLineOption listenAddressOption(
        QStringLiteral("listen-address"),
        QStringLiteral("IPv4 or IPv6 address for the TCP listener."),
        QStringLiteral("address"));

    const QCommandLineOption listenPortOption(
        QStringLiteral("listen-port"),
        QStringLiteral("TCP port for the listener."),
        QStringLiteral("port"));

    parser.addOption(dataDirectoryOption);
    parser.addOption(listenAddressOption);
    parser.addOption(listenPortOption);

    QStringList parserArguments = QCoreApplication::arguments();
    for (qsizetype index = 0; index < parserArguments.size(); ++index)
    {
        if (parserArguments.at(index) == QStringLiteral("-platform")
            || parserArguments.at(index) == QStringLiteral("--platform"))
        {
            parserArguments.removeAt(index);
            if (index < parserArguments.size())
            {
                parserArguments.removeAt(index);
            }
            break;
        }
    }

    parser.process(parserArguments);

    QString dataDirectoryPath = parser.value(dataDirectoryOption);
    if (dataDirectoryPath.isEmpty())
    {
        dataDirectoryPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }

    if (dataDirectoryPath.isEmpty())
    {
        qCritical("Unable to determine the server data directory.");
        return 1;
    }

    QHostAddress listenAddress = QHostAddress::AnyIPv4;
    if (parser.isSet(listenAddressOption)
        && !listenAddress.setAddress(parser.value(listenAddressOption)))
    {
        qCritical("Invalid listen address.");
        return 1;
    }

    quint16 listenPort = 8080;
    if (parser.isSet(listenPortOption))
    {
        bool portIsValid = false;
        const uint parsedPort = parser.value(listenPortOption).toUInt(&portIsValid);
        if (!portIsValid || parsedPort > std::numeric_limits<quint16>::max())
        {
            qCritical("Invalid listen port.");
            return 1;
        }

        listenPort = static_cast<quint16>(parsedPort);
    }

    const QDir dataDirectory(dataDirectoryPath);
    ServerApplicationRuntime::Configuration configuration;
    configuration.address = listenAddress;
    configuration.port = listenPort;
    configuration.licenseStoragePath = dataDirectory.filePath(QStringLiteral("licenses.json"));
    configuration.fileStorageRoot = dataDirectory.filePath(QStringLiteral("storage"));

    ServerApplicationRuntime runtime(configuration);
    if (!runtime.start())
    {
        qCritical().noquote() << "Unable to start server:" << runtime.lastError();
        return 1;
    }

    ServerWindow s(*runtime.licenseManager());
    s.show();
    return QApplication::exec();
}
