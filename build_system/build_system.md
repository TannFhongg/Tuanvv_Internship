# Giai đoạn 5 — Build System, CMake, Makefile và Library

Một build system tốt không chỉ giúp chương trình “compile được”, mà còn phải quản lý chính xác:

- File nguồn và header.
- Dependency giữa các module.
- Compiler flags và linker flags.
- Debug/Release configuration.
- External dependency.
- Test target.
- Incremental build.
- Static/shared library.
- Khả năng tương thích ABI.

---

# 1. Tổng quan quá trình build C++

Một chương trình C++ thường đi qua bốn giai đoạn:

```text
Source code (.cpp, .hpp)
          |
          v
1. Preprocessor
          |
          v
Expanded translation unit
          |
          v
2. Compiler
          |
          v
Assembly
          |
          v
3. Assembler
          |
          v
Object file (.o/.obj)
          |
          v
4. Linker
          |
          v
Executable hoặc Library
```

## 1.1. Preprocessing

Preprocessor xử lý:

- `#include`
- `#define`
- `#if`, `#ifdef`
- Macro expansion

Xem kết quả preprocessing:

```bash
g++ -E main.cpp -o main.ii
```

Header không được compile độc lập theo cách thông thường. Nội dung header được chèn vào từng `.cpp` đã include nó.

Mỗi `.cpp` sau preprocessing trở thành một **translation unit**.

## 1.2. Compilation

Compiler phân tích cú pháp, kiểm tra kiểu và sinh assembly:

```bash
g++ -S main.ii -o main.s
```

## 1.3. Assembly

Assembler chuyển assembly thành object file:

```bash
g++ -c main.s -o main.o
```

Object file thường chứa:

- Machine code.
- Data sections.
- Symbol table.
- Relocation entries.
- Debug information.
- Các symbol chưa được resolve.

## 1.4. Linking

Linker ghép các object file và library:

```bash
g++ main.o calculator.o -o app
```

Linker phải:

- Tìm definition cho các symbol.
- Ghép code và data section.
- Sửa lại địa chỉ thông qua relocation.
- Trích object từ static library.
- Ghi dependency tới shared library.
- Tạo executable cuối cùng.

---

# 2. CMake là gì?

CMake **không phải compiler** và thường cũng không trực tiếp build code.

CMake là một **build-system generator**.

```text
CMakeLists.txt
      |
      v
    CMake
      |
      v
Ninja / Make / Visual Studio / Xcode files
      |
      v
Compiler + Linker
      |
      v
Executable / Library
```

Ví dụ:

```bash
cmake -S . -B build
cmake --build build
```

- Lệnh đầu tiên: configure và generate build files.
- Lệnh thứ hai: gọi backend build tool.

---

# 3. Modern Target-Based CMake

CMake cũ thường dùng:

```cmake
include_directories(...)
link_directories(...)
add_definitions(...)
```

Các lệnh này có thể làm cấu hình lan ra toàn directory scope.

Hậu quả:

- Target nhìn thấy include path không cần thiết.
- Khó biết dependency đến từ đâu.
- Dễ include nhầm header.
- Build graph thiếu rõ ràng.
- Khó tái sử dụng module.

Modern CMake coi executable hoặc library là một **target**.

Một target có thể mang theo:

- Source files.
- Include directories.
- Compile features.
- Compile options.
- Compile definitions.
- Link options.
- Dependency.
- Usage requirements truyền cho consumer.

## 3.1. Các loại target

### Executable

```cmake
add_executable(app
    app/main.cpp
)
```

### Static library

```cmake
add_library(mathlib STATIC
    src/calculator.cpp
)
```

### Shared library

```cmake
add_library(robot SHARED
    src/robot.cpp
)
```

### Interface library

```cmake
add_library(project_warnings INTERFACE)
```

Interface library không tạo binary. Nó thường dùng cho:

- Header-only library.
- Warning flags.
- Feature flags.
- Include paths.
- Compile definitions.

---

# 4. `PRIVATE`, `PUBLIC`, `INTERFACE`

Giả sử:

```text
app  --->  mathlib
```

## 4.1. `PRIVATE`

