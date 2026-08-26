add_executable(ClientNetworkTests
    networkclienttest.cpp
)

target_link_libraries(ClientNetworkTests
    PRIVATE
    Qt6::Test
    MainCore
)

add_executable(RequestDispatcherTests
    requestdispatchertest.cpp
)

target_link_libraries(RequestDispatcherTests
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

add_test(
    NAME ClientNetworkTests.connectToServer_whenAlreadyConnected_returnsFalse
    COMMAND ClientNetworkTests connectToServer_whenAlreadyConnected_returnsFalse)

add_test(
    NAME ClientNetworkTests.connectToServer_closedPort_emitsConnectionError
    COMMAND ClientNetworkTests connectToServer_closedPort_emitsConnectionError)

add_test(
    NAME ClientNetworkTests.connectToServer_afterFailure_canConnectSuccessfully
    COMMAND ClientNetworkTests connectToServer_afterFailure_canConnectSuccessfully)

add_test(
    NAME ClientNetworkTests.serverDisconnect_connectedClient_emitsDisconnected
    COMMAND ClientNetworkTests serverDisconnect_connectedClient_emitsDisconnected)

add_test(
    NAME ClientNetworkTests.sendFrame_whenDisconnected_returnsFalse
    COMMAND ClientNetworkTests sendFrame_whenDisconnected_returnsFalse)

add_test(
    NAME ClientNetworkTests.sendFrame_whenConnected_serverReceivesCompleteFrame
    COMMAND ClientNetworkTests sendFrame_whenConnected_serverReceivesCompleteFrame)

add_test(
    NAME ClientNetworkTests.sendFrame_invalidMessageType_returnsFalseAndSendsNoBytes
    COMMAND ClientNetworkTests sendFrame_invalidMessageType_returnsFalseAndSendsNoBytes)

add_test(
    NAME ClientNetworkTests.sendFrame_payloadTooLarge_returnsFalseAndSendsNoBytes
    COMMAND ClientNetworkTests sendFrame_payloadTooLarge_returnsFalseAndSendsNoBytes)

add_test(
    NAME ClientNetworkTests.sendFrame_payloadAtMaximumSize_serverReceivesCompleteFrame
    COMMAND ClientNetworkTests sendFrame_payloadAtMaximumSize_serverReceivesCompleteFrame)

add_test(
    NAME ClientNetworkTests.sendFrame_twoConsecutiveFrames_serverReceivesBothInOrder
    COMMAND ClientNetworkTests sendFrame_twoConsecutiveFrames_serverReceivesBothInOrder)

add_test(
    NAME ClientNetworkTests.readyRead_completeFrame_emitsFrameReceived
    COMMAND ClientNetworkTests readyRead_completeFrame_emitsFrameReceived)

add_test(
    NAME ClientNetworkTests.readyRead_partialHeader_waitsForRemainingBytes
    COMMAND ClientNetworkTests readyRead_partialHeader_waitsForRemainingBytes)

add_test(
    NAME ClientNetworkTests.readyRead_partialPayload_waitsForRemainingBytes
    COMMAND ClientNetworkTests readyRead_partialPayload_waitsForRemainingBytes)

add_test(
    NAME ClientNetworkTests.readyRead_twoCompleteFrames_emitsBothInOrder
    COMMAND ClientNetworkTests readyRead_twoCompleteFrames_emitsBothInOrder)

add_test(
    NAME ClientNetworkTests.readyRead_completeFrameAndPartialNextFrame_preservesRemainder
    COMMAND ClientNetworkTests readyRead_completeFrameAndPartialNextFrame_preservesRemainder)

add_test(
    NAME ClientNetworkTests.readyRead_invalidMagic_emitsProtocolErrorAndDisconnects
    COMMAND ClientNetworkTests readyRead_invalidMagic_emitsProtocolErrorAndDisconnects)

add_test(
    NAME ClientNetworkTests.disconnectWithPartialFrame_thenReconnect_doesNotReuseOldBuffer
    COMMAND ClientNetworkTests disconnectWithPartialFrame_thenReconnect_doesNotReuseOldBuffer)

add_test(
    NAME RequestDispatcherTests
    COMMAND RequestDispatcherTests)

add_test(
    NAME RequestDispatcherTests.sendRequest_invalidMessageType_returnsFailedWithoutPending
    COMMAND RequestDispatcherTests sendRequest_invalidMessageType_returnsFailedWithoutPending)

add_test(
    NAME RequestDispatcherTests.sendRequest_invalidDestination_returnsFailedWithoutPending
    COMMAND RequestDispatcherTests sendRequest_invalidDestination_returnsFailedWithoutPending)

add_test(
    NAME RequestDispatcherTests.sendRequest_whenDisconnected_returnsNotConnectedWithoutPending
    COMMAND RequestDispatcherTests sendRequest_whenDisconnected_returnsNotConnectedWithoutPending)

add_test(
    NAME RequestDispatcherTests.sendRequest_whenConnected_returnsAcceptedAndCreatesPending
    COMMAND RequestDispatcherTests sendRequest_whenConnected_returnsAcceptedAndCreatesPending)

add_test(
    NAME RequestDispatcherTests.sendRequest_twoRequests_generatesDistinctIdsAndTracksBoth
    COMMAND RequestDispatcherTests sendRequest_twoRequests_generatesDistinctIdsAndTracksBoth)

add_test(
    NAME RequestDispatcherTests.response_matchingRequest_removesPendingAndEmitsResponse
    COMMAND RequestDispatcherTests response_matchingRequest_removesPendingAndEmitsResponse)

add_test(
    NAME RequestDispatcherTests.response_unknownRequestId_isIgnoredAndKeepsPending
    COMMAND RequestDispatcherTests response_unknownRequestId_isIgnoredAndKeepsPending)

add_test(
    NAME RequestDispatcherTests.response_wrongMessageType_removesPendingAndEmitsResponseTypeMismatch
    COMMAND RequestDispatcherTests response_wrongMessageType_removesPendingAndEmitsResponseTypeMismatch)

add_test(
    NAME RequestDispatcherTests.errorResponse_matchingRequest_removesPendingAndEmitsRemoteError
    COMMAND RequestDispatcherTests errorResponse_matchingRequest_removesPendingAndEmitsRemoteError)

add_test(
    NAME RequestDispatcherTests.errorResponse_malformedPayload_removesPendingAndEmitsInvalidResponsePayload
    COMMAND RequestDispatcherTests errorResponse_malformedPayload_removesPendingAndEmitsInvalidResponsePayload)

add_test(
    NAME RequestDispatcherTests.disconnect_withPendingRequests_clearsAllAndEmitsConnectionLostForEach
    COMMAND RequestDispatcherTests disconnect_withPendingRequests_clearsAllAndEmitsConnectionLostForEach)

add_test(
    NAME RequestDispatcherTests.pendingRequest_whenDeadlineExpires_emitsRequestTimeout
    COMMAND RequestDispatcherTests pendingRequest_whenDeadlineExpires_emitsRequestTimeout)

add_test(
    NAME RequestDispatcherTests.response_afterTimeout_isIgnoredAndDoesNotCompleteTwice
    COMMAND RequestDispatcherTests response_afterTimeout_isIgnoredAndDoesNotCompleteTwice) 

add_test(
    NAME RequestDispatcherTests.response_beforeDeadline_doesNotEmitTimeoutLater
    COMMAND RequestDispatcherTests response_beforeDeadline_doesNotEmitTimeoutLater)

add_test(
    NAME RequestDispatcherTests.responses_outOfOrder_areCorrelatedByRequestId
    COMMAND RequestDispatcherTests responses_outOfOrder_areCorrelatedByRequestId)