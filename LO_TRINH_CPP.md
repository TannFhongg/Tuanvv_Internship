# Lộ trình C++ — từ nền tảng đến hệ thống

## 1. Mức hiện tại

Đã học các nội dung nền tảng sau:

- Pointer & Reference: con trỏ, tham chiếu, `const`, stack/heap, smart pointer, RAII.
- Function: truyền tham số, overload, override, `virtual`/pure virtual, operator overloading.
- Type casting: `static_cast`, `dynamic_cast`, `const_cast`.
- OOP: `class`/`struct`, constructor/destructor, kế thừa, đa hình, Rule of 0/3/5, multiple/diamond inheritance.
- STL: `vector`, `array`, `list`, `map`, `set`, iterator, `sort`, `find`.
- CMake cơ bản để build project.

## 2. Nguyên tắc học và đầu ra


1. **Ghi chú lý thuyết**: khái niệm, cơ chế bên dưới, ưu/nhược điểm, use case và anti-pattern.
2. **Ví dụ nhỏ**: mã nguồn ngắn, tự build bằng dòng lệnh và có giải thích kết quả.
3. **Bài tập tự cài đặt**: hạn chế dùng sẵn thành phần đang học để hiểu cơ chế thực tế.
4. **Tự kiểm tra**: trả lời được “vì sao dùng cách này?”, “đổi sang cách khác có rủi ro gì?”, và “máy tính/compiler thực hiện gì?”.


## 3. Thứ tự ưu tiên

| Giai đoạn | Nội dung | Mục tiêu chính | Sản phẩm đầu ra |
| --- | --- | --- | --- |
| 0 | Củng cố casting, smart pointer, macro/inline | Hiểu an toàn kiểu, ownership và cơ chế biên dịch 
| 1 | Compiler, linker, debugging | Tự build từng stage và lần lỗi có hệ thống | `compiler_pipeline_lab` |
| 2 | Exception và templates | Viết generic code an toàn, hiểu compile-time | `result_or_error`, `mini_traits` |
| 3 | Modern C++ | Ownership transfer, perfect forwarding, code rõ ràng | `move_semantics_lab` |
| 4 | Concurrency | Hiểu đồng bộ và data race từ gốc | `threading_primitives` |
| 5 | Build system & libraries | Quản lý target, dependency, static/dynamic library | `cpp_build_system_lab` |
| 6 | Tổng hợp | Đóng gói kiến thức thành một project | `concurrent_task_queue` hoặc `mini_logger` |


---

## Giai đoạn 0 — Củng cố các nền tảng được yêu cầu học sâu

### 0.1. Type casting

#### Mục tiêu

Phân biệt rõ **compile-time check** và **run-time check**, hiểu cast nào thể hiện đúng ý đồ và cast nào che giấu lỗi thiết kế.

| Cast | Cơ chế / điểm mạnh | Rủi ro / điểm yếu | Use case phù hợp |
| --- | --- | --- | --- |
| `static_cast<T>` | Kiểm tra nhiều lỗi ở compile time; rõ ý định chuyển kiểu thông thường | Downcast base → derived không được kiểm tra runtime; sai kiểu có thể gây undefined behavior khi dùng | Numeric conversion chủ động; upcast; downcast khi invariant đã được bảo đảm; gọi constructor chuyển đổi rõ ràng |
| `dynamic_cast<T>` | Kiểm tra kiểu thật lúc runtime với hierarchy có RTTI và base polymorphic | Có chi phí runtime; phụ thuộc RTTI; dùng quá nhiều có thể báo hiệu thiết kế đa hình chưa tốt | Downcast cần kiểm tra an toàn; với pointer trả `nullptr`, với reference ném `std::bad_cast` |
| `const_cast<T>` | Thêm/bỏ `const` hoặc `volatile` ở type system | Ghi vào object được khai báo `const` là undefined behavior | Giao tiếp API cũ nhận non-const pointer nhưng cam kết không sửa dữ liệu; rất hiếm khi nên dùng |
| C-style cast / `reinterpret_cast` | C-style có thể thử nhiều dạng cast; `reinterpret_cast` biểu diễn chuyển đổi bit-level | Dễ che giấu ý định, có thể vi phạm alignment/aliasing/lifetime và gây UB | Tránh C-style cast; chỉ dùng `reinterpret_cast` cho low-level code có điều kiện được chứng minh rõ ràng |


