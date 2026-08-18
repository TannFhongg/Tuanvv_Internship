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
