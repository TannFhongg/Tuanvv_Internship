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
        QVERIFY(server.isListening());

        const quint16 port = server.serverPort();
        QVERIFY(port != 0);

        QVERIFY(server.startListening(QHostAddress::LocalHost, 0) == false);
        QCOMPARE(spy.count(), 1);
        QVERIFY(server.isListening());
        QCOMPARE(server.serverPort(), port);

        server.stop();
        QVERIFY(server.isListening() == false);
        QCOMPARE(server.serverPort(), 0);
    }

    void firstClientConnection_emitsClientConnected()
    {

        TcpServer server;
        QSignalSpy spyConnected(&server, &TcpServer::clientConnected);
        QSignalSpy spyRejected(&server, &TcpServer::clientRejected);

        QTcpSocket clientSocket;

        QVERIFY(server.startListening(QHostAddress::LocalHost, 0));

        QVERIFY(server.isListening() == true);
        const quint16 port = server.serverPort();
        QVERIFY(port != 0);

        QVERIFY(spyConnected.isValid() == true);
        QVERIFY(spyRejected.isValid() == true);
        QVERIFY(spyConnected.count() == 0);
        QVERIFY(spyRejected.count() == 0);

        clientSocket.connectToHost(QHostAddress::LocalHost, port);

        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(spyConnected.count(), 1);
        QCOMPARE(spyRejected.count(), 0);
        QTRY_VERIFY(server.isListening() == true);

        clientSocket.disconnectFromHost();
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::UnconnectedState);

        server.stop();
        QVERIFY(server.isListening() == false);
    }

    void secondClientConnection_isRejectedWhileFirstRemainsConnected()
    {

        TcpServer server;
        QTcpSocket clientA;
        QTcpSocket clientB;

        QSignalSpy spyConnected(&server, &TcpServer::clientConnected);
        QSignalSpy spyRejectted(&server, &TcpServer::clientRejected);
        QSignalSpy spyDisconnected(&server, &TcpServer::clientDisconnected);

        QVERIFY(server.startListening(QHostAddress::LocalHost, 0));

        const quint16 port = server.serverPort();
        QVERIFY(port != 0);
        clientA.connectToHost(QHostAddress::LocalHost, port);
        QTRY_COMPARE(clientA.state(), QAbstractSocket::ConnectedState);
        QCOMPARE(spyConnected.count(), 1);
        QCOMPARE(spyRejectted.count(), 0);

        clientB.connectToHost(QHostAddress::LocalHost, port);
        QTRY_COMPARE(spyRejectted.count(), 1);
        QTRY_COMPARE(clientB.state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(spyConnected.count(), 1);
        QTRY_COMPARE(clientA.state(), QAbstractSocket::ConnectedState);
        QVERIFY(spyDisconnected.count() == 0);
        QTRY_VERIFY(server.isListening() == true);

        clientA.disconnectFromHost();
        QTRY_COMPARE(clientA.state(), QAbstractSocket::UnconnectedState);
        server.stop();
    }

    void newClient_isAcceptedAfterActiveClientDisconnects()
    {
        TcpServer server;
        QTcpSocket clientA;
        QTcpSocket clientC;

        QSignalSpy spyConnected(&server, &TcpServer::clientConnected);
        QSignalSpy spyDisconnected(&server, &TcpServer::clientDisconnected);
        QSignalSpy spyRejected(&server, &TcpServer::clientRejected);

        QVERIFY(server.startListening(QHostAddress::LocalHost, 0));

        clientA.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientA.state(), QAbstractSocket::ConnectedState);
        QCOMPARE(spyConnected.count(), 1);
        QCOMPARE(spyRejected.count(), 0);
        
        clientA.disconnectFromHost();
        QTRY_COMPARE(clientA.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(spyDisconnected.count(), 1);

        clientC.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientC.state(), QAbstractSocket::ConnectedState);
        QCOMPARE(spyConnected.count(), 2);
        QTRY_COMPARE(spyDisconnected.count(), 1);
        QCOMPARE(spyRejected.count(), 0);
        QVERIFY(server.isListening() == true);

        clientC.disconnectFromHost();
        QTRY_COMPARE(clientC.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(spyDisconnected.count(), 2);
            
    }

    void stop_disconnectsActiveClientAndStopsListening() 
    {
        
        TcpServer server;
        QTcpSocket clientA;

        QSignalSpy spyConnected(&server, &TcpServer::clientConnected);
        QSignalSpy spyDisconnected(&server, &TcpServer::clientDisconnected);
        QSignalSpy spyRejected(&server, &TcpServer::clientRejected);

        QVERIFY(server.startListening(QHostAddress::LocalHost, 0));

        clientA.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientA.state(), QAbstractSocket::ConnectedState);
        QCOMPARE(spyConnected.count(), 1);
        QCOMPARE(spyDisconnected.count(), 0);

        server.stop();
        QVERIFY(server.isListening() == false);
        QCOMPARE(server.serverPort(), 0);

        QTRY_COMPARE(clientA.state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(spyDisconnected.count(), 1);
        QVERIFY(server.isListening() == false);

        server.stop();
        QVERIFY(server.isListening() == false);
         QCOMPARE(spyDisconnected.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TcpServerTest)
#include "tcpservertest.moc"