```cmake
target_link_libraries(mathlib
    PRIVATE
        OpenSSL::Crypto
)
```

Ý nghĩa:

> `mathlib` cần OpenSSL để tự build, nhưng consumer của `mathlib` không cần biết OpenSSL.

Phù hợp khi dependency chỉ xuất hiện trong `.cpp`.

## 4.2. `PUBLIC`

```cmake
target_link_libraries(trajectory
    PUBLIC
        Eigen3::Eigen
)
```

Ý nghĩa:

> Target hiện tại cần dependency và consumer cũng cần dependency.

Ví dụ public header để lộ kiểu Eigen:

```cpp
#include <Eigen/Dense>

Eigen::MatrixXd calculateTrajectory();
```

Target include header này cũng phải hiểu `Eigen::MatrixXd`.

## 4.3. `INTERFACE`

```cmake
add_library(sys_config INTERFACE)

target_include_directories(sys_config
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
```

Ý nghĩa:

> Target hiện tại không tự dùng thuộc tính; thuộc tính chỉ được truyền cho consumer.

## 4.4. Bảng tóm tắt

| Scope | Target hiện tại dùng | Consumer nhận |
|---|---:|---:|
| `PRIVATE` | Có | Không |
| `PUBLIC` | Có | Có |
| `INTERFACE` | Không | Có |

## 4.5. Under the hood: usage requirements

Khi viết:

```cmake
target_include_directories(mathlib PUBLIC include)
```

Có thể hiểu khái niệm như CMake đang lưu:

```text
mathlib.INCLUDE_DIRECTORIES += include
mathlib.INTERFACE_INCLUDE_DIRECTORIES += include
```

Khi `app` link với `mathlib`, CMake truyền các thuộc tính `INTERFACE_*` sang `app`.

Vì vậy:

```cmake
target_link_libraries(app PRIVATE mathlib)
```

không chỉ có nghĩa “thêm `-lmathlib`”. Nó còn có thể truyền:

- Public include directories.
- Public compile definitions.
- Public compile features.
- Transitive link dependencies.

---

# 5. Cấu trúc project đề xuất

```text
cpp_build_system_lab/
├── CMakeLists.txt
├── include/
│   ├── mathlib/
│   │   └── calculator.hpp
│   └── sys_config/
│       └── config.hpp
├── src/
│   └── calculator.cpp
├── app/
│   └── main.cpp
└── tests/
    └── calculator_tests.cpp
```

Public header nên có namespace directory:

```cpp
#include <mathlib/calculator.hpp>
```

Cách này giảm nguy cơ trùng tên header.

---

# 6. `CMakeLists.txt` mẫu hoàn chỉnh

```cmake
cmake_minimum_required(VERSION 3.20)

project(
    cpp_build_system_lab
    VERSION 1.0.0
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Header-only target
# ---------------------------------------------------------------------------

add_library(sys_config INTERFACE)

target_include_directories(sys_config
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(sys_config
    INTERFACE
        cxx_std_17
)

# ---------------------------------------------------------------------------
# Warning policy
# ---------------------------------------------------------------------------

add_library(project_warnings INTERFACE)

target_compile_options(project_warnings
    INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang>:
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
        >
        $<$<CXX_COMPILER_ID:MSVC>:
            /W4
        >
)

# ---------------------------------------------------------------------------
# Static library
# ---------------------------------------------------------------------------

add_library(mathlib STATIC
    src/calculator.cpp
)

target_include_directories(mathlib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(mathlib
    PUBLIC
        cxx_std_17
)

target_link_libraries(mathlib
    PRIVATE
        project_warnings
)

# ---------------------------------------------------------------------------
# External dependency
# ---------------------------------------------------------------------------

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(nlohmann_json)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------

add_executable(app
    app/main.cpp
)

target_link_libraries(app
    PRIVATE
        mathlib
        sys_config
        project_warnings
        nlohmann_json::nlohmann_json
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

include(CTest)

if(BUILD_TESTING)
    add_executable(calculator_tests
        tests/calculator_tests.cpp
    )

    target_link_libraries(calculator_tests
        PRIVATE
            mathlib
            project_warnings
    )

    add_test(
        NAME MathLibTests
        COMMAND calculator_tests
    )
endif()
```

