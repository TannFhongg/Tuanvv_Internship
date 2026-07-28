# Static Library và Dynamic Library trong C++ — Under the Hood

Khi dự án lớn, ta thường không compile toàn bộ source code thành một file executable duy nhất ngay từ đầu. Thay vào đó, code được chia thành các **library** để tái sử dụng.

Có hai loại chính:

- **Static library**: thư viện được nhúng vào executable lúc link.
- **Dynamic library**: thư viện được nạp khi chương trình chạy hoặc lúc khởi động.

---

## 1. Quy trình build cơ bản

Giả sử có:

```cpp
// math_utils.h
#pragma once

int add(int a, int b);
```

```cpp
// math_utils.cpp
#include "math_utils.h"

int add(int a, int b)
{
    return a + b;
}
```

```cpp
// main.cpp
#include <iostream>
#include "math_utils.h"

int main()
{
    std::cout << add(10, 20) << '\n';
}
```

Quy trình build:

```text
Source code
    ↓
Preprocessor
    ↓
Compiler
    ↓
Object file
    ↓
Linker
    ↓
Executable
```

Cụ thể:

```text
main.cpp       → main.o
math_utils.cpp → math_utils.o

main.o + math_utils.o → executable
```

Trên Windows, object file thường là:

```text
main.obj
math_utils.obj
```

Trên Linux:

```text
main.o
math_utils.o
```

Library thực chất là một cách đóng gói các object file.

---

## 2. Static Library là gì?

Static library là tập hợp các object file được đóng gói thành một file thư viện.

Tên file thường là:

| Hệ điều hành | Static library |
|---|---|
| Linux | `libmath.a` |
| macOS | `libmath.a` |
| Windows MSVC | `math.lib` |
| Windows MinGW | `libmath.a` |

Ví dụ:

```text
math_utils.o
vector_utils.o
string_utils.o
        ↓
     libutils.a
```

Khi build executable:

```text
main.o + libutils.a
        ↓ linker
      application
```

Linker sẽ lấy code cần thiết trong static library và **copy vào executable**.

---

## 3. Static library hoạt động under the hood

Giả sử static library chứa:

```cpp
int add(int a, int b);
int subtract(int a, int b);
int multiply(int a, int b);
```

Nhưng `main.cpp` chỉ gọi:

```cpp
add(10, 20);
```

Static library có thể chứa:

```text
libmath.a
├── add.o
├── subtract.o
└── multiply.o
```

Khi linker xử lý:

```text
main.o có undefined symbol: add(int, int)
```

Linker tìm trong `libmath.a`:

```text
add.o định nghĩa add(int, int)
```

Sau đó linker đưa `add.o` vào executable.

Thông thường, linker không nhất thiết copy toàn bộ library; nó lấy các **object module cần thiết** để giải quyết symbol chưa được định nghĩa.

Kết quả:

```text
application
├── main()
└── add()
```

`subtract()` và `multiply()` có thể không được đưa vào executable nếu chúng nằm trong object file riêng và không được sử dụng.

---

## 4. Static library không được nạp khi chạy

Sau khi link xong:

```text
application
```

đã chứa code của library bên trong.

Khi chạy:

```bash
./application
```

hệ điều hành không cần tìm `libmath.a`.

File `.a` hoặc `.lib` chỉ cần ở **thời điểm build**.

Ví dụ:

```text
Build time:
main.o + libmath.a → application

Run time:
application chạy độc lập
```

Do đó, sau khi build xong, bạn có thể xóa static library mà executable vẫn chạy được.

---

## 5. Tạo static library bằng GCC

Compile source thành object file:

```bash
g++ -c math_utils.cpp -o math_utils.o
```

Tạo static library:

```bash
ar rcs libmath.a math_utils.o
```

Ý nghĩa:

```text
ar = archive tool
r  = insert hoặc replace object file
c  = create archive
s  = tạo symbol index
```

Compile `main.cpp`:

```bash
g++ -c main.cpp -o main.o
```

Link:

```bash
g++ main.o -L. -lmath -o app
```

Trong đó:

```text
-L.       tìm library trong thư mục hiện tại
-lmath    tìm libmath.a hoặc libmath.so
```

Quy tắc đặt tên của GCC:

```text
-lmath
```

sẽ tìm:

```text
libmath.a
libmath.so
```

chứ không tìm file tên `math.a`.

---

## 6. Static linking và symbol resolution

Trong `main.o`, lời gọi:

```cpp
add(10, 20);
```

chưa chứa địa chỉ thật của hàm `add`.

Object file chỉ chứa một symbol chưa giải quyết:

```text
Undefined symbol: _Z3addii
```

Tên `_Z3addii` là kết quả của **name mangling** trong C++.

Bạn có thể kiểm tra bằng:

```bash
nm main.o
```

Ví dụ:

```text
U _Z3addii
```

`U` nghĩa là undefined.

Trong `math_utils.o`:

```bash
nm math_utils.o
```

có thể cho:

```text
T _Z3addii
```

`T` nghĩa là symbol được định nghĩa trong text/code section.

Linker ghép hai phần:

```text
main.o:
U _Z3addii

math_utils.o:
T _Z3addii
```

Sau khi link:

```text
undefined symbol đã được resolve
```

Executable biết chính xác địa chỉ của `add()`.

---

## 7. Link order của static library

Thứ tự link có thể quan trọng trên GCC/Linux.

Lệnh đúng:

```bash
g++ main.o -lmath -o app
```

Lệnh này đôi khi sai:

```bash
g++ -lmath main.o -o app
```

Lý do linker thường xử lý từ trái sang phải.

Khi gặp `-lmath` trước:

```text
Chưa có undefined symbol nào cần giải quyết
→ không lấy add.o
```

Sau đó gặp `main.o`:

```text
main.o cần add()
→ nhưng linker đã đi qua libmath.a
```

Kết quả:

```text
undefined reference to `add(int, int)'
```

Quy tắc thực tế:

```text
Object cần library → đặt trước
Library cung cấp symbol → đặt sau
```

```bash
g++ main.o -lmath
```

---

## 8. Dynamic Library là gì?

Dynamic library không được copy toàn bộ vào executable khi link.

Tên file thường là:

| Hệ điều hành | Dynamic library |
|---|---|
| Linux | `libmath.so` |
| macOS | `libmath.dylib` |
| Windows | `math.dll` |

Executable chỉ chứa:

- thông tin library cần dùng;
- danh sách symbol cần resolve;
- cơ chế để dynamic loader tìm library;
- các bảng phục vụ gọi hàm động.

Ví dụ:

```text
app
├── main()
├── dependency: libmath.so
└── undefined dynamic symbol: add()
```

Khi chạy:

```text
app
    ↓
OS dynamic loader
    ↓
load libmath.so vào memory
    ↓
resolve add()
    ↓
chạy chương trình
```

---

## 9. Dynamic library under the hood

Khi chạy executable trên Linux, kernel không trực tiếp tự resolve toàn bộ library.

Trong executable ELF có thông tin về **program interpreter**, ví dụ:

```text
/lib64/ld-linux-x86-64.so.2
```

Kernel sẽ:

1. Map executable vào virtual memory.
2. Map dynamic loader.
3. Chuyển quyền điều khiển cho dynamic loader.
4. Dynamic loader đọc danh sách dependency.
5. Tìm các file `.so`.
6. Map các `.so` vào process.
7. Resolve symbol.
8. Thực hiện relocation.
9. Gọi code khởi tạo.
10. Cuối cùng chạy `main()`.

Luồng đơn giản:

```text
Kernel
  ↓
ELF executable
  ↓
Dynamic loader: ld-linux
  ↓
libstdc++.so
libc.so
libmath.so
  ↓
resolve symbols
  ↓
main()
```

---

## 10. Dynamic library được map vào virtual memory

Dynamic library không đơn giản được “copy” vào process.

Hệ điều hành sử dụng cơ chế:

```text
memory mapping
```

Ví dụ:

```text
Process virtual memory
┌──────────────────────────┐
│ Stack                    │
├──────────────────────────┤
│ Shared libraries         │
│ libmath.so               │
│ libc.so                  │
│ libstdc++.so             │
├──────────────────────────┤
│ Heap                     │
├──────────────────────────┤
│ Executable data          │
├──────────────────────────┤
│ Executable code          │
└──────────────────────────┘
```

Phần code chỉ đọc của dynamic library có thể được chia sẻ giữa nhiều process.

Ví dụ:

```text
Process A ─┐
           ├── cùng map code pages của libmath.so