#### Bài tập

1. Viết hierarchy `Shape -> Circle/Rectangle` có destructor `virtual`; thử downcast đúng/sai bằng `static_cast` và `dynamic_cast`.
2. Tạo ví dụ numeric narrowing (`double` sang `int`) và ghi rõ mất mát dữ liệu.
3. Tạo object `const` thật sự, dùng `const_cast` rồi thử sửa; chạy sanitizer để quan sát hành vi không an toàn. Không dùng kỹ thuật này trong code production.
4. Viết `README` một trang: mỗi cast có một ví dụ “nên dùng” và một ví dụ “không nên dùng”.

#### Tiêu chí hoàn thành

- Không dùng C-style cast trong toàn bộ bài tập C++.
- Khi review một cast, chỉ ra được precondition và lý do cast đó an toàn.

### 0.2. Smart pointer và RAII

#### Mục tiêu

Hiểu ownership, lifetime, exception safety và lý do smart pointer là công cụ RAII chứ không phải “con trỏ tự động cho mọi trường hợp”.

| Loại | Ownership | Khi dùng | Lưu ý |
| --- | --- | --- | --- |
| `std::unique_ptr<T>` | Một owner duy nhất | Mặc định khi cấp phát động cần ownership | Move-only; ưu tiên `std::make_unique`; không copy được |
| `std::shared_ptr<T>` | Nhiều owner, reference counting | Ownership dùng chung thực sự cần thiết | Có control block và chi phí atomic; tránh dùng thay cho thiết kế ownership rõ ràng |
| `std::weak_ptr<T>` | Không sở hữu | Quan sát object do `shared_ptr` quản lý; phá vòng tham chiếu | Phải `lock()` trước khi dùng |
| Raw pointer/reference | Không nên ngầm biểu diễn ownership | Quan sát/truy cập object còn sống do nơi khác quản lý | Cần quy ước lifetime rõ ràng |

#### Bài tập tự cài đặt

1. `UniquePtr<T>`: move constructor, move assignment, `get`, `release`, `reset`, `operator*`, `operator->`, destructor.
2. `SharedPtr<T>` và `WeakPtr<T>` tối giản: tách object và control block; đếm `strong_count`/`weak_count`; xử lý destruction đúng thứ tự.
3. Dùng class `FileHandle` hoặc `MutexGuard` để minh họa RAII: tài nguyên luôn được giải phóng khi return sớm hoặc exception.
4. Viết test cho copy/move, self-assignment, object destructor count, vòng `shared_ptr` và cách `weak_ptr` giải quyết vòng đó.

#### Giới hạn của bài tự cài đặt

Không cần sao chép toàn bộ standard library. Cần ghi rõ các phần chưa hỗ trợ như custom deleter, aliasing constructor, thread-safe control block hoàn chỉnh.

### 0.3. Macro và `inline`

#### Macro

- Macro được **preprocessor** mở rộng trước khi compiler phân tích cú pháp/type. Nó chỉ thay text token, không có type checking, scope theo C++ hay debugger-friendly behavior.
- Ưu điểm: include guard, feature flag, conditional compilation, platform/compiler detection, `assert`-like facility.
- Nhược điểm: double evaluation, precedence bug, name collision, lỗi khó debug.
- Không dùng macro cho hằng số, function-like logic hay type alias nếu C++ có lựa chọn an toàn hơn: `constexpr`, `inline` function, template, `enum class`, `using`.

Ví dụ cần phân tích:

