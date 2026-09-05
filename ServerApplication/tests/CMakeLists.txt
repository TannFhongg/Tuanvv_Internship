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

add_executable(FileManagerTests
    filemanagertest.cpp
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

target_link_libraries(FileManagerTests PRIVATE
    ServerCore
    Qt6::Test
    Qt6::Core
)

add_executable(ServerRequestDispatcherTest
    serverrequestdispatchertest.cpp
)

target_link_libraries(ServerRequestDispatcherTest PRIVATE
    ServerCore
    Qt6::Test
    Qt6::Network
)

add_executable(ServerControllerTest
    servercontrollertest.cpp
)

target_link_libraries(ServerControllerTest PRIVATE
    ServerCore
    Qt6::Test
    Qt6::Network
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
    NAME FileManagerTest.browseRoot_emptyStorage_returnsEmptyEntries
    COMMAND FileManagerTests
            browseRoot_emptyStorage_returnsEmptyEntries
)

add_test(
    NAME FileManagerTest.browseRoot_multipleEntries_returnsAllMetadataInDeterministicOrder
    COMMAND FileManagerTests
            browseRoot_multipleEntries_returnsAllMetadataInDeterministicOrder
)

add_test(
    NAME FileManagerTest.browseNestedDirectory_returnsOnlyDirectChildrenWithLogicalPaths
    COMMAND FileManagerTests
            browseNestedDirectory_returnsOnlyDirectChildrenWithLogicalPaths
)

add_test(
    NAME FileManagerTest.browsePathTraversal_failsWithoutExposingOutsideStorage
    COMMAND FileManagerTests
            browsePathTraversal_failsWithoutExposingOutsideStorage
)

add_test(
    NAME FileManagerTest.browseNonCanonicalLogicalPath_failsWithoutMutation
    COMMAND FileManagerTests
            browseNonCanonicalLogicalPath_failsWithoutMutation
)

add_test(
    NAME FileManagerTest.browseMissingOrFilePath_failsWithoutMutation
    COMMAND FileManagerTests
            browseMissingOrFilePath_failsWithoutMutation
)

add_test(
    NAME FileManagerTest.createDirectory_underExistingDirectory_createsAndReturnsLogicalPath
    COMMAND FileManagerTests
            createDirectory_underExistingDirectory_createsAndReturnsLogicalPath
)

add_test(
    NAME FileManagerTest.createDirectory_invalidPathOrName_failsWithoutMutation
    COMMAND FileManagerTests
            createDirectory_invalidPathOrName_failsWithoutMutation
)

add_test(
    NAME FileManagerTest.createDirectory_atRoot_createsAndReturnsRootChild
    COMMAND FileManagerTests
            createDirectory_atRoot_createsAndReturnsRootChild
)

add_test(
    NAME FileManagerTest.createDirectory_missingOrFileParent_failsWithoutMutation
    COMMAND FileManagerTests
            createDirectory_missingOrFileParent_failsWithoutMutation
)

add_test(
    NAME FileManagerTest.createDirectory_existingFileOrDirectory_failsWithoutOverwriting
    COMMAND FileManagerTests
            createDirectory_existingFileOrDirectory_failsWithoutOverwriting
)

add_test(
    NAME ServerNetworkTests
    COMMAND ServerNetworkTests
)

add_test(
    NAME ServerControllerTest
    COMMAND ServerControllerTest
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
    NAME ServerNetworkTests.acceptedClientFrame_isForwardedByTcpServerWithSessionIdentity
    COMMAND ServerNetworkTests acceptedClientFrame_isForwardedByTcpServerWithSessionIdentity
)

add_test(
    NAME ServerControllerTest.validAuthentication_overTcp_returnsCorrelatedValidResponse
    COMMAND ServerControllerTest validAuthentication_overTcp_returnsCorrelatedValidResponse
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
    NAME LicenseManagerTest.createLicense_afterInitialization_generatesCanonicalProductKeyFormat
    COMMAND LicenseManagerTest createLicense_afterInitialization_generatesCanonicalProductKeyFormat
)

add_test(
    NAME LicenseManagerTest.createLicense_generatedKeyCollision_retriesAndPreservesLeadingZeroes
    COMMAND LicenseManagerTest createLicense_generatedKeyCollision_retriesAndPreservesLeadingZeroes
)

add_test(
    NAME LicenseManagerTest.createLicense_twice_generatesDistinctPersistedKeys
    COMMAND LicenseManagerTest createLicense_twice_generatesDistinctPersistedKeys
)

add_test(
    NAME LicenseManagerTest.createLicense_persistenceFails_returnsFailureWithoutProductKey
    COMMAND LicenseManagerTest createLicense_persistenceFails_returnsFailureWithoutProductKey
)

add_test(
    NAME LicenseManagerTest.disableLicense_existingEnabledLicense_persistsDisabledState
    COMMAND LicenseManagerTest disableLicense_existingEnabledLicense_persistsDisabledState
)

add_test(
    NAME LicenseManagerTest.disableLicense_beforeInitialization_fails
    COMMAND LicenseManagerTest disableLicense_beforeInitialization_fails
)

add_test(
    NAME LicenseManagerTest.disableLicense_blankProductKey_fails
    COMMAND LicenseManagerTest disableLicense_blankProductKey_fails
)

add_test(
    NAME LicenseManagerTest.disableLicense_unknownProductKey_failsWithoutMutation
    COMMAND LicenseManagerTest disableLicense_unknownProductKey_failsWithoutMutation
)

add_test(
    NAME LicenseManagerTest.disableLicense_alreadyDisabledLicense_succeedsWithoutRewrite
    COMMAND LicenseManagerTest disableLicense_alreadyDisabledLicense_succeedsWithoutRewrite
)

add_test(
    NAME LicenseManagerTest.disableLicense_persistenceFails_returnsFailureWithoutChangingState
    COMMAND LicenseManagerTest disableLicense_persistenceFails_returnsFailureWithoutChangingState
)

add_test(
    NAME LicenseManagerTest.enableLicense_existingDisabledLicense_persistsEnabledState
    COMMAND LicenseManagerTest enableLicense_existingDisabledLicense_persistsEnabledState
)

add_test(
    NAME LicenseManagerTest.enableLicense_alreadyEnabledLicense_succeedsWithoutRewrite
    COMMAND LicenseManagerTest enableLicense_alreadyEnabledLicense_succeedsWithoutRewrite
)

add_test(
    NAME LicenseManagerTest.enableLicense_persistenceFails_returnsFailureWithoutChangingState
    COMMAND LicenseManagerTest enableLicense_persistenceFails_returnsFailureWithoutChangingState
)

add_test(
    NAME LicenseManagerTest.authenticate_afterDisableAndReEnable_reflectsCurrentLicenseState
    COMMAND LicenseManagerTest authenticate_afterDisableAndReEnable_reflectsCurrentLicenseState
)

add_test(
    NAME LicenseManagerTest.authenticate_disabledLicense_returnsDisabledBeforeDeviceCheck
    COMMAND LicenseManagerTest authenticate_disabledLicense_returnsDisabledBeforeDeviceCheck
)

add_test(
    NAME LicenseManagerTest.listLicenses_beforeInitialization_fails
    COMMAND LicenseManagerTest listLicenses_beforeInitialization_fails
)

add_test(
    NAME LicenseManagerTest.listLicenses_emptyRepository_returnsEmptyListWithoutMutation
    COMMAND LicenseManagerTest listLicenses_emptyRepository_returnsEmptyListWithoutMutation
)

add_test(
    NAME LicenseManagerTest.listLicenses_multipleRecords_returnsAllFieldsInDeterministicOrderWithoutRewrite
    COMMAND LicenseManagerTest listLicenses_multipleRecords_returnsAllFieldsInDeterministicOrderWithoutRewrite
)

add_test(
    NAME ServerNetworkTests.newSession_startsUnauthenticated
    COMMAND ServerNetworkTests newSession_startsUnauthenticated
)

add_test(
    NAME ServerRequestDispatcherTest.authenticateRequest_validBoundLicense_returnsCorrelatedValidResponseAndAuthenticatesSession
    COMMAND ServerRequestDispatcherTest authenticateRequest_validBoundLicense_returnsCorrelatedValidResponseAndAuthenticatesSession
)

add_test(
    NAME ServerRequestDispatcherTest.authenticateRequest_unknownProductKey_returnsInvalidKeyWithoutAuthenticatingSession
    COMMAND ServerRequestDispatcherTest authenticateRequest_unknownProductKey_returnsInvalidKeyWithoutAuthenticatingSession
)

add_test(
    NAME ServerRequestDispatcherTest.authenticateRequest_malformedPayload_returnsCorrelatedInvalidRequestWithoutAuthenticatingSession
    COMMAND ServerRequestDispatcherTest authenticateRequest_malformedPayload_returnsCorrelatedInvalidRequestWithoutAuthenticatingSession
)

add_test(
    NAME ServerRequestDispatcherTest.fileChunk_beforeAuthentication_returnsCorrelatedAuthenticationFailedAndKeepsSessionLocked
    COMMAND ServerRequestDispatcherTest fileChunk_beforeAuthentication_returnsCorrelatedAuthenticationFailedAndKeepsSessionLocked
)

add_test(
    NAME ServerRequestDispatcherTest.reconnect_afterAuthenticatedSession_startsLockedAndRequiresAuthenticationAgain
    COMMAND ServerRequestDispatcherTest reconnect_afterAuthenticatedSession_startsLockedAndRequiresAuthenticationAgain
)

add_test(
    NAME ServerRequestDispatcherTest.authenticateRequest_disabledLicense_returnsDisabledBeforeDeviceCheckWithoutAuthenticatingSession
    COMMAND ServerRequestDispatcherTest authenticateRequest_disabledLicense_returnsDisabledBeforeDeviceCheckWithoutAuthenticatingSession
)

add_test(
    NAME ServerRequestDispatcherTest.authenticateRequest_boundLicenseWithDifferentDevice_returnsDeviceMismatchWithoutAuthenticatingSession
    COMMAND ServerRequestDispatcherTest authenticateRequest_boundLicenseWithDifferentDevice_returnsDeviceMismatchWithoutAuthenticatingSession
)

add_test(NAME ServerRequestDispatcherTest.authenticateRequest_unboundLicense_bindsDeviceReturnsValidAndAuthenticatesSession
    COMMAND ServerRequestDispatcherTest authenticateRequest_unboundLicense_bindsDeviceReturnsValidAndAuthenticatesSession
)

add_test(
    NAME ServerRequestDispatcherTest
    COMMAND ServerRequestDispatcherTest
)

add_test(
    NAME ServerRequestDispatcherTest.authenticateRequest_unboundLicensePersistenceFails_returnsCorrelatedInternalServerErrorWithoutAuthenticatingSession
    COMMAND ServerRequestDispatcherTest
    authenticateRequest_unboundLicensePersistenceFails_returnsCorrelatedInternalServerErrorWithoutAuthenticatingSession
)