Process B ─┘
```

Hai process vẫn có không gian địa chỉ ảo riêng, nhưng các virtual page có thể trỏ đến cùng physical memory page cho phần code chỉ đọc.

Dữ liệu có thể ghi thường là riêng cho từng process nhờ cơ chế như copy-on-write hoặc mapping riêng.

---

## 11. Tạo dynamic library bằng GCC

Compile với `-fPIC`:

```bash
g++ -fPIC -c math_utils.cpp -o math_utils.o
```

Tạo shared library:

```bash
g++ -shared math_utils.o -o libmath.so
```

Compile chương trình:

```bash
g++ main.cpp -L. -lmath -o app
```

Khi chạy:

```bash
./app
```

có thể gặp lỗi:

```text
error while loading shared libraries:
libmath.so: cannot open shared object file
```

Vì dynamic loader không tự động tìm trong thư mục hiện tại.

Có thể chạy:

```bash
LD_LIBRARY_PATH=. ./app
```

Hoặc thêm runtime path lúc link:

```bash
g++ main.cpp -L. -lmath \
    -Wl,-rpath,'$ORIGIN' \
    -o app
```

`$ORIGIN` nghĩa là thư mục chứa executable.

---

## 12. Vì sao dynamic library cần `-fPIC`?

`PIC` là:

```text
Position Independent Code
```

Dynamic library có thể được map vào các địa chỉ virtual memory khác nhau.

Ví dụ:

```text
Process A:
libmath.so tại 0x7F100000

Process B:
libmath.so tại 0x7A800000
```

Nếu machine code chứa địa chỉ tuyệt đối:

```text
call function at 0x7F100500
```

thì code chỉ chạy đúng khi library nằm đúng địa chỉ đó.

PIC tạo code dựa trên:

- địa chỉ tương đối;
- program counter;
- GOT;
- PLT.

Ví dụ ý tưởng:

```text
Không PIC:
load từ địa chỉ tuyệt đối 0x7F100500

PIC:
load từ địa chỉ hiện tại + offset
```

Điều này giúp cùng một library được load ở nhiều địa chỉ khác nhau mà không cần sửa quá nhiều machine code.

---

## 13. GOT và PLT

Trên ELF/Linux, dynamic linking thường sử dụng:

- **GOT**: Global Offset Table
- **PLT**: Procedure Linkage Table

Giả sử chương trình gọi:

```cpp
add(10, 20);
```

Executable chưa biết địa chỉ thật của `add()` trong `libmath.so`.

Thay vì gọi trực tiếp:

```text
call 0x7F123456
```

nó có thể gọi thông qua PLT:

```text
call add@plt
```

PLT kiểm tra địa chỉ trong GOT:

```text
PLT
 ↓
GOT entry của add
 ↓
địa chỉ thật của add trong libmath.so
```

Luồng:

```text
main()
  ↓
add@plt
  ↓
GOT[add]
  ↓
libmath.so:add()
```

---

## 14. Lazy binding

Dynamic loader có thể không resolve tất cả function ngay lúc chương trình khởi động.

Thay vào đó, nó dùng **lazy binding**.

Lần đầu gọi:

```cpp
add(10, 20);
```

quy trình có thể là:

```text
main()
 ↓
add@plt
 ↓
GOT chưa có địa chỉ thật
 ↓
dynamic resolver
 ↓
tìm add trong libmath.so
 ↓
ghi địa chỉ add vào GOT
 ↓
gọi add()
```

Lần gọi sau:

```text
main()
 ↓
add@plt
 ↓
GOT đã chứa địa chỉ thật
 ↓
gọi trực tiếp add()
```

Lợi ích:

- giảm thời gian startup;
- chỉ resolve các function thực sự được gọi.

Có thể buộc resolve ngay từ đầu:

```bash
LD_BIND_NOW=1 ./app
```

---

## 15. Load-time linking và runtime loading

Dynamic library có hai kiểu sử dụng.

### Kiểu 1: Load khi chương trình khởi động

Bạn link bằng:

```bash
g++ main.cpp -lmath -o app
```

Executable ghi dependency:

```text
NEEDED: libmath.so
```

Dynamic loader tự load library trước `main()`.

Có thể kiểm tra:

```bash
ldd ./app
```

Ví dụ:

```text
libmath.so => ./libmath.so
libstdc++.so.6 => /lib/x86_64-linux-gnu/libstdc++.so.6
libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6
```

### Kiểu 2: Load thủ công trong runtime

Linux dùng:

```cpp
dlopen()
dlsym()
dlclose()
```

Ví dụ:

```cpp
#include <dlfcn.h>
#include <iostream>

