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

target_link_libraries(MainApplication
    PRIVATE
        Qt6::Widgets
        Qt6::Network
        ProtocolCommon
)
