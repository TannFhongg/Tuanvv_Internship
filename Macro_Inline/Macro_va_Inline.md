# Macro (`#define`) và `inline` trong C++

## 1. Bản chất và cơ chế bên dưới

### Macro (`#define`)

- **Thời điểm xử lý:** Preprocessing phase — trước cả khi compiler phân tích cú pháp.
- **Cơ chế:** Cắt-dán văn bản đơn thuần (*textual substitution*). Preprocessor không biết gì về kiểu dữ liệu (*type*), phạm vi (*namespace/scope*) hay cú pháp C++.
- **Tác động:** Tạo ra mã nguồn trung gian (file `.i`) trước khi chuyển sang compiler.

### `inline` (C++ hiện đại)

- **Thời điểm xử lý:** Compilation và linking phase.
- **Cơ chế gốc trong C++:**
  - **Linkage / ODR (One Definition Rule):** Báo cho linker biết hàm hoặc biến này có thể xuất hiện ở nhiều file `.cpp` (*translation units* — TU) khác nhau mà không gây lỗi `duplicate symbol`. Linker sẽ gộp chúng thành một bản duy nhất trong binary.
  - **Code expansion (tùy chọn):** Gợi ý compiler thay thế lời gọi hàm bằng chính thân hàm để triệt tiêu chi phí gọi hàm (*function-call overhead*: push/pop stack, jump).
- **Thực tế:** Compiler hiện đại (GCC, Clang, MSVC) tự quyết định có inline mã hay không, dựa trên heuristic như kích thước hàm, vòng lặp và cờ tối ưu hóa (`-O2`, `-O3`), bất kể có viết từ khóa `inline` hay không.

## 2. So sánh nhanh

| Tiêu chí | Macro (`#define`) | `inline` |
| --- | --- | --- |
| Giai đoạn xử lý | Preprocessing | Compilation và linking |
| Cách hoạt động | Thay thế văn bản | Quy tắc ODR/linkage; compiler có thể mở rộng thân hàm |
| Kiểm tra kiểu | Không | Có |
| Phạm vi C++ | Không tuân theo scope/namespace | Tuân theo scope/namespace |
| Inlining mã | Luôn thay thế văn bản trước biên dịch | Chỉ là gợi ý; compiler quyết định |
| Mục đích chính | Hằng số, macro có điều kiện, tiện ích tiền xử lý | Định nghĩa hàm/biến trong header an toàn theo ODR |

> Trong C++ hiện đại, ưu tiên `constexpr`, `const`, template và `inline` thay cho macro khi có thể. Macro vẫn phù hợp cho các tác vụ bắt buộc ở preprocessing, chẳng hạn `#include` hoặc biên dịch có điều kiện (`#if`, `#ifdef`).