```cpp
#define SQUARE(x) x * x      // SQUARE(1 + 2) => 1 + 2 * 1 + 2
#define MAX(a, b) ((a) > (b) ? (a) : (b)) // có thể evaluate hai lần
```

#### `inline`

- `inline` hiện đại chủ yếu là quy tắc **ODR/linkage**: cho phép cùng định nghĩa function/variable xuất hiện ở nhiều translation unit, miễn định nghĩa giống nhau.
- Nó **không bắt buộc** compiler chèn thân hàm vào nơi gọi. Quyết định inlining do optimizer dựa trên optimization level, kích thước hàm, profile, ABI, v.v.
- Dùng `inline` cho function định nghĩa trong header khi header được include ở nhiều `.cpp`; các member function định nghĩa ngay trong class là inline ngầm.
- `constexpr` function thường có thể inline nhưng hai khái niệm không đồng nghĩa.

#### Bài tập

1. Dùng `g++ -E` hoặc `clang++ -E` để xem output macro expansion.
2. Tạo function định nghĩa trong header, include từ hai `.cpp`; quan sát multiple-definition khi bỏ `inline` và build thành công khi thêm `inline`.
3. So sánh build `-O0` và `-O2`, xuất assembly (`-S`) để thấy compiler có thể inline một hàm không ghi `inline`, hoặc không inline hàm đã ghi `inline`.

---

## Giai đoạn 1 — Compiler, linker và debugging

### 1.1. Quy trình từ `.cpp`/`.h` đến executable

Header không được build độc lập; nó được chèn vào translation unit thông qua `#include`. Một chương trình nhiều file được xử lý theo chuỗi sau:

```text
source.cpp + included headers
        │  Preprocessing: #include, #define, #if
        ▼
preprocessed source (.ii / .i)
        │  Compilation: parse, type-check, optimize, sinh assembly
        ▼
assembly (.s)
        │  Assembling: assembly → machine code + symbol/relocation info
        ▼
object files (.o / .obj)
        │  Linking: resolve symbols, kết hợp object + libraries
        ▼
executable (.exe) hoặc library (.a/.lib, .so/.dll)
        │  Loader (khi chạy): nạp executable và dynamic libraries vào process
        ▼
running process
```

#### Thực hành build từng bước với GCC/MinGW

Ví dụ có `main.cpp` gọi hàm trong `math.cpp`/`math.h`:

```powershell
# 1. Preprocess
g++ -E main.cpp -o main.ii

# 2. Compile sang assembly
g++ -S main.ii -o main.s

# 3. Assemble từng translation unit sang object file
g++ -c main.s -o main.o
g++ -c math.cpp -o math.o

# 4. Link object files thành executable
g++ main.o math.o -o calculator.exe

# Build trực tiếp (driver sẽ gọi các bước cần thiết)
g++ -std=c++20 -Wall -Wextra -Wpedantic -g main.cpp math.cpp -o calculator.exe
```

Sau mỗi bước, dùng công cụ phù hợp để quan sát:

- `g++ -E`: xem output preprocessor và macro/header expansion.
- `g++ -S`: xem assembly compiler sinh ra.
- `objdump -d calculator.exe`: disassemble.
- `nm main.o` (hoặc `dumpbin /symbols` trên MSVC): xem symbol chưa/đã được resolve.
- `ldd` trên Linux hoặc `dumpbin /dependents` trên Windows: xem dynamic dependency.

#### Các lỗi cần tự tạo và sửa

1. **Compile error**: sai type/cú pháp — không sinh object file.
2. **Linker error**: khai báo hàm nhưng không link implementation (`undefined reference` / `unresolved external symbol`).
3. **Multiple definition/ODR violation**: định nghĩa non-inline trong header được include bởi nhiều `.cpp`.
4. **Runtime loader error**: thiếu DLL khi chạy executable dùng dynamic library.

### 1.2. Các compiler/toolchain thường gặp

