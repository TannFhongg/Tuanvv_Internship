add_executable(ServerNetworkTests
    tcpservertest.cpp
)
target_link_libraries(ServerNetworkTests PRIVATE
    ServerCore
    Qt6::Test
    Qt6::Network
)

add_executable(LicenseRepositoryTest
    licenserepositorytest.cpp
)

add_executable(LicenseManagerTest
    licensemanagertest.cpp
)

target_link_libraries(LicenseRepositoryTest PRIVATE
    ServerCore
    Qt6::Test
    Qt6::Core
)

target_link_libraries(LicenseManagerTest PRIVATE
    ServerCore
    Qt6::Test
    Qt6::Core
)

add_test(
    NAME LicenseManagerTest
    COMMAND LicenseManagerTest
)

add_test(
    NAME LicenseRepositoryTest
    COMMAND LicenseRepositoryTest
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

add_test(
    NAME LicenseManagerTest.initialize_missingRepository_succeeds
    COMMAND LicenseManagerTest initialize_missingRepository_succeeds
)

add_test(
    NAME LicenseManagerTest.initialize_malformedRepository_failsAndRemainsUninitialized
    COMMAND LicenseManagerTest initialize_malformedRepository_failsAndRemainsUninitialized
)

add_test(
    NAME LicenseManagerTest.authenticate_beforeInitialization_fails
    COMMAND LicenseManagerTest authenticate_beforeInitialization_fails
)

add_test(
    NAME LicenseManagerTest.authenticate_blankRequiredField_fails
    COMMAND LicenseManagerTest authenticate_blankRequiredField_fails
)

add_test(
    NAME LicenseManagerTest.authenticate_unknownProductKey_returnsInvalidKey
    COMMAND LicenseManagerTest authenticate_unknownProductKey_returnsInvalidKey
)

add_test(
    NAME LicenseManagerTest.authenticate_unboundLicense_bindsDeviceAndReturnsValid
    COMMAND LicenseManagerTest authenticate_unboundLicense_bindsDeviceAndReturnsValid
)

add_test(
    NAME LicenseManagerTest.authenticate_boundLicenseWithSameDevice_returnsValid
    COMMAND LicenseManagerTest authenticate_boundLicenseWithSameDevice_returnsValid
)

add_test(
    NAME LicenseManagerTest.authenticate_boundLicenseWithDifferentDevice_returnsDeviceMismatch
    COMMAND LicenseManagerTest authenticate_boundLicenseWithDifferentDevice_returnsDeviceMismatch
)

add_test(
    NAME LicenseManagerTest.authenticate_unboundLicensePersistenceFails_returnsOperationFailure
    COMMAND LicenseManagerTest authenticate_unboundLicensePersistenceFails_returnsOperationFailure
)

add_test(
    NAME LicenseManagerTest.createLicense_beforeInitialization_fails
    COMMAND LicenseManagerTest createLicense_beforeInitialization_fails
)

add_test(
    NAME LicenseManagerTest.createLicense_afterInitialization_persistsEnabledUnboundLicense
    COMMAND LicenseManagerTest createLicense_afterInitialization_persistsEnabledUnboundLicense
)

add_test(
    NAME LicenseManagerTest.createLicense_twice_generatesDistinctPersistedKeys
    COMMAND LicenseManagerTest createLicense_twice_generatesDistinctPersistedKeys
)

add_test(
    NAME LicenseManagerTest.createLicense_persistenceFails_returnsFailureWithoutProductKey
    COMMAND LicenseManagerTest createLicense_persistenceFails_returnsFailureWithoutProductKey
)