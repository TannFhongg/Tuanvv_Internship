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
    src/licenserepository.cpp
    src/licensemanager.cpp
    src/filemanager.cpp
    src/ServerRequestDispatcher.cpp
    src/servercontroller.cpp

    include/licensemanager.h
    include/TcpServer.h
    include/ClientSession.h
    include/licenserepository.h
    include/licenserecord.h
    include/filemanager.h
    include/ServerRequestDispatcher.h
    include/servercontroller.h
)
target_include_directories(ServerCore
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(ServerCore
    PUBLIC
    Qt6::Core
    Qt6::Network
    ProtocolCommon
)

target_link_libraries(ServerApplication
    PRIVATE
    Qt6::Widgets
    ServerCore
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