| Toolchain | Đặc điểm chính | Môi trường phổ biến | Lưu ý |
| --- | --- | --- | --- |
| GCC (`g++`) | Compiler phổ biến, mã nguồn mở; driver điều phối preprocessor/compiler/assembler/linker | Linux, MinGW/WSL trên Windows | `g++` link C++ standard library tự động; `gcc` chủ yếu là C driver |
| Clang (`clang++`) | Frontend LLVM, diagnostic thường dễ đọc, có tool ecosystem tốt | macOS, Linux, Windows | Thường dùng LLVM linker/lld tùy cấu hình; ABI phải tương thích với thư viện đang link |
| MSVC (`cl.exe`) | Toolchain C++ chính của Microsoft, tích hợp tốt Windows/Visual Studio | Windows | Dùng `.obj`, `.lib`, `.dll`; flag và runtime library khác GCC/Clang |
| MinGW-w64 | Port/toolchain GCC cho Windows, tạo executable Windows | Windows | Không phải một compiler riêng; chủ yếu cung cấp GCC + runtime/headers Windows |

Điểm cần hiểu: compiler có thể cùng hỗ trợ chuẩn C++ nhưng không hoàn toàn giống nhau về diagnostics, flags, ABI, standard library, name mangling, runtime và linker. Không tự do link mọi binary build bởi toolchain khác nhau nếu ABI/runtime không tương thích.

### 1.3. Debugging

#### Quy trình debug chuẩn

1. Tạo bản tái hiện lỗi nhỏ nhất.
2. Build debug: `-g -O0` (GCC/Clang) hoặc `/Zi /Od` (MSVC).
3. Đọc lỗi, đặt breakpoint gần nơi dữ liệu bắt đầu sai; không chỉ đặt ở chỗ crash.
4. Kiểm tra call stack, biến cục bộ, object state và ownership/lifetime.
5. Sửa nguyên nhân, thêm regression test, rồi chạy lại với warning và sanitizer nếu dùng được.

#### GDB commands cần thành thạo

```text
gdb ./app.exe       # mở debugger
break main          # đặt breakpoint
run                 # chạy chương trình
next / step         # bước qua / đi vào hàm
continue            # chạy đến breakpoint tiếp theo
print variable      # xem biến/expression
backtrace           # call stack
frame 1             # chuyển stack frame
info locals         # biến local hiện tại
watch variable      # dừng khi biến bị thay đổi
quit
```

#### Bài tập

- Tạo crash do dangling pointer hoặc out-of-bounds, dùng breakpoint + `backtrace` để tìm nguyên nhân.
- Tạo bug logic trong linked list/`UniquePtr`, dùng watchpoint để tìm nơi state bị thay đổi sai.
- Build cùng code ở `-O0 -g` và `-O2 -g`; quan sát vì sao một số biến có thể bị optimizer thay đổi/khó quan sát.
- Nếu môi trường hỗ trợ, chạy AddressSanitizer/UndefinedBehaviorSanitizer: `-fsanitize=address,undefined -fno-omit-frame-pointer`.

#### Tiêu chí hoàn thành

- Có thể giải thích compiler error, linker error và runtime error khác nhau ở giai đoạn nào.
- Có thể tự build thủ công một chương trình hai `.cpp` qua preprocess, compile, assemble và link.
- Có thể dùng GDB để lấy backtrace và lần đến dòng gây lỗi.

---

## Giai đoạn 2 — Exception handling và templates

### 2.1. Exception handling

#### Mục tiêu

Viết code có an toàn tài nguyên khi lỗi xảy ra và chọn chiến lược báo lỗi phù hợp.

- Hiểu luồng `throw` → stack unwinding → destructor của các object local → `catch`.
- Bắt exception bằng `const std::exception&` để tránh slicing và copy không cần thiết.
- Custom exception kế thừa `std::runtime_error`, `std::logic_error` hoặc `std::exception` tùy ý nghĩa.
- Không dùng exception cho nhánh điều khiển thông thường; không ném exception từ destructor (trừ trường hợp xử lý cẩn thận).
- Phân biệt basic guarantee, strong guarantee, no-throw guarantee.