using AddFunction = int (*)(int, int);

int main()
{
    void* handle = dlopen("./libmath.so", RTLD_LAZY);

    if (!handle)
    {
        std::cerr << dlerror() << '\n';
        return 1;
    }

    auto add = reinterpret_cast<AddFunction>(
        dlsym(handle, "add")
    );

    if (!add)
    {
        std::cerr << dlerror() << '\n';
        dlclose(handle);
        return 1;
    }

    std::cout << add(10, 20) << '\n';

    dlclose(handle);
}
```

Compile:

```bash
g++ main.cpp -ldl -o app
```

Library nên export hàm với C linkage:

```cpp
extern "C" int add(int a, int b)
{
    return a + b;
}
```

Nếu không có `extern "C"`, C++ name mangling làm symbol có tên như:

```text
_Z3addii
```

thay vì:

```text
add
```

`dlsym(handle, "add")` sẽ không tìm thấy tên đã bị mangle.

---

## 16. Static và dynamic linking khác nhau ở đâu?

### Static linking

```text
main.o
   +
libmath.a
   ↓
linker copy code cần thiết
   ↓
app chứa code của math library
```

Khi chạy:

```text
app chạy độc lập
```

### Dynamic linking

```text
main.o
   +
libmath.so metadata
   ↓
app chỉ ghi dependency
```

Khi chạy:

```text
app
 ↓
dynamic loader
 ↓
libmath.so
 ↓
resolve symbols
```

---

## 17. So sánh trực tiếp

| Tiêu chí | Static library | Dynamic library |
|---|---|---|
| Linux | `.a` | `.so` |
| Windows | `.lib` | `.dll` |
| Code nằm trong executable | Có | Không hoàn toàn |
| Cần library khi chạy | Không | Có |
| Kích thước executable | Lớn hơn | Nhỏ hơn |
| Startup | Đơn giản hơn | Phải load và resolve |
| Chia sẻ code giữa process | Không hiệu quả bằng | Có |
| Update library | Phải build lại executable | Có thể thay library |
| Deployment | Dễ hơn | Phải mang theo dependency |
| Rủi ro version mismatch | Thấp | Cao hơn |
| Plugin system | Không phù hợp | Rất phù hợp |

---

## 18. Ví dụ kích thước

Giả sử library có 500 KB code.

Ba chương trình đều static link:

```text
app1 = executable + 500 KB
app2 = executable + 500 KB
app3 = executable + 500 KB
```

Tổng có thể tăng khoảng:

```text
1.5 MB
```

Nếu dùng dynamic library:

```text
app1 → libmath.so
app2 → libmath.so
app3 → libmath.so
```

Chỉ cần một file:

```text
libmath.so = 500 KB
```

Ngoài dung lượng ổ đĩa, phần code chỉ đọc còn có thể được chia sẻ trong physical memory.

---

## 19. Cập nhật library

### Static library

Ban đầu:

```text
app chứa add() version 1
```

Bạn cập nhật `libmath.a`:

```text
add() version 2
```

Executable cũ vẫn dùng version 1 vì code đã được nhúng vào executable.

Phải relink:

```text
main.o + libmath.a mới → app mới
```

### Dynamic library

Executable tham chiếu:

```text
libmath.so
```

Bạn thay file library tương thích:

```text
libmath.so cũ → libmath.so mới
```

Application có thể dùng code mới mà không cần build lại.

Nhưng chỉ an toàn khi ABI vẫn tương thích.

---

## 20. API và ABI

### API

API là giao diện ở mức source code.

Ví dụ:

```cpp
int add(int a, int b);
```

Nếu đổi thành:

```cpp
long add(long a, long b);
```

source code phía người dùng có thể phải compile lại.

### ABI

ABI là giao diện ở mức binary:

- cách truyền parameter;
- kiểu trả về;
- kích thước object;
- layout class;
- virtual table;
- name mangling;
- calling convention;
- alignment;
- exception mechanism.

Dynamic library phụ thuộc rất mạnh vào ABI.

Ví dụ ban đầu:

```cpp
class User
{
public:
    int id;
};
```

Sau đó library đổi thành:

```cpp
class User
{
public:
    int id;
    bool active;
};
```

Kích thước và layout của `User` có thể thay đổi.

Executable cũ nghĩ rằng:

```text
sizeof(User) = 4
```

Library mới nghĩ rằng:

```text
sizeof(User) = 8
```

Kết quả có thể là:

- đọc sai memory;
- heap corruption;
- crash;
- undefined behavior.

Vì vậy, dynamic library cần quản lý ABI cẩn thận.

---

## 21. Vì sao C API thường dùng ở ranh giới library?

Thay vì expose trực tiếp:

```cpp
class MathEngine
{
public:
    virtual int add(int, int) = 0;
};
```

library có thể expose C API:

```cpp
extern "C"
{
    void* math_create();
    int math_add(void* handle, int a, int b);
    void math_destroy(void* handle);
}
```

Lợi ích:

- không có C++ name mangling;
- ABI ổn định hơn;
- ít phụ thuộc compiler;
- dễ gọi từ C, Python, Rust, C#, Java;
- che giấu layout class C++.

Đây là mô hình opaque handle:

```text
Application chỉ giữ void*
Library quản lý object thật bên trong
```

---

## 22. Static library cũng có ABI không?

Có.

Dù static library chỉ dùng lúc build, object file vẫn phải tương thích với:

- compiler;
- architecture;
- standard library;
- runtime library;
- calling convention;
- build configuration.

Ví dụ, thư viện build cho ARM không thể link vào executable x86-64.

```text
libmath.a cho ARM
        ≠
