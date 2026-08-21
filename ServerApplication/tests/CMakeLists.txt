add_executable(ServerNetworkTests
    tcpservertest.cpp
)
target_link_libraries(ServerNetworkTests PRIVATE
    ServerCore
    Qt6::Test
    Qt6::Network
)

add_test(
    NAME ServerNetworkTests
    COMMAND ServerNetworkTests
)

add_test(
    NAME ServerNetworkTests.startAndStop_updatesListeningState
    COMMAND ServerNetworkTests startAndStop_updatesListeningState
)

add_test(
    NAME ServerNetworkTests.firstClientConnection_emitsClientConnected
    COMMAND ServerNetworkTests  firstClientConnection_emitsClientConnected)

add_test(
    NAME ServerNetworkTests.secondClientConnection_isRejectedWhileFirstRemainsConnected
    COMMAND ServerNetworkTests secondClientConnection_isRejectedWhileFirstRemainsConnected)

add_test(
    NAME ServerNetworkTests.newClient_isAcceptedAfterActiveClientDisconnects
    COMMAND ServerNetworkTests newClient_isAcceptedAfterActiveClientDisconnects)

add_test(
    NAME ServerNetworkTests.stop_disconnectsActiveClientAndStopsListening
    COMMAND ServerNetworkTests stop_disconnectsActiveClientAndStopsListening)