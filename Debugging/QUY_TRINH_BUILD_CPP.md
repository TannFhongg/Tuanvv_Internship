# Quy trình build chương trình C++ với `g++`

Quy trình build C++ gồm:

```text
.cpp + header
    ↓ Preprocess
.ii
    ↓ Compile
.s
    ↓ Assemble
.o
    ↓ Link
.exe
```

## 1. Preprocess

```powershell
g++ -E main.cpp -o main.ii
```

- `g++`: GCC driver dành cho C++.
- `-E`: Chỉ chạy preprocessor rồi dừng.
- `main.cpp`: Mã nguồn đầu vào.
- `-o main.ii`: Ghi kết quả vào `main.ii`.

Ở bước này, preprocessor sẽ:

- Thay `#include` bằng nội dung header.
- Mở rộng các macro `#define`.
- Xử lý `#if`, `#ifdef`, `#ifndef`.
- Loại bỏ comment.
- Chưa sinh mã máy.

`.ii` là phần mở rộng thường dùng cho mã C++ đã được preprocess.

Ví dụ:

```cpp
#define VALUE 10

int number = VALUE;
```

Sau preprocessing sẽ gần giống:

```cpp
int number = 10;
```

## 2. Compile sang assembly

```powershell
g++ -S main.ii -o main.s
```

- `-S`: Compile rồi dừng trước bước assemble.
- `main.ii`: Mã C++ đã qua preprocessing.
- `main.s`: Mã assembly đầu ra.

Ở bước này compiler thực hiện:

- Phân tích cú pháp.
- Kiểm tra kiểu dữ liệu.
- Xử lý template.
- Kiểm tra tên biến và hàm.
- Thực hiện tối ưu hóa nếu có `-O1`, `-O2` hoặc `-O3`.
- Sinh assembly cho kiến trúc máy hiện tại.

Ví dụ assembly có thể gần giống:

```asm
mov eax, 10
```

## 3. Assemble thành object file

```powershell
g++ -c main.s -o main.o
```

- `-c`: Tạo object file nhưng không link.
- `main.s`: Assembly đầu vào.
- `main.o`: Object file chứa mã máy.

`main.o` đã chứa mã máy nhưng chưa thể chạy độc lập, vì nó có thể còn tham chiếu đến các hàm nằm trong file khác.

Ví dụ, nếu `main.cpp` gọi:

```cpp
int result = add(2, 3);
```

và `add()` được định nghĩa trong `math.cpp`, `main.o` chỉ ghi nhận rằng nó cần một symbol tên `add`.

Lệnh tiếp theo:

```powershell
g++ -c math.cpp -o math.o
```

Lệnh này không chỉ assemble. Vì đầu vào là `math.cpp`, driver tự chạy ba bước:

```text
Preprocess → Compile → Assemble
```

Sau đó tạo trực tiếp `math.o`. Vì vậy, comment chính xác hơn là:

```powershell
# Tạo object file cho từng translation unit
g++ -c main.s -o main.o
g++ -c math.cpp -o math.o
```

## 4. Link thành executable

```powershell
g++ main.o math.o -o calculator.exe
```

Linker sẽ:

- Gộp `main.o` và `math.o`.
- Tìm định nghĩa cho các symbol được sử dụng.
- Liên kết thư viện chuẩn C++ cần thiết.
- Tạo chương trình `calculator.exe`.

Ví dụ:

```text
main.o cần hàm add()
              ↓
math.o cung cấp hàm add()
              ↓
Link thành công
```

Nếu không truyền `math.o`, linker có thể báo:

```text
undefined reference to `add(int, int)'
```

Nếu `add()` được định nghĩa nhiều lần không hợp lệ, linker có thể báo:

```text
multiple definition of `add(int, int)'
```

## 5. Build trực tiếp

```powershell
g++ -std=c++20 -Wall -Wextra -Wpedantic -g main.cpp math.cpp -o calculator.exe
```

Driver tự thực hiện toàn bộ quy trình cho từng file `.cpp`:

```text
main.cpp → main object tạm
math.cpp → math object tạm
                  ↓
                Link
                  ↓
          calculator.exe
```

Ý nghĩa các tùy chọn:

| Tùy chọn | Ý nghĩa |
| --- | --- |
| `-std=c++20` | Sử dụng chuẩn C++20 |
| `-Wall` | Bật nhóm cảnh báo phổ biến |
| `-Wextra` | Bật thêm các cảnh báo hữu ích |
| `-Wpedantic` | Cảnh báo mã không tuân thủ nghiêm chuẩn C++ |
| `-g` | Thêm thông tin debug cho GDB |
| `-o calculator.exe` | Đặt tên file đầu ra |

