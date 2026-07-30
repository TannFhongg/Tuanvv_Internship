
# Hướng Dẫn Debug Lỗi Out-Of-Bounds C++ Bằng GDB

Tài liệu hướng dẫn quy trình từng bước sử dụng **GDB (GNU Debugger)** để truy vết và xử lý lỗi truy cập vượt quá phạm vi bộ nhớ (`std::out_of_range`) trong C++.

---

## 1. Mã Nguồn Lỗi (`out_of_bounds_debug.cpp`)

```cpp
#include <cstddef>
#include <iostream>
#include <vector>

int readValue(const std::vector<int>& values, std::size_t index) {
    return values.at(index); // Nơi ném ra exception std::out_of_range
}

int calculateTotal(const std::vector<int>& values) {
    return readValue(values, 5) + 10; // Lỗi: Truyền index = 5
}

int main() {
    const std::vector<int> values{10, 20, 30}; // Kích thước = 3 (index hợp lệ: 0..2)
    std::cout << "Total: " << calculateTotal(values) << '\n';
}
```

---

## 2. Biên Dịch Với Thông Tin Debug

Để GDB hiển thị đúng tên biến, tên hàm và số dòng mã nguồn, cần thêm cờ `-g` khi biên dịch:

```bash
g++ -std=c++20 -g -O1 -Wall -Wextra out_of_bounds_debug.cpp -o app.exe
```

---

## 3. Quy Trình Debug Chi Tiết Bằng GDB

### Bước 1: Khởi động GDB và đặt Breakpoint tại `main`

Mở chương trình bằng debugger và đặt điểm ngắt tại đầu hàm `main`:

```gdb
gdb ./app.exe
(gdb) break main
(gdb) run
```

> **Hiện tượng:** Chương trình thực thi và dừng lại ngay tại dòng khởi tạo vector trong `main()`.

---

### Bước 2: Theo dõi quá trình khởi tạo dữ liệu

Chạy dòng lệnh hiện tại bằng `next` và kiểm tra cấu trúc dữ liệu:

```gdb
(gdb) next
(gdb) print values
```

> **Kết quả:**  
> `$1 = std::vector of length 3, capacity 3 = {10, 20, 30}`  
> ➔ **Nhận xét:** Vector `values` chỉ có **3 phần tử**, dải index hợp lệ là `0` đến `2`.

---

### Bước 3: Đi vào hàm `calculateTotal`

Dùng `step` để chui vào bên trong hàm `calculateTotal` khi lệnh gọi hàm thực thi:

```gdb
(gdb) step
(gdb) info locals
```

> **Hiện tượng:** Con trỏ thực thi chuyển sang hàm `calculateTotal`. Lệnh tiếp theo sẽ là `return readValue(values, 5) + 10;`.  
> ➔ **Phát hiện:** `index = 5` đang được chuẩn bị truyền vào `readValue`.

---

### Bước 4: Kiểm tra giá trị tham số và Call Stack

Tiếp tục `step` vào hàm `readValue` để xác nhận giá trị `index`:

```gdb
(gdb) step
(gdb) print index
```

> **Kết quả:** `$2 = 5`  
> ➔ **Xác nhận nguyên nhân:** `5 >= values.size()` (3), gọi `values.at(5)` sẽ làm chương trình throw exception và crash.

Để kiểm tra xem hàm nào đã gọi đến `readValue` với tham số sai này, xem chuỗi gọi hàm (Call Stack):

```gdb
(gdb) backtrace
(gdb) frame 1
```

> **Giải thích:**
> - `backtrace`: Hiển thị danh sách các frame.
> - `frame 1`: Chuyển ngữ cảnh về lại hàm `calculateTotal` để soi trực tiếp dòng lệnh truyền sai tham số.

---

### Bước 5: Bắt điểm ném Exception (Kỹ thuật nâng cao)

Nếu không biết lỗi xảy ra ở đâu, bạn có thể yêu cầu GDB tự động dừng ngay khi có ngoại lệ xuất hiện:

```gdb
(gdb) catch throw
(gdb) continue
(gdb) backtrace
```

---

## 4. Bảng Tóm Tắt Lệnh GDB Đã Sử Dụng

| Lệnh GDB | Lệnh viết tắt | Chức năng |
| :--- | :--- | :--- |
| `break main` | `b main` | Đặt điểm ngắt tại hàm `main` |
| `run` | `r` | Bắt đầu chạy chương trình |
| `next` | `n` | Chạy sang dòng tiếp theo (không chui vào hàm) |
| `step` | `s` | Chạy sang dòng tiếp theo (chui vào trong hàm) |
| `print index` | `p index` | In giá trị biến `index` |
| `backtrace` | `bt` | In danh sách các hàm đang nằm trong stack frame |
| `frame 1` | `f 1` | Di chuyển đến stack frame số 1 |
| `info locals` | `i lo` | Xem tất cả các biến cục bộ tại frame hiện tại |
| `catch throw` | `catch throw` | Tự động ngắt khi có C++ exception được quăng ra |
| `quit` | `q` | Thoát khỏi GDB |

---

## 5. Khắc Phục Lỗi Mã Nguồn

Thoát GDB (`quit`) và cập nhật lại tham số `index` hợp lệ trong `calculateTotal()` (ví dụ: `2` thay vì `5`):

```cpp
int calculateTotal(const std::vector<int>& values) {
    return readValue(values, 2) + 10; // Sửa index từ 5 thành 2
}
```

Biên dịch và kiểm tra lại với **Sanitizer**:

```bash
g++ -std=c++20 -g -O1 -Wall -Wextra -fsanitize=address,undefined out_of_bounds_debug.cpp -o app.exe
./app.exe
```

> **Kết quả mong đợi:**  
> `Total: 40` (Chương trình chạy mượt mà, không còn lỗi bộ nhớ).

```