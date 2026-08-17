
add_library(ProtocolCommon STATIC)

target_sources(ProtocolCommon
    PRIVATE
        src/protocolcommon.cpp
        include/protocolcommon.h
        include/protocolconstants.h
        include/protocoltypes.h
        include/protocolheader.h
)

target_include_directories(ProtocolCommon
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(ProtocolCommon
    PUBLIC
        Qt6::Core
)