application x86-64
```

Tương tự, object build bằng các cấu hình runtime khác nhau trên Windows có thể gây lỗi.

---

## 23. Dynamic library trên Windows

Windows sử dụng:

```text
math.dll
math.lib
```

Điểm dễ nhầm:

```text
math.dll
```

là library dùng lúc runtime.

```text
math.lib
```

có thể là **import library**, dùng lúc link.

Import library không chứa toàn bộ implementation. Nó chứa thông tin để linker biết symbol thực sự nằm trong DLL.

Mô hình:

```text
Build time:
main.obj + math.lib → app.exe

Run time:
app.exe + math.dll
```

Trên Windows, để export symbol:

```cpp
#ifdef BUILDING_MATH_LIBRARY
#define MATH_API __declspec(dllexport)
#else
#define MATH_API __declspec(dllimport)
#endif

extern "C" MATH_API int add(int a, int b);
```

Khi build DLL:

```text
BUILDING_MATH_LIBRARY được định nghĩa
→ __declspec(dllexport)
```

Khi application sử dụng DLL:

```text
không định nghĩa macro
→ __declspec(dllimport)
```

---

## 24. `.lib` trên Windows có hai nghĩa

Một file `.lib` có thể là:

### Static library

```text
math.lib
```

chứa implementation thật.

### Import library

```text
math.lib
```

chỉ hỗ trợ linker gọi vào:

```text
math.dll
```

Vì vậy, chỉ nhìn extension `.lib` chưa chắc biết đó là static library hay import library.

---

## 25. Symbol visibility

Không phải mọi function trong dynamic library đều nên được export.

Linux có thể dùng:

```cpp
__attribute__((visibility("default")))
```

Ví dụ:

```cpp
#define API __attribute__((visibility("default")))

extern "C" API int add(int a, int b);
```

Compile với mặc định ẩn symbol:

```bash
g++ -fPIC -fvisibility=hidden \
    -shared math.cpp \
    -o libmath.so
