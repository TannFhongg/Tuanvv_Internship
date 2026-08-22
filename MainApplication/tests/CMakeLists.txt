add_executable(ClientNetworkTests
    networkclienttest.cpp
)

target_link_libraries(ClientNetworkTests
    PRIVATE
    Qt6::Test
    MainCore
)

add_test(
    NAME ClientNetworkTests
    COMMAND ClientNetworkTests)

add_test(
    NAME ClientNetworkTests.initialState_isUnconnected
    COMMAND ClientNetworkTests initialState_isUnconnected)

add_test(
    NAME ClientNetworkTests.connectToServer_invalidArguments_returnsFalse
    COMMAND ClientNetworkTests connectToServer_invalidArguments_returnsFalse)

add_test(
    NAME ClientNetworkTests.connectToServer_listeningServer_emitsConnected
    COMMAND ClientNetworkTests connectToServer_listeningServer_emitsConnected)

add_test(
    NAME ClientNetworkTests.disconnectFromServer_connectedClient_emitsDisconnected
    COMMAND ClientNetworkTests disconnectFromServer_connectedClient_emitsDisconnected)