#### Bài tập

1. `FileParser` ném `ParseError` có dòng/cột/lý do khi input sai.
2. `BankAccount::transfer` đạt strong exception guarantee: nếu thao tác thất bại, dữ liệu không bị thay đổi một phần.
3. So sánh API ném exception với API trả lỗi (`std::optional`, `std::expected` nếu dùng C++23 hoặc kiểu `Result` tự viết).

### 2.2. Templates

#### Mục tiêu

Hiểu template được instantiation tại compile time, cách giới hạn generic code và khi specialization hợp lý.

- Function template, class template, non-type template parameter.
- Full specialization và partial specialization (chỉ class template; function template không partial specialization).
- Variadic template, parameter pack, fold expression.
- Type traits cơ bản: `std::is_integral`, `std::is_same`, `std::remove_reference`, `std::decay`, `std::enable_if`.
- SFINAE: substitution failure loại overload không phù hợp thay vì tạo lỗi compile; biết rằng C++20 concepts thường đọc rõ hơn khi có thể.

#### Bài tập

1. Viết `max_value<T>` và `FixedArray<T, N>`.
2. Viết `print_all(Args&&... args)` bằng fold expression.
3. Viết overload `to_string_value` chỉ nhận integral/floating-point bằng `std::enable_if_t` hoặc `std::is_arithmetic_v`.
4. Viết `is_pointer<T>` tối giản bằng partial specialization để hiểu cơ chế type trait.

---

## Giai đoạn 3 — Modern C++

### Mục tiêu

Hiểu move semantics là chuyển giao tài nguyên/ownership, không phải “copy nhanh hơn” trong mọi trường hợp.

### Nội dung

- Value category: lvalue, xvalue, prvalue; lvalue reference và rvalue reference.
- Move constructor/assignment; trạng thái hợp lệ nhưng không xác định rõ sau move.
- `std::move` chỉ là cast sang rvalue; nó không tự di chuyển dữ liệu.
- `std::forward` và forwarding reference; perfect forwarding ở mức cơ bản.
- `noexcept` và ảnh hưởng khi container (ví dụ `std::vector`) quyết định move hay copy khi reallocation.
- Rule of Zero là ưu tiên; dùng member RAII để compiler tự sinh special member functions khi phù hợp.
- Structured bindings: `auto [key, value]`, binding với array/tuple/struct; cẩn thận copy so với `auto&`.

### Bài tập

1. Viết `Buffer` sở hữu dynamic array, log copy/move/destructor; dùng trong `std::vector<Buffer>` để quan sát reallocation.
2. Thử có/không có `noexcept` cho move constructor và giải thích số lần copy/move.
3. Viết factory trả về `std::unique_ptr<Base>` và dùng polymorphism mà không manual `delete`.
4. Duyệt `std::map` bằng structured binding, so sánh `auto [k, v]` và `const auto& [k, v]`.

---

## Giai đoạn 4 — Concurrency (học kỹ và tự cài đặt)

### Mục tiêu

Nắm được các khái niệm hệ thống: thread scheduling, data race, race condition, critical section, visibility, deadlock, lock ordering và memory ordering.

> Quy tắc quan trọng: **data race trong C++ là undefined behavior**. `volatile` không thay thế `std::atomic` và mutex không chỉ nhằm “ngăn hai thread cùng chạy”, mà còn tạo đồng bộ/quan hệ happens-before cho dữ liệu được bảo vệ.

### 4.1. `std::thread`

- Tạo thread, truyền tham số, `join`, `detach`; thread phải được `join`/`detach` trước destructor.
- Chọn `join` mặc định; chỉ `detach` khi lifetime và cơ chế báo lỗi được thiết kế rất rõ.
- Bài tập: chia mảng lớn thành các đoạn, tính tổng song song; so sánh với bản tuần tự.