```

Chỉ symbol được đánh dấu `default` mới được export.

Lợi ích:

- giảm symbol table;
- giảm thời gian dynamic linking;
- tránh xung đột tên;
- che giấu implementation;
- bảo vệ ABI nội bộ.

---

## 26. Global variable trong dynamic library

Giả sử library có:

```cpp
int counter = 0;
```

Nếu nhiều process dùng library:

```text
Process A có counter riêng
Process B có counter riêng
```

Phần code có thể chia sẻ physical memory, nhưng writable data thường không được chia sẻ trực tiếp giữa process.

Trong cùng một process, nếu nhiều module dùng cùng một instance library, chúng có thể cùng nhìn thấy một biến global của library.

Tuy nhiên, việc load nhiều bản library hoặc khác namespace loader có thể tạo các instance khác nhau tùy nền tảng và cơ chế load.

---

## 27. Static library và global state

Nếu cùng một static library được link vào nhiều executable:

```text
app1 chứa counter riêng
app2 chứa counter riêng
```

Vì code và data đã được copy vào từng executable.

Nếu static library bị link vào nhiều dynamic library trong cùng process, đôi khi mỗi dynamic library có thể mang một bản global state riêng. Đây là một vấn đề kiến trúc cần chú ý.

---

## 28. Debug và Release library

Không nên tùy tiện trộn:

```text
Debug executable + Release library
```

đặc biệt trên Windows/MSVC.

Khác biệt có thể gồm:

- runtime library khác nhau;
- iterator debugging khác nhau;
- allocator khác nhau;
- macro khác nhau;
- cấu trúc STL khác nhau;
- optimization khác nhau.

Ví dụ nguy hiểm:

```text
DLL A cấp phát std::string
EXE giải phóng bằng runtime khác
```

Điều này có thể gây heap corruption.

Quy tắc an toàn:

```text
Module nào allocate thì module đó deallocate
```

Ví dụ:

```cpp
extern "C" char* create_buffer();
extern "C" void destroy_buffer(char* buffer);
```

Không nên:

```cpp
char* p = create_buffer();
delete[] p;
```

Nên:

```cpp
char* p = create_buffer();
destroy_buffer(p);
```

---

## 29. Dynamic library versioning trên Linux

Linux thường có các tên:

```text
libmath.so
libmath.so.1
libmath.so.1.2.0
```

Ý nghĩa:

```text
libmath.so        → tên dùng khi link
libmath.so.1      → SONAME / major ABI version
libmath.so.1.2.0  → file thật
```

Symlink:

```text
libmath.so -> libmath.so.1
libmath.so.1 -> libmath.so.1.2.0
```

Nếu ABI không tương thích, tăng major version:

```text
libmath.so.1
        ↓ breaking ABI change
libmath.so.2
```

Executable cũ vẫn cần:

```text
libmath.so.1
```

Executable mới có thể cần:

```text
libmath.so.2
```

Hai version có thể cùng tồn tại.

---

## 30. Static library và dead code

Static library không có nghĩa là toàn bộ code luôn bị copy.

Linker có thể bỏ code không dùng thông qua:

- archive member selection;
- section garbage collection;
- link-time optimization.

Compile:

```bash
g++ -ffunction-sections -fdata-sections -c math.cpp
```

Link:

```bash
g++ main.o \
    -Wl,--gc-sections \
    -lmath \
    -o app
```

Mỗi function được đặt vào section riêng.

Linker có thể bỏ các section không được tham chiếu.

Ngoài ra, LTO:

```bash
g++ -flto ...
```

cho phép optimizer tối ưu xuyên nhiều translation unit.

---

## 31. Header-only library có phải static library không?

Không.

Ví dụ:

```cpp
template <typename T>
T add(T a, T b)
{
    return a + b;
}
```

Nếu toàn bộ implementation nằm trong header, mỗi translation unit include header sẽ compile code cần thiết.

```text
main.cpp
  ↓ include header
compiler thấy implementation
  ↓
main.o chứa template instance
```

Header-only library không nhất thiết tạo `.a` hay `.so`.

Ví dụ phổ biến:

- nhiều template library;
- utility nhỏ;
- generic algorithms.

Nhược điểm:

- compile chậm;
- tăng dependency;
- thay header phải compile lại nhiều file;
- có thể tăng code size nếu tối ưu và COMDAT folding không xử lý tốt.

---

## 32. CMake tạo static library

```cmake
cmake_minimum_required(VERSION 3.16)

project(MathApp LANGUAGES CXX)

add_library(math STATIC
    src/math_utils.cpp
)

