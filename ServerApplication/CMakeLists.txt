qt_add_executable(ServerApplication
    src/main.cpp
    src/serverwindow.cpp

    include/serverwindow.h
    Forms/serverwindow.ui
)

target_include_directories(ServerApplication
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(ServerApplication PROPERTIES
    AUTOUIC_SEARCH_PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/Forms"
)
add_library(ServerCore STATIC
    src/TcpServer.cpp
    src/ClientSession.cpp

    include/TcpServer.h
    include/ClientSession.h
)
target_include_directories(ServerCore
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(ServerCore
    PUBLIC
    Qt6::Network
    ProtocolCommon
    PRIVATE
    Qt6::Core
)

target_link_libraries(ServerApplication
    PRIVATE
    Qt6::Widgets
    ServerCore
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()