#include <QtTest/QTest>
#include "TcpServer.h"
#include <QSignalSpy>
class TcpServerTest : public QObject
{
    Q_OBJECT
private slots:

    void startAndStop_updatesListeningState()
    {
        TcpServer server;
        QSignalSpy spy(&server, &TcpServer::listenFailed); 

        QVERIFY(server.isListening() == false);

        QVERIFY(server.startListening(QHostAddress::LocalHost, 0));
        QVERIFY(server.isListening() );

        const quint16 port = server.serverPort(); 
        QVERIFY(port != 0);

        QVERIFY(server.startListening(QHostAddress::LocalHost, 0) == false);
        QCOMPARE(spy.count(), 1);
        QVERIFY(server.isListening() );
        QCOMPARE(server.serverPort(), port);

        server.stop();
        QVERIFY(server.isListening() == false);
        QCOMPARE(server.serverPort(), 0);
    }
};

QTEST_APPLESS_MAIN(TcpServerTest)
#include "tcpservertest.moc"
