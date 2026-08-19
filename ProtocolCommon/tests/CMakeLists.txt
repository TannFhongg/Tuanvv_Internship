add_executable(ProtocolCommonTests
    protocolcodectest.cpp
)
target_link_libraries(ProtocolCommonTests
    PRIVATE
    ProtocolCommon
    Qt6::Test
)

add_test(NAME Protocol.encode
    COMMAND ProtocolCommonTests encode)

add_test(NAME Protocol.testBigEndian
    COMMAND ProtocolCommonTests testBigEndian)

add_test(NAME Protocol.deserialize_validHeader_returnsSuccess
    COMMAND ProtocolCommonTests deserialize_validHeader_returnsSuccess)

add_test(NAME Protocol.deserialize_lessThanFourBytes_returnsNeedMoreData
    COMMAND ProtocolCommonTests deserialize_lessThanFourBytes_returnsNeedMoreData)

add_test(NAME Protocol.deserialize_validMagicButIncomplete_returnsNeedMoreData
    COMMAND ProtocolCommonTests deserialize_validMagicButIncomplete_returnsNeedMoreData)

add_test(NAME Protocol.deserialize_invalidMagic_returnsInvalidFrame
    COMMAND ProtocolCommonTests deserialize_invalidMagic_returnsInvalidFrame)

add_test(NAME Protocol.deserialize_unsupportedVersion_returnsError
    COMMAND ProtocolCommonTests deserialize_unsupportedVersion_returnsError)

add_test(NAME Protocol.deserialize_unknownMessageType_returnsError
    COMMAND ProtocolCommonTests deserialize_unknownMessageType_returnsError)

add_test(NAME Protocol.deserialize_payloadTooLarge_returnsError
    COMMAND ProtocolCommonTests deserialize_payloadTooLarge_returnsError)

add_test(NAME Protocol.deserialize_headerWithFollowingPayload_returnsSuccess
    COMMAND ProtocolCommonTests deserialize_headerWithFollowingPayload_returnsSuccess)