Lệnh này chưa bật tối ưu hóa nên phù hợp cho quá trình học và debug.

## 6. Quy trình thủ công đầy đủ cho cả hai file

Nếu muốn quan sát đủ mọi giai đoạn cho cả `main.cpp` và `math.cpp`:

```powershell
# Preprocess
g++ -std=c++20 -E main.cpp -o main.ii
g++ -std=c++20 -E math.cpp -o math.ii

# Compile sang assembly
g++ -std=c++20 -S main.ii -o main.s
g++ -std=c++20 -S math.ii -o math.s

# Assemble sang object
g++ -c main.s -o main.o
g++ -c math.s -o math.o

# Link
g++ main.o math.o -o calculator.exe

# Chạy
.\calculator.exe
```

Mỗi file `.cpp` là một translation unit riêng. Header được đưa vào translation unit thông qua `#include`; thông thường header không được compile độc lập.

## 1.3. Debugging

### Quy trình debug chuẩn

1. **Tạo bản tái hiện lỗi nhỏ nhất** (*minimal reproducible example*): loại bỏ những phần không liên quan nhưng vẫn giữ được lỗi.
2. **Build bản debug:**
   - GCC/Clang: dùng `-g -O0`.
   - MSVC: dùng `/Zi /Od`.
3. **Đọc kỹ thông báo lỗi** và đặt breakpoint gần nơi dữ liệu bắt đầu sai, không chỉ đặt tại dòng chương trình crash.
4. **Kiểm tra trạng thái chương trình:** call stack, biến cục bộ, trạng thái object, ownership và lifetime.
5. **Sửa nguyên nhân gốc**, thêm regression test để lỗi không xuất hiện trở lại.
6. **Kiểm tra lại:** build với warning ở mức cao và chạy sanitizer nếu môi trường hỗ trợ.

Build debug với GCC:

```powershell
g++ -std=c++20 -g -O0 -Wall -Wextra -Wpedantic main.cpp math.cpp -o app.exe
```

Trong đó:

- `-g`: thêm thông tin symbol và source line để debugger sử dụng.
- `-O0`: tắt tối ưu hóa, giúp thứ tự thực thi và giá trị biến dễ theo dõi hơn.
- Các cờ `-Wall -Wextra -Wpedantic`: bật những cảnh báo hữu ích ngay từ lúc compile.

Build debug với MSVC:

```powershell
cl /std:c++20 /Zi /Od /EHsc main.cpp math.cpp /Fe:app.exe
```

### Các lệnh GDB cần thành thạo

```gdb
gdb ./app.exe       # Mở chương trình bằng debugger
break main          # Đặt breakpoint tại hàm main
run                 # Chạy chương trình
next                # Chạy dòng hiện tại, không đi vào trong hàm được gọi
step                # Chạy dòng hiện tại và đi vào trong hàm được gọi
continue            # Chạy đến breakpoint hoặc sự kiện dừng tiếp theo
print variable      # Xem giá trị của biến hoặc expression
backtrace           # Hiển thị call stack
frame 1             # Chuyển sang stack frame số 1
info locals         # Hiển thị các biến local của frame hiện tại
watch variable      # Dừng khi giá trị của biến thay đổi
quit                # Thoát GDB
```

Ví dụ một phiên debug ngắn:

```gdb
gdb ./app.exe
break main
run
next
print result
backtrace
continue
quit
```

### Khi đặt breakpoint

Nếu chương trình crash tại dòng sử dụng con trỏ, nguyên nhân có thể đã xảy ra trước đó, chẳng hạn object bị giải phóng quá sớm hoặc con trỏ bị ghi đè. Vì vậy cần lần ngược call stack và đặt breakpoint tại nơi dữ liệu được tạo, thay đổi hoặc chuyển quyền sở hữu.

Sau khi sửa lỗi, nên chạy lại với sanitizer nếu dùng GCC hoặc Clang:

```powershell
g++ -std=c++20 -g -O1 -Wall -Wextra -fsanitize=address,undefined main.cpp math.cpp -o app.exe
.\app.exe
```

- AddressSanitizer giúp phát hiện truy cập bộ nhớ không hợp lệ, use-after-free và buffer overflow.
- UndefinedBehaviorSanitizer giúp phát hiện nhiều dạng undefined behavior trong lúc chạy.