### 4.2. `std::mutex` và lock

- `std::mutex` bảo vệ critical section và dữ liệu chung.
- Ưu tiên RAII lock: `std::lock_guard`, `std::unique_lock`, `std::scoped_lock`; không tự `lock()`/`unlock()` thủ công trừ khi có lý do rõ ràng.
- Deadlock: lock cùng thứ tự, dùng `std::scoped_lock` cho nhiều mutex, hạn chế giữ lock lâu.
- Bài tập: tự viết `LockGuard` tối giản bằng OOP (không copyable) bọc `std::mutex` để hiểu RAII; sau đó dùng `std::lock_guard` trong code thật.

### 4.3. `std::condition_variable`

- Dùng cho việc chờ một **điều kiện trạng thái**, không phải chờ một event mơ hồ.
- Luôn dùng predicate: `cv.wait(lock, [] { return condition; })` để chống spurious wakeup và kiểm tra lại state sau khi thức dậy.
- Thứ tự điển hình: lock → thay đổi state → unlock → `notify_one`/`notify_all`.
- Bài tập: tự viết bounded blocking queue (producer–consumer) bằng `std::queue`, `std::mutex`, hai `condition_variable`; có `push`, `pop`, `close` và nhiều producer/consumer.

### 4.4. `std::atomic`

- Atomic xử lý thao tác nguyên tử trên một biến, không thay thế mutex cho nhiều biến cần invariant chung.
- Hiểu `load`, `store`, `fetch_add`, `compare_exchange_weak/strong`.
- Bắt đầu với `memory_order_seq_cst`; chỉ học `acquire/release/relaxed` sau khi giải thích được happens-before.
- Bài tập: atomic counter; spinlock tối giản bằng `atomic_flag`; so sánh với mutex và ghi rõ hạn chế của spinlock (busy-wait, fairness, priority inversion).

### Bài tập tổng hợp bắt buộc

Xây dựng `ThreadSafeQueue<T>`:

- Bounded capacity.
- Nhiều producer và consumer.
- Không data race, không busy waiting.
- Đóng queue an toàn: consumer đang chờ được đánh thức và biết không còn dữ liệu.
- Không giữ mutex trong lúc xử lý phần tử lâu.
- Có test stress lặp nhiều lần; chạy ThreadSanitizer nếu toolchain/môi trường hỗ trợ.

### Câu hỏi phải trả lời được

- Race condition và data race khác nhau như thế nào?
- Vì sao `count++` không an toàn giữa nhiều thread dù `count` là `int`?
- Vì sao condition variable cần predicate?
- Khi nào atomic counter đủ, khi nào cần mutex?
- Deadlock xảy ra ra sao và cách thiết kế để tránh?

---

## Giai đoạn 5 — CMake, Makefile và libraries

### 5.1. CMake nâng cao

#### Nội dung cần nắm

- CMake là **build-system generator**, không trực tiếp là compiler/build tool. Nó tạo file build cho Ninja, Makefiles, Visual Studio/MSBuild, v.v.
- Target-based CMake: `add_executable`, `add_library`, `target_sources`, `target_include_directories`, `target_compile_features`, `target_compile_options`, `target_link_libraries`.
- Scope dependency: `PRIVATE`, `PUBLIC`, `INTERFACE`.
  - `PRIVATE`: chỉ target hiện tại cần.
  - `PUBLIC`: target hiện tại và consumer của nó cần.
  - `INTERFACE`: chỉ consumer cần (header-only/interface requirement).
- Không dùng global `include_directories()` / `link_directories()` nếu target-based API đáp ứng được.
- Build directory tách source directory: `cmake -S . -B build`, sau đó `cmake --build build`.
- `Debug` vs `Release`, generator expressions cơ bản, `CTest`, `install`, `find_package`/`FetchContent` ở mức phù hợp.

#### Ví dụ cấu trúc project

