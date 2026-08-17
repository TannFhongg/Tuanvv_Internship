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


target_link_libraries(ServerApplication
    PRIVATE
    Qt6::Widgets
    Qt6::Network
    ProtocolCommon
)