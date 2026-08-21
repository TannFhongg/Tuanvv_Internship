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