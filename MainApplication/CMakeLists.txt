qt_add_executable(MainApplication
    src/main.cpp
    src/mainwindow.cpp
    include/mainwindow.h
    Forms/mainwindow.ui
)

target_include_directories(MainApplication
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(MainApplication PROPERTIES
    AUTOUIC_SEARCH_PATHS
        "${CMAKE_CURRENT_SOURCE_DIR}/Forms"
)

add_library(MainCore STATIC
    src/NetworkClient.cpp
    include/NetworkClient.h
)

target_include_directories(MainCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(MainCore
    PUBLIC 
    Qt6::Network
    PRIVATE
    Qt6::Core
    ProtocolCommon
)
target_link_libraries(MainApplication
    PRIVATE
        Qt6::Widgets
        MainCore
)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()