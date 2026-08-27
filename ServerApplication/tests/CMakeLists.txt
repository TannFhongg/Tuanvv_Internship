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

target_link_libraries(LicenseRepositoryTest PRIVATE
    ServerCore
    Qt6::Test
    Qt6::Core
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
    NAME LicenseRepositoryTest.load_missingFile_succeedsWithEmptyRepository
    COMMAND LicenseRepositoryTest load_missingFile_succeedsWithEmptyRepository
)

add_test(
    NAME LicenseRepositoryTest.load_validSingleRecord_populatesRepository
    COMMAND LicenseRepositoryTest load_validSingleRecord_populatesRepository
)

add_test(
    NAME LicenseRepositoryTest.load_malformedJson_failsWithoutChangingExistingRecords
    COMMAND LicenseRepositoryTest load_malformedJson_failsWithoutChangingExistingRecords
)


add_test(
    NAME LicenseRepositoryTest.load_invalidRootSchema_failsWithoutChangingExistingRecords
    COMMAND LicenseRepositoryTest load_invalidRootSchema_failsWithoutChangingExistingRecords
)

add_test(
    NAME LicenseRepositoryTest.load_invalidLicenseRecord_failsWithoutChangingExistingRecords
    COMMAND LicenseRepositoryTest load_invalidLicenseRecord_failsWithoutChangingExistingRecords
)

add_test(
    NAME LicenseRepositoryTest.load_duplicateProductKey_failsWithoutChangingExistingRecords
    COMMAND LicenseRepositoryTest load_duplicateProductKey_failsWithoutChangingExistingRecords
)

add_test(
    NAME LicenseRepositoryTest.load_emptyLicenseArray_replacesExistingRepositoryWithEmpty
    COMMAND LicenseRepositoryTest load_emptyLicenseArray_replacesExistingRepositoryWithEmpty)

add_test(
    NAME LicenseRepositoryTest.load_extraFieldsAndUnboundLicense_succeeds
    COMMAND LicenseRepositoryTest load_extraFieldsAndUnboundLicense_succeeds
)

add_test(
    NAME LicenseRepositoryTest.insert_validRecord_persistsAndCanBeReloaded
    COMMAND LicenseRepositoryTest insert_validRecord_persistsAndCanBeReloaded
)

add_test(
    NAME LicenseRepositoryTest.insert_duplicateProductKey_failsWithoutChangingRepositoryOrFile
    COMMAND LicenseRepositoryTest insert_duplicateProductKey_failsWithoutChangingRepositoryOrFile
)

add_test(
    NAME LicenseRepositoryTest.insert_emptyProductKey_failsWithoutCreatingFile
    COMMAND LicenseRepositoryTest insert_emptyProductKey_failsWithoutCreatingFile
)

add_test(
    NAME LicenseRepositoryTest.insert_fileOpenFails_doesNotChangeMemory
    COMMAND LicenseRepositoryTest insert_fileOpenFails_doesNotChangeMemory
)

add_test(
    NAME LicenseRepositoryTest.update_existingRecord_persistsAndCanBeReloaded
    COMMAND LicenseRepositoryTest update_existingRecord_persistsAndCanBeReloaded
)

add_test(
    NAME LicenseRepositoryTest.update_invalidTarget_failsWithoutChangingRepositoryOrFile
    COMMAND LicenseRepositoryTest update_invalidTarget_failsWithoutChangingRepositoryOrFile
)

add_test(
    NAME LicenseRepositoryTest.insert_secondRecord_preservesExistingRecordAfterReload
    COMMAND LicenseRepositoryTest insert_secondRecord_preservesExistingRecordAfterReload
)

add_test(
    NAME LicenseRepositoryTest.update_oneRecord_preservesOtherRecordAfterReload
    COMMAND LicenseRepositoryTest update_oneRecord_preservesOtherRecordAfterReload
)

add_test(
    NAME LicenseRepositoryTest.update_fileOpenFails_doesNotChangeMemoryOrPersistedRecord
    COMMAND LicenseRepositoryTest update_fileOpenFails_doesNotChangeMemoryOrPersistedRecord
)
