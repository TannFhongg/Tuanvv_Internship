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
    COMMAND ServerNetworkTests firstClientConnection_emitsClientConnected)

add_test(
    NAME ServerNetworkTests.secondClientConnection_isRejectedWhileFirstRemainsConnected
    COMMAND ServerNetworkTests secondClientConnection_isRejectedWhileFirstRemainsConnected)

add_test(
    NAME ServerNetworkTests.newClient_isAcceptedAfterActiveClientDisconnects
    COMMAND ServerNetworkTests newClient_isAcceptedAfterActiveClientDisconnects)

add_test(
    NAME ServerNetworkTests.stop_disconnectsActiveClientAndStopsListening
    COMMAND ServerNetworkTests stop_disconnectsActiveClientAndStopsListening)

add_test(
    NAME ServerNetworkTests.sendFrame_whenDisconnected_returnsFalse
    COMMAND ServerNetworkTests sendFrame_whenDisconnected_returnsFalse)

add_test(
    NAME ServerNetworkTests.sendFrame_whenConnected_clientReceivesCompleteFrame
    COMMAND ServerNetworkTests sendFrame_whenConnected_clientReceivesCompleteFrame
)
add_test(
    NAME ServerNetworkTests.sendFrame_invalidMessageType_returnsFalseAndSendsNoBytes
    COMMAND ServerNetworkTests sendFrame_invalidMessageType_returnsFalseAndSendsNoBytes
)

add_test(
    NAME ServerNetworkTests.sendFrame_payloadTooLarge_returnsFalseAndSendsNoBytes
    COMMAND ServerNetworkTests sendFrame_payloadTooLarge_returnsFalseAndSendsNoBytes
)

add_test(NAME ServerNetworkTests.sendFrame_twoConsecutiveFrames_clientReceivesBothInOrder
    COMMAND ServerNetworkTests sendFrame_twoConsecutiveFrames_clientReceivesBothInOrder
)

add_test(
    NAME ServerNetworkTests.readyRead_completeFrame_emitsFrameReceived
    COMMAND ServerNetworkTests readyRead_completeFrame_emitsFrameReceived
)

add_test(
    NAME ServerNetworkTests.readyRead_partialHeader_waitsForRemainingBytes
    COMMAND ServerNetworkTests readyRead_partialHeader_waitsForRemainingBytes
)

add_test(
    NAME ServerNetworkTests.readyRead_partialPayload_waitsForRemainingBytes
    COMMAND ServerNetworkTests readyRead_partialPayload_waitsForRemainingBytes
)

add_test(
    NAME ServerNetworkTests.readyRead_twoCompleteFrames_emitsBothInOrder
    COMMAND ServerNetworkTests readyRead_twoCompleteFrames_emitsBothInOrder
)

add_test(
    NAME ServerNetworkTests.readyRead_completeFrameAndPartialNextFrame_preservesRemainder
    COMMAND ServerNetworkTests readyRead_completeFrameAndPartialNextFrame_preservesRemainder
)

add_test(
    NAME ServerNetworkTests.readyRead_invalidMagic_emitsProtocolErrorAndFinishesSession
    COMMAND ServerNetworkTests readyRead_invalidMagic_emitsProtocolErrorAndFinishesSession
)
