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

add_executable(ApplicationControllerTest
    applicationcontrollertest.cpp
)

target_link_libraries(ApplicationControllerTest
    PRIVATE
    Qt6::Test
    MainCore
)

add_executable(MainWindowTest
    mainwindowtest.cpp
    ../src/mainwindow.cpp
    ../include/mainwindow.h
    ../Forms/mainwindow.ui
)

target_include_directories(MainWindowTest
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

set_target_properties(MainWindowTest PROPERTIES
    AUTOUIC_SEARCH_PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/../Forms"
)

target_link_libraries(MainWindowTest
    PRIVATE
    Qt6::Test
    Qt6::Widgets
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
    NAME ApplicationControllerTest
    COMMAND ApplicationControllerTest)

add_test(
    NAME MainWindowTest
    COMMAND MainWindowTest)

add_test(
    NAME MainWindowTest.initialState_showsActivationPageAndDisablesFileFeatures
    COMMAND MainWindowTest initialState_showsActivationPageAndDisablesFileFeatures)

add_test(
    NAME MainWindowTest.connectButton_validEndpoint_connectsAndEnablesActivationControls
    COMMAND MainWindowTest connectButton_validEndpoint_connectsAndEnablesActivationControls)

add_test(
    NAME MainWindowTest.disconnectButton_whenConnected_disconnectsAndRestoresLockedUi
    COMMAND MainWindowTest disconnectButton_whenConnected_disconnectsAndRestoresLockedUi)

add_test(
    NAME MainWindowTest.activateButton_validCredentials_sendsAuthenticationAndShowsPendingState
    COMMAND MainWindowTest activateButton_validCredentials_sendsAuthenticationAndShowsPendingState)

add_test(
    NAME MainWindowTest.authenticateResponse_valid_opensFileFeaturesAndKeepsSessionConnected
    COMMAND MainWindowTest authenticateResponse_valid_opensFileFeaturesAndKeepsSessionConnected)

add_test(
    NAME MainWindowTest.authenticateResponse_nonValidStatus_staysLockedShowsErrorAndAllowsRetry
    COMMAND MainWindowTest authenticateResponse_nonValidStatus_staysLockedShowsErrorAndAllowsRetry)

add_test(
    NAME MainWindowTest.errorResponse_duringAuthentication_showsRemoteErrorAndAllowsRetry
    COMMAND MainWindowTest errorResponse_duringAuthentication_showsRemoteErrorAndAllowsRetry)

add_test(
    NAME MainWindowTest.authenticateResponse_malformedPayload_showsInvalidResponseErrorAndAllowsRetry
    COMMAND MainWindowTest authenticateResponse_malformedPayload_showsInvalidResponseErrorAndAllowsRetry)

add_test(
    NAME MainWindowTest.authenticationRequest_whenDeadlineExpires_showsTimeoutAndAllowsRetry
    COMMAND MainWindowTest authenticationRequest_whenDeadlineExpires_showsTimeoutAndAllowsRetry)

add_test(
    NAME MainWindowTest.disconnectButton_afterActivation_closesFileFeaturesAndReturnsToActivationPage
    COMMAND MainWindowTest disconnectButton_afterActivation_closesFileFeaturesAndReturnsToActivationPage)

add_test(
    NAME MainWindowTest.activateButton_blankRequiredField_showsValidationErrorWithoutSendingFrame
    COMMAND MainWindowTest activateButton_blankRequiredField_showsValidationErrorWithoutSendingFrame)

add_test(
    NAME MainWindowTest.activate_afterInvalidKey_retriesWithEditedKeyAndOpensFilePageWithoutReconnect
    COMMAND MainWindowTest activate_afterInvalidKey_retriesWithEditedKeyAndOpensFilePageWithoutReconnect)

add_test(
    NAME MainWindowTest.connectButton_closedEndpoint_showsConnectionErrorAndAllowsRetry
    COMMAND MainWindowTest connectButton_closedEndpoint_showsConnectionErrorAndAllowsRetry)

add_test(
    NAME ApplicationControllerTest.initialState_isLockedAndFeatureAccessDenied
    COMMAND ApplicationControllerTest initialState_isLockedAndFeatureAccessDenied)

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

add_test(
    NAME ApplicationControllerTest.activate_whenDisconnected_returnsNotConnectedAndRemainsLocked
    COMMAND ApplicationControllerTest activate_whenDisconnected_returnsNotConnectedAndRemainsLocked)

add_test(
    NAME ApplicationControllerTest.activate_whenConnected_sendsAuthenticateRequestAndEntersAuthenticating
    COMMAND ApplicationControllerTest activate_whenConnected_sendsAuthenticateRequestAndEntersAuthenticating)

add_test(
    NAME ApplicationControllerTest.authenticateResponse_valid_transitionsToActiveAndAllowsFeatureAccess
    COMMAND ApplicationControllerTest authenticateResponse_valid_transitionsToActiveAndAllowsFeatureAccess)

add_test(
    NAME ApplicationControllerTest.authenticateResponse_nonValidStatus_returnsLockedAndDeniesFeatureAccess
    COMMAND ApplicationControllerTest authenticateResponse_nonValidStatus_returnsLockedAndDeniesFeatureAccess)

add_test(
    NAME ApplicationControllerTest.errorResponse_duringAuthentication_returnsLockedAndReportsRemoteError
    COMMAND ApplicationControllerTest errorResponse_duringAuthentication_returnsLockedAndReportsRemoteError)

add_test(
    NAME ApplicationControllerTest.authenticateResponse_malformedPayload_returnsLockedAndReportsInvalidResponsePayload
    COMMAND ApplicationControllerTest authenticateResponse_malformedPayload_returnsLockedAndReportsInvalidResponsePayload)

add_test(
    NAME ApplicationControllerTest.serverDisconnect_afterActivation_returnsLockedAndDeniesFeatureAccess
    COMMAND ApplicationControllerTest serverDisconnect_afterActivation_returnsLockedAndDeniesFeatureAccess)

add_test(
    NAME ApplicationControllerTest.authenticationRequest_whenDeadlineExpires_returnsLockedAndReportsRequestTimeout
    COMMAND ApplicationControllerTest authenticationRequest_whenDeadlineExpires_returnsLockedAndReportsRequestTimeout)

add_test(
    NAME ApplicationControllerTest.activate_afterInvalidKey_canRetryAndBecomeActiveWithoutReconnect
    COMMAND ApplicationControllerTest activate_afterInvalidKey_canRetryAndBecomeActiveWithoutReconnect)

add_test(
    NAME ApplicationControllerTest.activate_whileAuthenticationPending_returnsInvalidRequestWithoutSendingSecondFrame
    COMMAND ApplicationControllerTest activate_whileAuthenticationPending_returnsInvalidRequestWithoutSendingSecondFrame)

add_test(
    NAME ApplicationControllerTest.activate_blankRequiredField_returnsInvalidRequestWithoutSendingFrame
    COMMAND ApplicationControllerTest activate_blankRequiredField_returnsInvalidRequestWithoutSendingFrame)

add_test(
    NAME ApplicationControllerTest.serverDisconnect_whileAuthenticating_returnsLockedAndReportsConnectionLostOnce
    COMMAND ApplicationControllerTest serverDisconnect_whileAuthenticating_returnsLockedAndReportsConnectionLostOnce)

add_test(
    NAME ApplicationControllerTest.reconnect_afterActivation_requiresAndAcceptsNewAuthentication
    COMMAND ApplicationControllerTest reconnect_afterActivation_requiresAndAcceptsNewAuthentication)

add_test(
    NAME ApplicationControllerTest.connectionLifecycle_connectThenDisconnect_reportsStateAndKeepsFeaturesLocked
    COMMAND ApplicationControllerTest connectionLifecycle_connectThenDisconnect_reportsStateAndKeepsFeaturesLocked)

