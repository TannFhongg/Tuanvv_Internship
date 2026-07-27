cmake_minimum_required(VERSION 3.20) # yêu cầu CMake version >=3.20 

project(
    cpp_build_system_lab # tên dự án
    VERSION 1.0.0 # bản dự án 
    LANGUAGES CXX # dự án dùng Cpp,Cmake sẽ tự tìm và cấu hình C++ compiler
)

# ---------------------------------------------------------------------------
# Header-only target
# ---------------------------------------------------------------------------
# Target chỉ chứa header
add_library(sys_config INTERFACE) # tạo thư viện không sinh ra .lib .a hay file binh dịch file cpp
#nó chỉ gom các "yêu cầu khi sử dụng" như đường dẫn header,chuẩn c++,compiler otp  

# tạo 1 target link với sys_config, nó sẽ được thêm đường dẫn tìm header
target_include_directories(sys_config
    INTERFACE # các target dùng sys_config sẽ nhận thiết lập này 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> 
    # ${CMAKE_CURRENT_SOURCE_DIR} : thư mục chứa CMakeLists.txt hiện tại 
    # $<BUILD_INTERFACE:...>  # khi build trực tiếp từ source code, thêm đường dẫn (build_system/include)
 
        $<INSTALL_INTERFACE:include>
    # Khi dự án được cài đặt để dự án khác dùng, header được hiểu la nằm trong /include

)

#target nào dùng sys_config cần được biên dịch tối thiểu theo chuẩn c++17
target_compile_features(sys_config
    INTERFACE
        cxx_std_17
)

# ---------------------------------------------------------------------------
# Warning policy
# ---------------------------------------------------------------------------
# tạo ra target ảo để gom tất cả các compiler warning của dự án
#Mục đích là tái sử dụng: target nào link với nó sẽ nhận warning tương ứng.
add_library(project_warnings INTERFACE)


target_compile_options(project_warnings # thêm compiler flags vào các target sử dụng project warning
    INTERFACE
      #nếu compiler là Gcc hoặc clang thì bật
         $<$<CXX_COMPILER_ID:GNU,Clang>: 
            -Wal 
            -Wextra
            -Wpedantic  
            -Wshadow 
        >
#-Wall: các cảnh báo phổ biến.
#-Wextra: thêm cảnh báo hữu ích.
#-Wpedantic: cảnh báo phần code không đúng chuẩn C++ nghiêm ngặt.
#-Wshadow: cảnh báo khi biến cục bộ che khuất biến khác cùng tên.

#nếu compiler là MSVC thì bật mức cảnh cảnh cáo cao /W4
        $<$<CXX_COMPILER_ID:MSVC>:
            /W4
        >
)

# ---------------------------------------------------------------------------
# Static library - Thư viện tĩnh
# ---------------------------------------------------------------------------

#biên dịch src/calculator.cpp thành static library "mathlib" - thường là file .lib 
#thư viện này được nhúng vào executable khi link 
add_library(mathlib STATIC
    src/calculator.cpp
)

#thêm đường dẫn header cho mathlib
target_include_directories(mathlib
    PUBLIC
    # mathlib cần đường dẫn này để biên dịch "calculator.cpp"
    #target nào link với mathlib cũng nhận được đường dẫn này

        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

#mathlib dùng c++17, và excutable/test nào cũng sẽ được yêu cầu hỗ trợ c++17
target_compile_features(mathlib
    PUBLIC
        cxx_std_17
)

#biên dịch nội bộ mathlib nhận các warning từ project_warnings
target_link_libraries(mathlib
    PRIVATE
        project_warnings
)

# ---------------------------------------------------------------------------
# External dependency
# ---------------------------------------------------------------------------

#Dùng để nạp module Cmake FetchContent, dùng để tải dependency lúc cấu hình build 
include(FetchContent)

#<Mô tả dependecy cần tải 
FetchContent_Declare(
    nlohmann_json #tên nội bộ trong cmake
    GIT_REPOSITORY https://github.com/nlohmann/json.git #repo Github của thư viện Json 
    GIT_TAG v3.11.3 # cố định đúng phiên bản, giúp build tái lập được
    GIT_SHALLOW TRUE #shallow clone, chỉ tải lịch sử cần thiết
)

#Tải và thêm thư viện này vào build hiện tại 
#Sau đó link target chính thức nlohmann_json::nlohmann_json
FetchContent_MakeAvailable(nlohmann_json)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------

#biên dịch app/main.cpp thành chương trình thực thi tên app 
add_executable(app
    app/main.cpp
)

#khai báo dependency cho app
target_link_libraries(app
    PRIVATE
        mathlib #dùng các hàm trong thư vện calculator 
        sys_config # nhận đường dẫn header và yêu cầu c++17 từ target header only
        project_warnings # bật warning khi biên dịch app 
        nlohmann_json::nlohmann_json # dùng thư viện json bên ngoài 
)

# ---------------------------------------------------------------------------
# Tests thiết lập kiểm thử
# ---------------------------------------------------------------------------

#Bật tích hợp CTest, tạo option BUILD_TESTIN. mặc đinh ON
include(CTest)

#Chỉ tạo test nếu option BUILD_SETTING ON
if(BUILD_TESTING)
    #tạo excutable test từ source test
    add_executable(calculator_tests
        tests/calculator_tests.cpp
    )

    #test dùng mathlib để kiểm tra tính toán đồng thời nhận các warning
    target_link_libraries(calculator_tests
        PRIVATE
            mathlib
            project_warnings
    )

    #đăng ký executable calculator_tests như 1 test CTest có tên MathlibTests
    add_test(
        NAME MathLibTests
        COMMAND calculator_tests
    )
endif()