---

# 7. `target_compile_features()`

Cách global:

```cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

Cách target-based:

```cmake
target_compile_features(mathlib PUBLIC cxx_std_17)
```

Ưu điểm:

- Chuẩn C++ gắn trực tiếp với target.
- Consumer có thể kế thừa requirement.
- Không ảnh hưởng target không liên quan.
- Dễ export và tái sử dụng library.

Nếu public header dùng `std::optional`, consumer cũng cần C++17, vì vậy `PUBLIC` là hợp lý.

---

# 8. Generator Expression

Generator Expression có dạng:

```cmake
$<...>
```

Ví dụ theo compiler:

```cmake
$<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
```

Ý nghĩa:

```text
Nếu compiler là GNU hoặc Clang
→ thêm -Wall
```

Ví dụ theo configuration:

```cmake
target_compile_definitions(app
    PRIVATE
        $<$<CONFIG:Debug>:APP_DEBUG>
)
```

## `BUILD_INTERFACE` và `INSTALL_INTERFACE`

```cmake
target_include_directories(mathlib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
```

- `BUILD_INTERFACE`: khi build trong source tree.
- `INSTALL_INTERFACE`: khi library đã được install.

Điều này tránh export đường dẫn tuyệt đối từ máy developer sang máy khác.

---

# 9. `FetchContent`

```cmake
include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(nlohmann_json)
```

Ở lần configure đầu, CMake thường:

1. Tải hoặc clone dependency.
2. Checkout tag/commit.
3. Đưa source dependency vào build tree.
4. Chạy `add_subdirectory()`.
5. Tạo target export của dependency.

Sau đó dùng:

```cmake
nlohmann_json::nlohmann_json
```

## Nên pin version

Không nên:

```cmake
GIT_TAG master
```

Nên:

```cmake
GIT_TAG v3.11.3
```

Hoặc commit hash cụ thể.

Điều này giúp build reproducible hơn.

---

# 10. Out-of-source build

Không nên:

```bash
cmake .
make
```

Nên:

```bash
cmake -S . -B build
cmake --build build
```

Source tree không bị lẫn với:

- Object files.
- Cache.
- Generated files.
- Executable.
- Dependency metadata.

Xóa toàn bộ build state:

```bash
rm -rf build
```

PowerShell:

```powershell
Remove-Item -Recurse -Force build
```

---

# 11. Debug và Release

Single-config generator:

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
```

Build:

```bash
cmake --build build-debug
cmake --build build-release
```

| Configuration | Đặc điểm |
|---|---|
| Debug | Có debug symbols, ít tối ưu |
| Release | Tối ưu cao |
| RelWithDebInfo | Tối ưu và có debug symbols |
| MinSizeRel | Tối ưu kích thước |

Với Visual Studio:

```bash
cmake -S . -B build
cmake --build build --config Release
```

---

# 12. Build và test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Xem command thực tế:

```bash
cmake --build build --verbose
```

Hoặc:

```bash
make VERBOSE=1
ninja -v
```

Đây là bước quan trọng khi debug:

- Thiếu include path.
- Sai macro.
- Sai compiler.
- Sai linker flags.
- Link sai library.
- Sai build configuration.

---

# 13. Makefile và Incremental Build

Make hoạt động dựa trên:

- Dependency graph.
- File timestamp.

Rule:

```make
target: prerequisites
	recipe
```

Ví dụ:

```make
main.o: main.cpp
	g++ -c main.cpp -o main.o
```

Make rebuild khi:

```text
target không tồn tại
hoặc
prerequisite mới hơn target
```

## Under the hood

Filesystem lưu modification time (`mtime`).

Make chủ yếu so sánh timestamp thay vì hash toàn bộ nội dung.

Ưu điểm:

- Nhanh.
- Đơn giản.

Hạn chế:

- Timestamp sai có thể làm incremental build sai.
- Đồng hồ lệch có thể gây `Clock skew detected`.
- Dependency thiếu khiến code cũ không được rebuild.

---

# 14. Biến tự động trong Makefile

| Biến | Ý nghĩa |
|---|---|
| `$@` | Target hiện tại |
| `$<` | Prerequisite đầu tiên |
| `$^` | Toàn bộ prerequisites |
| `$?` | Prerequisites mới hơn target |
| `$*` | Stem của pattern rule |
| `$(@D)` | Directory của target |

Ví dụ:

```make
build/src/main.o: src/main.cpp
	g++ -c $< -o $@
```

Khi đó:

```text
$< = src/main.cpp
$@ = build/src/main.o
$(@D) = build/src
```

---

# 15. Pattern rule và `.PHONY`

Pattern rule:

```make
$(BUILD_DIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
```

`.PHONY`:

```make
.PHONY: all clean test
```

Nếu không khai báo `.PHONY`, một file thật tên `clean` có thể khiến:

```bash
make clean
```

không chạy recipe.

---

# 16. Header dependency: `-MMD -MP`

Rule đơn giản:

```make
main.o: main.cpp
	g++ -c main.cpp -o main.o
```

Không nói cho Make biết `main.o` còn phụ thuộc header:

```cpp
#include <mathlib/calculator.hpp>
```

Dependency thật:

```text
main.o
├── main.cpp
└── include/mathlib/calculator.hpp
```

Nếu sửa header mà dependency không được theo dõi, object có thể không rebuild.

## `-MMD`

Compiler tự sinh file `.d`:

```bash
g++ -MMD -MP -c main.cpp -o main.o
```

Ví dụ `main.d`:

```make
main.o: main.cpp include/mathlib/calculator.hpp
include/mathlib/calculator.hpp:
```

## `-MP`

Tạo dummy rule cho header, giúp tránh lỗi khi header cũ bị xóa hoặc đổi tên.

## Include dependency files

```make
-include $(DEPS)
```

Dấu `-` nghĩa là không báo lỗi nếu `.d` chưa tồn tại ở lần build đầu.

---

# 17. Makefile mẫu

```make
CXX       := g++
CPPFLAGS  := -Iinclude
CXXFLAGS  := -std=c++17 -Wall -Wextra -Wpedantic -MMD -MP
LDFLAGS   :=
LDLIBS    :=

BUILD_DIR := build
SRC_DIR   := src
APP_DIR   := app

SRCS := 	$(wildcard $(SRC_DIR)/*.cpp) 	$(wildcard $(APP_DIR)/*.cpp)

OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

TARGET := $(BUILD_DIR)/app_exec

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CXX) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@
	@echo "Built successfully: $@"

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
```

## Phân biệt flags

| Biến | Mục đích |
|---|---|
| `CPPFLAGS` | `-I`, `-D` |
| `CXXFLAGS` | Chuẩn C++, warning, optimization |
| `LDFLAGS` | Linker flags |
| `LDLIBS` | Library như `-lpthread` |

---

# 18. Static Library

Linux:

```text
libmathlib.a
```

Windows:

```text
mathlib.lib
```

Tạo thủ công:

```bash
g++ -c calculator.cpp -o calculator.o
ar rcs libmathlib.a calculator.o
```

Xem object bên trong:

```bash
ar t libmathlib.a
```

Link:

```bash
g++ main.o -L. -lmathlib -o app
```

## Under the hood

Static library là archive chứa object files.

Giả sử:

```text
libmathlib.a
├── add.o
├── subtract.o
└── matrix.o
```

Nếu app cần `add()`, linker có thể chỉ trích `add.o`.

Granularity thường là object file, không phải từng function.

Muốn loại bỏ function/data không dùng:

```bash
-ffunction-sections
-fdata-sections
-Wl,--gc-sections
```

Điều này đặc biệt quan trọng với firmware giới hạn Flash.

---

# 19. Shared Library

Linux:

```text
librobot.so
```

Windows:

```text
robot.dll
```

Build:

```bash
g++ -fPIC -c robot.cpp -o robot.o
g++ -shared robot.o -o librobot.so
```

Link app:

```bash
g++ main.cpp -L. -lrobot -o app
```

Kiểm tra dependency:

```bash
ldd ./app
readelf -d ./app
```

---

# 20. Dynamic Loader hoạt động thế nào?

Khi app Linux chạy, kernel thường:

1. Map executable vào virtual memory.
2. Đọc interpreter của executable.
3. Chuyển điều khiển cho dynamic loader.
4. Loader tìm các `.so`.
5. Map library vào memory.
6. Thực hiện relocation.
7. Resolve symbol.
8. Chạy initialization.
9. Chuyển tới entry point của app.

Loader tìm library qua các nguồn như:

- `LD_LIBRARY_PATH`.
- `RUNPATH`.
- `/etc/ld.so.cache`.
- `/lib`, `/usr/lib`.

---

# 21. PLT và GOT

Hai cấu trúc quan trọng:

- **PLT**: Procedure Linkage Table.
- **GOT**: Global Offset Table.

Khái niệm:

```text
App gọi foo()
   |
   v
PLT entry
   |
   v
GOT chứa địa chỉ thật
   |
   v
foo() trong shared library
```

Với lazy binding, lần gọi đầu có thể đi qua resolver. Resolver tìm symbol và ghi địa chỉ vào GOT. Các lần sau gọi nhanh hơn qua địa chỉ đã được lưu.

---

# 22. Vì sao shared library tiết kiệm RAM?

Ba process cùng dùng OpenCV:

```text
camera_process
slam_process
detector_process
```

Các code page read-only của shared library có thể được kernel map chung:

```text
Process A virtual page ─┐
Process B virtual page ─┼──> Same physical code page
Process C virtual page ─┘
```

Tuy nhiên không nên hiểu rằng toàn bộ library luôn chỉ tồn tại đúng một bản.

Phần writable, relocation và dữ liệu riêng của process có thể không được chia sẻ giống code read-only.

---

# 23. `-fPIC` và ASLR

`PIC` là Position Independent Code.

Shared library có thể được load vào các địa chỉ khác nhau.

Compiler sinh code truy cập qua:

- PC-relative addressing.
- GOT.
- Relative offsets.

```bash
g++ -fPIC -c robot.cpp -o robot.o
```

ASLR làm địa chỉ load thay đổi để tăng bảo mật. PIC giúp code hoạt động dù vị trí load không cố định.

---

# 24. Static và Shared: so sánh

| Tiêu chí | Static | Shared |
|---|---|---|
| Ghép code | Link time | Load/runtime |
| Code nằm trong executable | Có | Không hoàn toàn |
| Kích thước executable | Lớn hơn | Nhỏ hơn |
| Dependency ngoài khi chạy | Ít | Có |
| Chia sẻ code giữa process | Không | Có thể |
| Update độc lập | Khó | Có thể |
| Rủi ro ABI | Thấp hơn sau khi link | Cao hơn |
| Bare-metal | Phù hợp | Thường không có loader |
| Plugin architecture | Khó | Phù hợp |

---

# 25. ABI — Application Binary Interface

API mô tả cách source code tương tác.

ABI mô tả cách binary tương tác.

ABI bao gồm:

- Symbol name.
- Calling convention.
- Register usage.
- Stack layout.
- Object layout.
- Padding và alignment.
- Vtable.
- RTTI.
- Exception ABI.
- Standard library ABI.

Hai phiên bản có thể API-compatible nhưng không ABI-compatible.

---

# 26. Name Mangling

C++ hỗ trợ overload:

```cpp
int add(int, int);
double add(double, double);
```

Compiler encode kiểu vào symbol:

```text
_Z3addii
_Z3adddd
```

Xem:

```bash
nm library.o
nm -C library.o
```

## `extern "C"`

```cpp
extern "C" int robot_initialize();
```

Nó yêu cầu C linkage và thường tắt C++ name mangling.

Nhưng nó không:

- Biến code C++ thành C.
- Tự làm class C++ ABI-stable.
- Cho phép exception C++ đi qua C boundary an toàn.
- Làm STL type trở thành C-compatible.

Mẫu opaque handle:

```cpp
extern "C" {

struct RobotHandle;

RobotHandle* robot_create();
void robot_destroy(RobotHandle* robot);
int robot_set_speed(RobotHandle* robot, double speed);

}
```

---

# 27. Memory layout và padding

```cpp
struct Robot
{
    int id;
    double speed;
};
```

Không nên mặc định:

```text
4 + 8 = 12 bytes
```

Do alignment, layout thường có thể là:

```text
id       : 4 bytes
padding  : 4 bytes
speed    : 8 bytes
-------------------
total    : 16 bytes
```

Kiểm tra:

```cpp
std::cout << sizeof(Robot) << '\n';
std::cout << alignof(Robot) << '\n';
```

Nếu app và `.so` không đồng ý về layout, có thể xảy ra memory corruption hoặc segmentation fault.

---

# 28. Vtable và ABI

Class có virtual function thường chứa hidden `vptr`.

```cpp
class Robot
{
public:
    virtual ~Robot() = default;
    virtual void update();
};
```

ABI có thể phụ thuộc:

- Thứ tự virtual functions.
- Multiple inheritance.
- Virtual inheritance.
- RTTI.
- Compiler ABI.

Thêm virtual function vào giữa danh sách có thể đổi vtable slot, khiến binary cũ gọi nhầm function.

---

# 29. Các thay đổi dễ phá ABI

- Thêm/xóa data member.
- Đổi thứ tự member.
- Đổi kiểu member.
- Thêm hoặc đổi thứ tự virtual function.
- Đổi base class.
- Đổi inheritance.
- Đổi compiler hoặc ABI flags.
- Đổi standard library ABI.
- Đổi alignment/packing.
- Public STL type qua module boundary.
- Macro làm class definition khác nhau giữa app và library.

---

# 30. Cách giảm rủi ro ABI

## C API boundary

```cpp
extern "C" RobotHandle* robot_create();
```

## Opaque pointer

Không để lộ class layout trong public header.

## PImpl

```cpp
class Robot
{
public:
    Robot();
    ~Robot();

    Robot(Robot&&) noexcept;
    Robot& operator=(Robot&&) noexcept;

    void update();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
```

Public layout chủ yếu chỉ chứa pointer; implementation có thể thay đổi trong `Impl`.

## Rebuild dependency

Nếu project không cam kết ABI ổn định, cách an toàn là:

> Thay public C++ interface thì rebuild toàn bộ binary phụ thuộc.

---

# 31. Symbol và linker errors

Object file có thể chứa:

```text
U calculate
T main
```

Trong `nm`:

- `U`: undefined symbol.
- `T`: code/text symbol.
- `D`: initialized data.
- `B`: BSS.
- `W`: weak symbol.

Compile có thể thành công nhưng link thất bại:

```text
undefined reference to `add(int, int)'
```

Compiler chỉ cần declaration; linker cần definition.

---

# 32. Link order của static library

Thường đúng:

```bash
g++ main.o -lmathlib -o app
```

Có thể sai:

```bash
g++ -lmathlib main.o -o app
```

Trên nhiều Unix linker, library được scan theo thứ tự. Khi library xuất hiện trước object cần symbol, linker có thể chưa biết phải trích object nào.

---

# 33. One Definition Rule

Sai:

```cpp
// header.hpp
int globalCounter = 0;
```

Header được include ở nhiều `.cpp` có thể tạo multiple definitions.

Cách sửa:

```cpp
// header.hpp
extern int globalCounter;
```

```cpp
// source.cpp
int globalCounter = 0;
```

Hoặc C++17:

```cpp
inline int globalCounter = 0;
```

---

# 34. Tối ưu thời gian build

## 34.1. Giảm dependency từ header

Sửa public header có thể làm hàng trăm translation unit rebuild.

Sửa `.cpp` thường chỉ rebuild object tương ứng.

Giải pháp:

- Forward declaration khi phù hợp.
- PImpl.
- Không include header nặng không cần thiết.
- Tách interface và implementation.

## 34.2. Precompiled Header

```cmake
target_precompile_headers(app
    PRIVATE
        <vector>
        <string>
        <memory>
)
```

Hữu ích với header nặng, ổn định.

## 34.3. Unity Build

```cmake
set_target_properties(app
    PROPERTIES
        UNITY_BUILD ON
)
```

Gộp nhiều `.cpp` để giảm overhead compiler, nhưng có thể gây collision và tăng phạm vi rebuild.

## 34.4. Compiler cache

```bash
cmake -S . -B build   -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
```

`ccache` có thể tái sử dụng object đã compile khi input tương ứng không đổi.

## 34.5. Ninja

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Ninja thường nhanh và có output gọn với project lớn.

---

# 35. Checklist

## CMake

- [ ] Dùng target-based commands.
- [ ] Hiểu `PRIVATE`, `PUBLIC`, `INTERFACE`.
- [ ] Tránh global `include_directories()`.
- [ ] Dùng imported target như `Package::Target`.
- [ ] Pin version dependency.
- [ ] Dùng out-of-source build.
- [ ] Tách Debug/Release.
- [ ] Tích hợp CTest.
- [ ] Xem verbose command khi build lỗi.

## Makefile

- [ ] Hiểu target, prerequisite, recipe.
- [ ] Recipe bắt đầu bằng Tab.
- [ ] Dùng `$@`, `$<`, `$^`.
- [ ] Khai báo `.PHONY`.
- [ ] Dùng `-MMD -MP`.
- [ ] Include `.d`.
- [ ] Tách object vào build directory.
- [ ] Phân biệt compile flags và link flags.

## Library và ABI

- [ ] Biết static library chứa object files.
- [ ] Biết shared library được loader map khi chạy.
- [ ] Hiểu `-fPIC`.
- [ ] Biết PLT/GOT ở mức khái niệm.
- [ ] Phân biệt API và ABI.
- [ ] Hiểu name mangling.
- [ ] Hiểu padding, alignment và class layout.
- [ ] Không thay `.so` tùy tiện khi ABI đã đổi.
- [ ] Cân nhắc C API, opaque handle hoặc PImpl.

---

# 36. Bài tập thực hành

## Bài 1 — Target-based CMake

Tạo project có:

- `mathlib`: static library.
- `config`: interface library.
- `app`: executable.
- `mathlib_tests`: test target.
- `nlohmann/json`: FetchContent.

Không dùng global `include_directories()`.

## Bài 2 — Quan sát incremental build

1. Build lần đầu.
2. Sửa một `.cpp`.
3. Quan sát chỉ object tương ứng rebuild.
4. Sửa public header.
5. Quan sát nhiều translation unit rebuild.
6. Chạy verbose build.

## Bài 3 — Static library

```bash
g++ -c calculator.cpp -o calculator.o
ar rcs libcalculator.a calculator.o
g++ main.cpp -L. -lcalculator -o app
```

Phân tích:

```bash
ar t libcalculator.a
nm -C libcalculator.a
```

## Bài 4 — Shared library

```bash
g++ -fPIC -c robot.cpp -o robot.o
g++ -shared robot.o -o librobot.so
g++ main.cpp -L. -lrobot -o app
```

Phân tích:

```bash
ldd ./app
readelf -d ./app
nm -D -C librobot.so
```

## Bài 5 — Quan sát ABI break

Phiên bản 1:

```cpp
class Robot
{
public:
    int id;
};
```

Phiên bản 2:

```cpp
class Robot
{
public:
    int id;
    double speed;
};
```

So sánh:

- `sizeof(Robot)`.
- `alignof(Robot)`.
- Offset member.
- App cũ và library mới hiểu object layout khác nhau thế nào.

> Chỉ thực hiện trong project thử nghiệm.

---

# 37. Tóm tắt

1. CMake sinh build system; compiler mới là công cụ dịch code.
2. Modern CMake quản lý dependency theo target.
3. `PRIVATE`, `PUBLIC`, `INTERFACE` điều khiển usage requirements.
4. Make incremental build chủ yếu dựa trên dependency graph và timestamp.
5. `-MMD -MP` giúp theo dõi header dependency.
6. Static library là archive của object files.
7. Shared library được dynamic loader map khi chạy.
8. PLT/GOT hỗ trợ gọi symbol trong shared library.
9. `-fPIC` cho phép code không phụ thuộc địa chỉ load cố định.
10. API compatibility không đảm bảo ABI compatibility.
11. Class layout, vtable và name mangling là nguồn ABI break phổ biến.
12. C API, opaque handle và PImpl giúp giảm rủi ro ABI.
