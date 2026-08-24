qt_add_executable(NetworkIntegrationTests
    networkintegrationtest.cpp
)

target_link_libraries(NetworkIntegrationTests
    PRIVATE
        MainCore
        ServerCore
        ProtocolCommon
        Qt6::Test
        Qt6::Network
)

add_test(
    NAME NetworkIntegrationTests
    COMMAND NetworkIntegrationTests
)
