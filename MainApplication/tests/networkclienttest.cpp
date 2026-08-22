#include <QtTest/QTest>
#include <QTcpServer>
#include "NetworkClient.h"
#include <QSignalSpy>
#include <QTcpSocket>
class NetworkClientTest : public QObject
{
    Q_OBJECT

private slots:
    void initialState_isUnconnected()
    {
        NetworkClient networkClient;

        QSignalSpy spyConnected(&networkClient, &NetworkClient::connected);
        QSignalSpy spyDisconnected(&networkClient, &NetworkClient::disconnected);
        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spystateChanged(&networkClient, &NetworkClient::stateChanged);

        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(networkClient.isConnected(), false);
        QCOMPARE(spyConnected.count(), 0);
        QCOMPARE(spyDisconnected.count(), 0);
        QCOMPARE(spyConnectionError.count(), 0);
        QCOMPARE(spystateChanged.count(), 0);

        networkClient.disconnectFromServer();
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(networkClient.isConnected(), false);
        QCOMPARE(spyConnected.count(), 0);
        QCOMPARE(spyDisconnected.count(), 0);
        QCOMPARE(spyConnectionError.count(), 0);
        QCOMPARE(spystateChanged.count(), 0);
    }

    void connectToServer_invalidArguments_returnsFalse()
    {
        NetworkClient networkClient;

        QSignalSpy spyConnected(&networkClient, &NetworkClient::connected);
        QSignalSpy spyDisconnected(&networkClient, &NetworkClient::disconnected);
        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spystateChanged(&networkClient, &NetworkClient::stateChanged);

        quint16 port = 12345;
        networkClient.connectToServer("", port);
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(networkClient.isConnected(), false);
        QCOMPARE(spyConnected.count(), 0);
        QCOMPARE(spyDisconnected.count(), 0);
        QCOMPARE(spystateChanged.count(), 0);

        QString hostname = "127.0.0.1";
        networkClient.connectToServer(hostname, 0);
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(networkClient.isConnected(), false);
        QCOMPARE(spyConnected.count(), 0);
        QCOMPARE(spyDisconnected.count(), 0);
        QCOMPARE(spystateChanged.count(), 0);
    }

    void connectToServer_listeningServer_emitsConnected()
    {
        QTcpServer server;
        server.listen(QHostAddress::LocalHost, 0);

        NetworkClient networkClient;
        QSignalSpy spyConnected(&networkClient, &NetworkClient::connected);
        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spystateChanged(&networkClient, &NetworkClient::stateChanged);

        QSignalSpy spynewConnection(&server, &QTcpServer::newConnection);

        quint16 port = server.serverPort();
        QString hostname = "127.0.0.1";

        QVERIFY(networkClient.connectToServer(hostname, port) == true);

        QTRY_COMPARE(spyConnected.count(), 1);
        QTRY_COMPARE(spynewConnection.count(), 1);

        QTRY_COMPARE(networkClient.isConnected(), true);
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QCOMPARE(spyConnectionError.count(), 0);

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        networkClient.disconnectFromServer();
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);

        serverSocket->close();
    }

    void disconnectFromServer_connectedClient_emitsDisconnected()
    {

        QTcpServer server;
        server.listen(QHostAddress::LocalHost, 0);

        NetworkClient networkClient;
        quint16 port = server.serverPort();
        QString hostname = "127.0.0.1";
        QVERIFY(networkClient.connectToServer(hostname, port) == true);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        QSignalSpy spyDisconnected(&networkClient, &NetworkClient::disconnected);
        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spystateChanged(&networkClient, &NetworkClient::stateChanged);
        
        QSignalSpy spyClientDisconnected(serverSocket, &QTcpSocket::disconnected);

        networkClient.disconnectFromServer();

        QTRY_COMPARE(spyDisconnected.count(), 1);
        QTRY_COMPARE(spyClientDisconnected.count(), 1);
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QVERIFY(networkClient.isConnected() == false);
        QTRY_COMPARE(spyConnectionError.count(), 0);


    }
};

QTEST_GUILESS_MAIN(NetworkClientTest)
#include "networkclienttest.moc"