```text
cpp_build_system_lab/
├── CMakeLists.txt
├── app/
│   └── main.cpp
├── include/
│   └── mathlib/calculator.hpp
├── src/
│   └── calculator.cpp
├── tests/
│   └── calculator_tests.cpp
```

#### CMake tối thiểu theo target

#### Bài tập

1. Chia project thành executable, static library và test target.
2. Tạo một header-only target dùng `INTERFACE`.
3. Dùng `PUBLIC` include directory đúng chỗ để app thấy header qua library mà không include path thủ công.
4. Link một thư viện ngoài bằng `find_package` hoặc package manager đã chọn (vcpkg hoặc Conan); ghi rõ dependency được tìm/cài ở đâu.

### 5.2. Makefile cơ bản

#### Mục tiêu

Hiểu build rule, dependency và incremental build trước khi chỉ dựa vào generator.

Cần giải thích được `$@`, `$<`, `$^`, `.PHONY`, target/prerequisite/recipe và vì sao header dependency cần được khai báo/tự sinh (ví dụ `-MMD -MP`) để rebuild đúng khi header thay đổi.

> Ninja chưa cần học riêng. Khi cần, hiểu rằng Ninja là một build tool nhanh; CMake có thể sinh Ninja build files tương tự như sinh Makefiles.

### 5.3. Static và dynamic library

| Loại | Artifact điển hình | Thời điểm liên kết | Ưu điểm | Nhược điểm |
| --- | --- | --- | --- | --- |
| Static library | Linux: `.a`; Windows: `.lib` | Nội dung cần thiết được linker đưa vào executable | Dễ deploy một executable, không phụ thuộc DLL runtime tương ứng | Executable lớn hơn; update library thường cần relink/redeploy app |
| Dynamic/shared library | Linux: `.so`; Windows: `.dll` + import `.lib` | Link một phần khi build, loader nạp implementation khi chạy | Chia sẻ code, update library có thể tách app nếu tương thích ABI | Deployment, versioning/ABI, search path và thiếu DLL phức tạp hơn |

#### Bài tập bắt buộc

1. Viết `mathlib` có header public và implementation `.cpp`.
2. Build `mathlib` dạng **static library**, link vào `calculator_app`; kiểm tra executable hoạt động độc lập theo cách phù hợp với platform.
3. Build lại `mathlib` dạng **shared library** (`SHARED` trong CMake); chạy app và xử lý library path/DLL placement đúng cách.
4. Ghi lại sự khác nhau của output, symbol export/import (đặc biệt trên Windows) và khi executable cần library.

---

## Giai đoạn 6 — Project tổng hợp


## Checklist 

- [ ] Phân biệt được `static_cast`, `dynamic_cast`, `const_cast`, C-style cast và rủi ro của chúng bằng ví dụ chạy được.
- [ ] Tự cài đặt `UniquePtr`, `SharedPtr`, `WeakPtr` bản tối giản và giải thích phần nào khác `std` implementation.
- [ ] Giải thích được macro chạy trước compiler thế nào và `inline` không đồng nghĩa với luôn inline code.
- [ ] Build một chương trình nhiều file qua bốn bước: preprocess → compile → assemble → link.
- [ ] Phân biệt GCC, Clang, MSVC, MinGW-w64; biết ABI/toolchain compatibility là vấn đề gì.
- [ ] Dùng GDB breakpoint, step, print, watchpoint, backtrace để tìm một lỗi thực tế.
- [ ] Viết exception an toàn với RAII và custom exception.
- [ ] Viết function/class/variadic template, specialization và SFINAE/type traits cơ bản.
- [ ] Giải thích và chứng minh bằng log/test copy/move/noexcept/structured binding.
- [ ] Tự viết bài producer–consumer đúng với mutex + condition variable; dùng atomic đúng phạm vi.
- [ ] Tạo CMake project nhiều target, dùng đúng `PRIVATE`/`PUBLIC`/`INTERFACE`.
- [ ] Viết Makefile cơ bản, build static library và dynamic library, sau đó link/run được app.