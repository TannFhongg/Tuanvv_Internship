add_executable(ProtocolCommonTests
    protocolcodectest.cpp
)
add_executable(FrameParserTests
    frameparsertest.cpp
)
add_executable(ErrorResponseCodecTests
    errorresponsetest.cpp
)

add_executable(AuthenticationCodecTests
    authenticationcodectest.cpp
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

target_link_libraries(ErrorResponseCodecTests
    PRIVATE
    ProtocolCommon
    Qt6::Test
)

target_link_libraries(AuthenticationCodecTests
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

add_test(NAME FrameParser.tryTakeFrame_invalidMagic_returnsFailedAndClearsBuffer
    COMMAND FrameParserTests tryTakeFrame_invalidMagic_returnsFailedAndClearsBuffer)

add_test(NAME FrameParser.tryTakeFrame_zeroLengthPayload_returnsFrameReady
    COMMAND FrameParserTests tryTakeFrame_zeroLengthPayload_returnsFrameReady)

add_test(NAME FrameParser.clear_bufferedData_removesAllBytes
    COMMAND FrameParserTests clear_bufferedData_removesAllBytes)

add_test(NAME Protocol.serializeFrame_validInput_returnsSuccess
    COMMAND ProtocolCommonTests serializeFrame_validInput_returnsSuccess)

add_test(NAME Protocol.serializeFrame_zeroLengthPayload_returnsSuccess
    COMMAND ProtocolCommonTests serializeFrame_zeroLengthPayload_returnsSuccess)

add_test(NAME Protocol.serializeFrame_invalidMessageType_returnsFailed
    COMMAND ProtocolCommonTests serializeFrame_invalidMessageType_returnsFailed)

add_test(NAME Protocol.serializeFrame_unknownMessageType_returnsFailed
    COMMAND ProtocolCommonTests serializeFrame_unknownMessageType_returnsFailed)

add_test(NAME Protocol.serializeFrame_payloadTooLarge_returnsFailed
    COMMAND ProtocolCommonTests serializeFrame_payloadTooLarge_returnsFailed)

add_test(NAME Protocol.serializeFrame_maxPayloadSize_returnsSuccess
    COMMAND ProtocolCommonTests serializeFrame_maxPayloadSize_returnsSuccess)

add_test(NAME ErrorResponseCodecTests.errorResponse_validData_roundTripsSuccessfully
    COMMAND ErrorResponseCodecTests errorResponse_validData_roundTripsSuccessfully)

add_test(
    NAME ErrorResponseCodecTests.serializeErrorResponse_noneErrorCode_returnsFailed
    COMMAND ErrorResponseCodecTests serializeErrorResponse_noneErrorCode_returnsFailed)

add_test(
    NAME ErrorResponseCodecTests.deserializeErrorResponse_malformedJson_returnsFailed
    COMMAND ErrorResponseCodecTests deserializeErrorResponse_malformedJson_returnsFailed)

add_test(
    NAME ErrorResponseCodecTests.deserializeErrorResponse_jsonArray_returnsFailed
    COMMAND ErrorResponseCodecTests deserializeErrorResponse_jsonArray_returnsFailed)

add_test(
    NAME ErrorResponseCodecTests.deserializeErrorResponse_missingErrorCode_returnsFailed
    COMMAND ErrorResponseCodecTests deserializeErrorResponse_missingErrorCode_returnsFailed)

add_test(
    NAME ErrorResponseCodecTests.deserializeErrorResponse_errorCodeWrongType_returnsFailed
    COMMAND ErrorResponseCodecTests deserializeErrorResponse_errorCodeWrongType_returnsFailed)

add_test(
    NAME ErrorResponseCodecTests.deserializeErrorResponse_unknownErrorCode_returnsFailed
    COMMAND ErrorResponseCodecTests deserializeErrorResponse_unknownErrorCode_returnsFailed)

add_test(
    NAME ErrorResponseCodecTests.deserializeErrorResponse_noneErrorCode_returnsFailed
    COMMAND ErrorResponseCodecTests deserializeErrorResponse_noneErrorCode_returnsFailed)

add_test(
    NAME ErrorResponseCodecTests.deserializeErrorResponse_missingMessage_returnsFailed
    COMMAND ErrorResponseCodecTests deserializeErrorResponse_missingMessage_returnsFailed)

add_test(
    NAME ErrorResponseCodecTests.deserializeErrorResponse_messageWrongType_returnsFailed
    COMMAND ErrorResponseCodecTests deserializeErrorResponse_messageWrongType_returnsFailed)

add_test(
    NAME ErrorResponseCodecTests.deserializeErrorResponse_missingDetails_returnsFailed
    COMMAND ErrorResponseCodecTests deserializeErrorResponse_missingDetails_returnsFailed)

add_test(
    NAME ErrorResponseCodecTests.deserializeErrorResponse_detailsWrongType_returnsFailed
    COMMAND ErrorResponseCodecTests deserializeErrorResponse_detailsWrongType_returnsFailed)

add_test(
    NAME ErrorResponseCodecTests.deserializeErrorResponse_fractionalErrorCode_returnsFailed
    COMMAND ErrorResponseCodecTests deserializeErrorResponse_fractionalErrorCode_returnsFailed)

add_test(
    NAME AuthenticationCodecTests
    COMMAND AuthenticationCodecTests)

add_test(NAME AuthenticationCodecTests.authenticateRequest_validData_roundTripsSuccessfully
    COMMAND AuthenticationCodecTests authenticateRequest_validData_roundTripsSuccessfully)

add_test(
    NAME AuthenticationCodecTests.authenticateResponse_validStatuses_roundTrip
    COMMAND AuthenticationCodecTests authenticateResponse_validStatuses_roundTrip)

add_test(
    NAME AuthenticationCodecTests.authenticateResponse_invalidStatuses_serializeFails
    COMMAND AuthenticationCodecTests authenticateResponse_invalidStatuses_serializeFails)

add_test(
    NAME AuthenticationCodecTests.authenticateResponse_invalidWireStatuses_deserializeFails
    COMMAND AuthenticationCodecTests authenticateResponse_invalidWireStatuses_deserializeFails)

add_test(
    NAME AuthenticationCodecTests.authenticateRequest_emptyRequiredField_serializeFails
    COMMAND AuthenticationCodecTests authenticateRequest_emptyRequiredField_serializeFails)

add_test(
    NAME AuthenticationCodecTests.authenticateRequest_invalidPayload_deserializeFails
    COMMAND AuthenticationCodecTests authenticateRequest_invalidPayload_deserializeFails)

add_test(
    NAME AuthenticationCodecTests.authenticateResponse_invalidPayload_deserializeFails
    COMMAND AuthenticationCodecTests authenticateResponse_invalidPayload_deserializeFails)

add_test(
    NAME AuthenticationCodecTests.authenticateRequest_extraFields_areIgnored
    COMMAND AuthenticationCodecTests authenticateRequest_extraFields_areIgnored)

add_test(
    NAME AuthenticationCodecTests.authenticateResponse_extraFields_areIgnored
    COMMAND AuthenticationCodecTests authenticateResponse_extraFields_areIgnored)
