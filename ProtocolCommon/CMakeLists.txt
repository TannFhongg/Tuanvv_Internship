
add_library(ProtocolCommon STATIC)

target_sources(ProtocolCommon
    PRIVATE
        src/protocolcommon.cpp
        src/protocolcodec.cpp

        include/protocolconstants.h
        include/protocoltypes.h
        include/protocolheader.h
        include/protocolcodec.h
)


target_include_directories(ProtocolCommon
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(ProtocolCommon
    PUBLIC
        Qt6::Core
) 
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif() 

