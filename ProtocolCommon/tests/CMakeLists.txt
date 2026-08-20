add_executable(ProtocolCommonTests
    protocolcodectest.cpp
)
add_executable(FrameParserTests
    frameparsertest.cpp
)
target_link_libraries(ProtocolCommonTests
    PRIVATE
    ProtocolCommon
    Qt6::Test
)
target_link_libraries(FrameParserTests
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

add_test(NAME FrameParser.tryTakeFrame_emptyBuffer_returnsNeedMoreData
    COMMAND FrameParserTests tryTakeFrame_emptyBuffer_returnsNeedMoreData)

add_test(NAME FrameParser.tryTakeFrame_partialHeader_returnsNeedMoreData
    COMMAND FrameParserTests tryTakeFrame_partialHeader_returnsNeedMoreData)

add_test(NAME FrameParser.tryTakeFrame_partialPayload_returnsNeedMoreData
    COMMAND FrameParserTests tryTakeFrame_partialPayload_returnsNeedMoreData)

add_test(NAME FrameParser.tryTakeFrame_completeFrame_returnsFrameReady
    COMMAND FrameParserTests tryTakeFrame_completeFrame_returnsFrameReady)

add_test(NAME FrameParser.tryTakeFrame_fragmentedFrame_returnsReadyAfterRemainderAppended
    COMMAND FrameParserTests tryTakeFrame_fragmentedFrame_returnsReadyAfterRemainderAppended)

add_test(NAME FrameParser.tryTakeFrame_twoCompleteFrames_returnsOneAtATime
    COMMAND FrameParserTests tryTakeFrame_twoCompleteFrames_returnsOneAtATime)

add_test(NAME FrameParser.tryTakeFrame_completeFrameAndPartialNext_preservesPartialNextFrame
    COMMAND FrameParserTests tryTakeFrame_completeFrameAndPartialNext_preservesPartialNextFrame)