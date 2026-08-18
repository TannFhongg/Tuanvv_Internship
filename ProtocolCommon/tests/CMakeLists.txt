add_executable(ProtocolCommonTests
    protocolcodectest.cpp
)
target_link_libraries(ProtocolCommonTests
    PRIVATE
    ProtocolCommon
    Qt6::Test
)

add_test(NAME Protocol.encode_and_BigEndian
    COMMAND ProtocolCommonTests encode_and_BigEndian)