target_include_directories(math
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(app
    src/main.cpp
)

target_link_libraries(app
    PRIVATE
        math
)
```

Dòng:

```cmake
add_library(math STATIC ...)
```

tạo:

```text
Linux: libmath.a
Windows: math.lib
```

---

## 33. CMake tạo dynamic library

```cmake
add_library(math SHARED
    src/math_utils.cpp
)
```

Tạo:

```text
Linux: libmath.so
Windows: math.dll + math.lib
```

Trên Windows, thường cần export macro:

```cmake
target_compile_definitions(math
    PRIVATE MATH_LIBRARY_BUILD
)
```

Header:

```cpp
#ifdef _WIN32
    #ifdef MATH_LIBRARY_BUILD
        #define MATH_API __declspec(dllexport)
    #else
        #define MATH_API __declspec(dllimport)
    #endif
#else
    #define MATH_API
#endif

MATH_API int add(int a, int b);
```

---

## 34. `PUBLIC`, `PRIVATE`, `INTERFACE` trong CMake

Giả sử:

```cmake
target_link_libraries(app PRIVATE math)
```

`PRIVATE` nghĩa là:

```text
app sử dụng math
nhưng dependency này không truyền tiếp cho target dùng app
```

Ví dụ library:

```cmake
target_link_libraries(engine PUBLIC math)
```

Có nghĩa:

```text
engine cần math
target nào dùng engine cũng cần biết dependency math
```

`INTERFACE` nghĩa là target hiện tại không dùng trực tiếp, nhưng consumer cần.

Ví dụ:

```cmake
target_include_directories(math
    PUBLIC include
)
```

Nghĩa là:

- `math` cần thư mục include;
- target sử dụng `math` cũng cần thư mục include.

---

## 35. Object library trong CMake

CMake còn có:

```cmake
add_library(math_objects OBJECT
    math.cpp
)
```

Object library không tạo `.a` hay `.so`.

Nó tạo object files để target khác tái sử dụng:

```cmake
add_library(math_static STATIC
    $<TARGET_OBJECTS:math_objects>
)

add_library(math_shared SHARED
    $<TARGET_OBJECTS:math_objects>
)
```

Mô hình:

```text
math.cpp
  ↓ compile một lần
math.o
  ├── libmath.a
  └── libmath.so
```

Trong thực tế, cần chú ý PIC nếu object được dùng cho shared library:

```cmake
set_target_properties(math_objects PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
```

---

## 36. Dùng cùng source để tạo cả static và shared library

```cmake
add_library(math_static STATIC
    src/math.cpp
)

add_library(math_shared SHARED
    src/math.cpp
)

set_target_properties(math_static PROPERTIES
    OUTPUT_NAME math
)

set_target_properties(math_shared PROPERTIES
    OUTPUT_NAME math
)
```

Kết quả Linux:

```text
libmath.a
libmath.so
```

Hai target khác nhau nhưng cùng output base name.

---

## 37. Trường hợp nên dùng static library

Static library phù hợp khi:

- muốn executable dễ deploy;
- thiết bị embedded;
- hệ thống không muốn phụ thuộc runtime;
- version library phải cố định;
- executable nhỏ về số lượng module;
- cần tối ưu toàn chương trình;
- môi trường không thuận tiện cài dependency.

Ví dụ với Raspberry Pi hoặc embedded Linux:

```text
app
```

chạy độc lập sẽ dễ đóng gói hơn so với:

```text
app
libA.so
libB.so
libC.so
```

Tuy nhiên, các thư viện hệ thống như `libc` vẫn có thể được dynamic link nếu không build fully static.

---

## 38. Trường hợp nên dùng dynamic library

Dynamic library phù hợp khi:

- nhiều ứng dụng dùng chung một library;
- cần plugin system;
- muốn cập nhật module độc lập;
- muốn giảm duplication;
- ứng dụng lớn có nhiều component;
- muốn thay implementation mà không build lại toàn bộ;
- cần runtime loading.

Ví dụ plugin:

```text
app
├── plugins/
│   ├── camera_plugin.so
│   ├── audio_plugin.so
│   └── tracking_plugin.so
```

Application:

```cpp
dlopen("camera_plugin.so");
```

Có thể thêm plugin mới mà không sửa executable chính.

---

## 39. Vấn đề “DLL Hell”

Dynamic library dễ gặp lỗi version.

Ví dụ application cần:

```text
libmath.so version 1
```

nhưng hệ thống chỉ có:

```text
libmath.so version 2
```

Nếu ABI thay đổi:

```text
symbol missing
symbol version mismatch
crash
```

Trên Windows, vấn đề tương tự được gọi phổ biến là:

```text
DLL Hell
```

Các cách giảm rủi ro:

- semantic versioning;
- SONAME;
- đặt library cạnh executable;
- container;
- package manager;
- rpath;
- ABI compatibility testing;
- expose C API ổn định;
- không expose trực tiếp STL qua boundary.

---

## 40. Vì sao tránh truyền STL qua DLL boundary?

Ví dụ:

```cpp
std::vector<std::string> getUsers();
```

API này phụ thuộc vào:

- compiler;
- phiên bản standard library;
- allocator;
- object layout;
- debug/release mode;
- ABI của `std::string`;
- ABI của `std::vector`.

An toàn hơn:

```cpp
extern "C"
{
    size_t get_user_count();
    const char* get_user_name(size_t index);
}
```

Hoặc dùng buffer do caller cung cấp:

```cpp
extern "C"
int get_user_name(
    size_t index,
    char* buffer,
    size_t buffer_size
);
```

---

## 41. Static executable hoàn toàn

Trên Linux có thể thử:

```bash
g++ main.cpp -static -o app
```

Khi đó executable cố gắng static link cả system libraries.

Kiểm tra:

```bash
ldd ./app
```

Có thể hiện:

```text
not a dynamic executable
```

Tuy nhiên fully static có nhược điểm:

- file rất lớn;
- vấn đề DNS/NSS với glibc;
- khó cập nhật security library;
- license;
- plugin/dlopen có thể phức tạp;
- không phải library nào cũng cung cấp static version.

Vì vậy “dùng static library của mình” không đồng nghĩa toàn bộ executable là fully static.

---

## 42. Một executable có thể dùng cả static và dynamic library

Ví dụ:

```text
app
├── static link: libcore.a
├── dynamic link: libQt6Core.so
├── dynamic link: libc.so
└── dynamic link: libstdc++.so
```

Đây là trường hợp rất phổ biến.

Bạn có thể static link code nội bộ nhưng dynamic link framework hệ thống.

---

## 43. Kiểm tra executable đang phụ thuộc library nào

Linux:

```bash
ldd ./app
```

Hoặc:

```bash
readelf -d ./app
```

Tìm:

```text
NEEDED
```

Ví dụ:

```text
Shared library: [libmath.so]
Shared library: [libstdc++.so.6]
Shared library: [libc.so.6]
```

Kiểm tra symbol:

```bash
nm libmath.a
nm -D libmath.so
```

Thông tin ELF:

```bash
readelf -h app
readelf -S app
readelf -s app
objdump -d app
```

Windows:

```text
dumpbin /DEPENDENTS app.exe
dumpbin /EXPORTS math.dll
```

---

## 44. Ví dụ toàn bộ quy trình static

Cấu trúc:

```text
project/
├── include/
│   └── math.h
├── src/
│   ├── math.cpp
│   └── main.cpp
└── build/
```

Compile:

```bash
g++ -Iinclude -c src/math.cpp -o build/math.o
```

Tạo library:

```bash
ar rcs build/libmath.a build/math.o
```

Compile main:

```bash
g++ -Iinclude -c src/main.cpp -o build/main.o
```

Link:

```bash
g++ build/main.o \
    -Lbuild \
    -lmath \
    -o build/app
```

Chạy:

```bash
./build/app
```

Sau khi build, `app` không cần `libmath.a`.

---

## 45. Ví dụ toàn bộ quy trình dynamic

Compile position-independent code:

```bash
g++ -Iinclude \
    -fPIC \
    -c src/math.cpp \
    -o build/math.o
```

Tạo shared library:

```bash
g++ -shared \
    build/math.o \
    -o build/libmath.so
```

Compile và link app:

```bash
g++ -Iinclude \
    src/main.cpp \
    -Lbuild \
    -lmath \
    -Wl,-rpath,'$ORIGIN' \
    -o build/app
```

Chạy:

```bash
./build/app
```

Ở đây:

```text
$ORIGIN
```

là thư mục chứa `app`, nên loader tìm `libmath.so` cạnh executable.

Nếu xóa:

```text
build/libmath.so
```

thì app không chạy được.

---

## 46. Tóm tắt under the hood

### Static library

```text
.cpp
 ↓ compiler
.o
 ↓ ar
.a
 ↓ linker lấy object cần thiết
executable chứa machine code
 ↓
chạy không cần .a
```

### Dynamic library

```text
.cpp
 ↓ compiler + PIC
.o
 ↓ linker -shared
.so
```

Application:

```text
.cpp
 ↓ compiler
.o
 ↓ linker ghi dependency
executable
```

Runtime:

```text
kernel
 ↓
dynamic loader
 ↓
map executable
 ↓
map .so
 ↓
resolve symbols qua GOT/PLT
 ↓
relocation
 ↓
main()
```

---

## 47. Cách nhớ nhanh

**Static library:**

```text
Copy code vào executable khi build.
```

**Dynamic library:**

```text
Giữ code bên ngoài và kết nối khi chạy.
```

Hoặc:

```text
Static = compile/link-time dependency
Dynamic = runtime dependency
```

Điểm quan trọng nhất là:

> Static library được linker lấy machine code cần thiết và đưa vào executable. Dynamic library được dynamic loader map vào virtual memory và resolve symbol khi chương trình khởi động hoặc khi library được load trong runtime.
