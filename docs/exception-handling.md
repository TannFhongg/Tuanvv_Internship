# Cơ chế Exception trong C++

## 1. Cơ chế Stack Unwinding

Khi một exception được ném ra bằng `throw`, chương trình không thể tiếp tục chạy theo luồng tuyến tính thông thường mà phải tìm khối `catch` phù hợp.

Quá trình này được gọi là **Stack Unwinding** — tháo gỡ các stack frame.

### Luồng thực thi diễn ra như thế nào?

#### Bước 1: Khởi tạo Exception Object

Khi lệnh sau được thực thi:

```cpp
throw my_exception;
```

C++ Runtime sẽ cấp phát một vùng nhớ đặc biệt để lưu trữ đối tượng exception.

Vùng nhớ này thường nằm trong:

* Heap dành riêng cho exception.
* Vùng nhớ do runtime quản lý.

#### Bước 2: Dừng thực thi tại vị trí `throw`

Luồng thực thi hiện tại dừng lại ngay lập tức.

Các câu lệnh nằm sau `throw` trong phạm vi hiện tại sẽ không được thực thi.

#### Bước 3: Duyệt ngược Call Stack

Runtime bắt đầu kiểm tra stack frame của hàm hiện tại:

* Hàm hiện tại có nằm trong một khối `try` hay không.
* Khối `try` đó có `catch` tương thích với kiểu exception hay không.

Nếu không tìm thấy `catch` phù hợp:

1. Stack frame của hàm hiện tại bị tháo gỡ.
2. Destructor của tất cả các object cục bộ được gọi.
3. Destructor được gọi theo thứ tự ngược lại với thứ tự khởi tạo.
4. Runtime quay lại hàm gọi nó — caller.
5. Quá trình tiếp tục cho đến khi tìm được `catch` phù hợp.

Ví dụ:

```cpp
void functionB()
{
    Resource resourceB;
    throw std::runtime_error("Error");
}

void functionA()
{
    Resource resourceA;
    functionB();
}
```

Khi `functionB()` ném exception:

1. Destructor của `resourceB` được gọi.
2. Runtime quay lại `functionA()`.
3. Destructor của `resourceA` được gọi nếu `functionA()` không xử lý exception.
4. Runtime tiếp tục tìm `catch` ở các stack frame phía trên.

#### Bước 4: Bắt được Exception

Quá trình stack unwinding dừng lại khi tìm thấy một khối `catch` có kiểu dữ liệu phù hợp.

Con trỏ thực thi được chuyển đến câu lệnh đầu tiên bên trong khối `catch`.

```cpp
try
{
    functionA();
}
catch (const std::runtime_error& ex)
{
    std::cout << ex.what();
}
```

#### Bước 5: Không tìm thấy `catch`

Nếu quá trình unwinding đi đến tận `main()` mà vẫn không tìm thấy khối `catch` phù hợp, C++ Runtime sẽ gọi:

```cpp
std::terminate();
```

Kết quả là chương trình kết thúc bất thường.

---

## 2. Compiler triển khai Exception như thế nào?

### Itanium ABI và DWARF

Trên các hệ thống Linux, Unix, GCC và Clang sử dụng **Itanium C++ ABI**, cơ chế xử lý exception hiện đại thường được triển khai theo mô hình:

> **Zero-Cost Exception Handling**

“Zero-cost” có nghĩa là khi không có exception nào được ném, chương trình gần như không phải chịu thêm chi phí kiểm tra exception trong luồng thực thi thông thường.

### Các bảng tra cứu do Compiler tạo ra

Compiler tạo ra các bảng metadata ẩn bên trong file nhị phân như ELF hoặc PE.

Hai thành phần thường gặp là:

* `.eh_frame`
* LSDA — Language-Specific Data Area

Các bảng này chứa thông tin như:

* Địa chỉ của các khối `try`.
* Vị trí của các khối `catch`.
* Kiểu exception mà từng khối `catch` có thể xử lý.
* Danh sách các object cục bộ cần gọi destructor.
* Phạm vi địa chỉ lệnh tương ứng với từng thao tác cleanup.
* Địa chỉ lệnh mà chương trình cần chuyển đến sau khi xử lý.

### Khi chương trình chạy bình thường

Trong **happy path**, khi không có exception:

* Không cần kiểm tra `if/else` ẩn sau mỗi câu lệnh.
* Không cần liên tục kiểm tra trạng thái lỗi.
* Code chạy gần với tốc độ gốc.

### Khi Exception xảy ra

Trong **sad path**, khi exception được ném:

1. C++ Runtime như `libstdc++` hoặc `libc++abi` đọc địa chỉ lệnh hiện tại.
2. Runtime tra cứu `.eh_frame` và LSDA.
3. Runtime xác định các destructor cần được gọi.
4. Runtime tháo gỡ từng stack frame.
5. Runtime tìm địa chỉ của `catch` phù hợp.
6. Con trỏ lệnh được chuyển đến khối `catch`.

Thông qua metadata, runtime có thể biết:

* Destructor nào cần được gọi tại vị trí hiện tại.
* Stack frame nào cần được loại bỏ.
* Khối `catch` nào có thể xử lý exception.
* Địa chỉ lệnh tiếp theo cần nhảy tới.

---

## 3. Vì sao không nên ném Exception từ Destructor?

Đây là một quy tắc quan trọng trong thiết kế hệ thống C++ và Embedded C++.

Hãy xem xét kịch bản sau:

1. Hàm `A()` đang ném exception `E1`.
2. Quá trình stack unwinding bắt đầu.
3. Một object cục bộ `obj` trên stack bị hủy.
4. Destructor `~Obj()` được gọi tự động.
5. Bên trong destructor lại ném thêm exception `E2`.

Lúc này tồn tại hai exception trên cùng một thread:

* `E1` đang được xử lý bởi quá trình stack unwinding.
* `E2` vừa được ném ra từ destructor.

C++ Runtime không thể tiếp tục tháo gỡ stack một cách an toàn.

Kết quả là runtime gọi ngay:

```cpp
std::terminate();
```

Chương trình sẽ kết thúc và exception mới không thể được xử lý bằng `catch` thông thường.

### Destructor và `noexcept`

Từ C++11 trở đi, destructor mặc định được xem như có đặc tính:

```cpp
noexcept
```

Ví dụ:

```cpp
class Resource
{
public:
    ~Resource() noexcept
    {
        // Không được để exception thoát ra ngoài destructor.
    }
};
```

Nếu một exception thoát ra khỏi destructor được đánh dấu `noexcept`, chương trình sẽ gọi:

```cpp
std::terminate();
```

### Cách xử lý lỗi bên trong Destructor

Nếu một thao tác trong destructor có thể ném exception, destructor nên tự xử lý exception đó.

```cpp
class Resource
{
public:
    ~Resource() noexcept
    {
        try
        {
            release();
        }
        catch (...)
        {
            // Ghi log hoặc xử lý lỗi nội bộ.
            // Không để exception thoát ra ngoài destructor.
        }
    }

private:
    void release();
};
```

---

## 4. Bắt Exception bằng `const std::exception&`

Khi viết khối `catch`, nên bắt exception bằng **tham chiếu hằng**:

```cpp
const std::exception&
```

Cách này giúp:

* Không tạo bản sao exception.
* Tránh object slicing.
* Giữ nguyên tính đa hình.
* Giảm chi phí copy.
* Cho phép `what()` hoạt động đúng với kiểu exception thực tế.

### Không nên: Catch by Value

```cpp
try
{
    throw std::runtime_error("Disk full!");
}
catch (std::exception ex)
{
    std::cout << ex.what();
}
```

Trong trường hợp này, `std::runtime_error` được copy thành một object `std::exception`.

Phần thông tin thuộc lớp dẫn xuất có thể bị mất. Hiện tượng này được gọi là:

> **Object Slicing**

### Nên dùng: Catch by Const Reference

```cpp
try
{
    throw std::runtime_error("Disk full!");
}
catch (const std::exception& ex)
{
    std::cout << ex.what();
}
```

Cách này:

* Không copy object.
* Giữ nguyên kiểu động của exception.
* Giữ nguyên tính đa hình.
* `ex.what()` trả về đúng thông báo của `std::runtime_error`.

### Quy tắc phổ biến

```cpp
try
{
    // Code có thể ném exception.
}
catch (const std::runtime_error& ex)
{
    // Xử lý lỗi runtime cụ thể.
}
catch (const std::exception& ex)
{
    // Xử lý các exception chuẩn khác.
}
catch (...)
{
    // Xử lý các exception không xác định.
}
```

Nên đặt các kiểu exception cụ thể trước kiểu tổng quát.

---

## 5. Ba cấp độ Exception Safety

Khi thiết kế hàm hoặc class, đặc biệt là các thành phần quản lý tài nguyên, code nên cung cấp một trong ba cấp độ an toàn exception.

| Cấp độ                 | Định nghĩa                                                                                                                                             | Ví dụ                                                                                                                                 |
| ---------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------- |
| **Basic Guarantee**    | Nếu exception xảy ra, chương trình không bị rò rỉ tài nguyên và các object vẫn ở trạng thái hợp lệ. Tuy nhiên, dữ liệu có thể đã bị thay đổi một phần. | Một số phần tử đã bị xóa khỏi danh sách trước khi lỗi xảy ra. Danh sách vẫn sử dụng được nhưng dữ liệu không còn giống ban đầu.       |
| **Strong Guarantee**   | Thao tác hoàn thành hoàn toàn hoặc không thay đổi trạng thái ban đầu. Còn gọi là **commit or rollback**.                                               | Nếu `std::vector::push_back()` thất bại trong quá trình cấp phát lại bộ nhớ, vector thường giữ nguyên dữ liệu và kích thước trước đó. |
| **No-throw Guarantee** | Hàm đảm bảo không để exception thoát ra ngoài. Thường được biểu diễn bằng `noexcept`.                                                                  | Destructor, hàm giải phóng tài nguyên, `swap()` và một số move constructor.                                                           |

### 5.1. Basic Guarantee

Basic Guarantee đảm bảo:

* Không rò rỉ bộ nhớ.
* Không rò rỉ file descriptor.
* Không rò rỉ mutex hoặc tài nguyên hệ thống.
* Object vẫn ở trạng thái hợp lệ.
* Object vẫn có thể bị hủy hoặc tiếp tục sử dụng.

Tuy nhiên, trạng thái của object có thể đã thay đổi.

```cpp
void processItems(std::vector<int>& values)
{
    while (!values.empty())
    {
        process(values.back());
        values.pop_back();
    }
}
```

Nếu `process()` ném exception sau khi một số phần tử đã được xử lý, vector vẫn hợp lệ nhưng đã mất một phần dữ liệu.

### 5.2. Strong Guarantee

Strong Guarantee đảm bảo:

> Hoặc thao tác thành công hoàn toàn, hoặc object giữ nguyên trạng thái như trước khi hàm được gọi.

Một kỹ thuật phổ biến là tạo dữ liệu tạm, sau đó chỉ commit khi mọi thao tác đều thành công.

```cpp
void updateData(std::vector<int>& destination)
{
    std::vector<int> temporary = destination;

    temporary.push_back(10);
    temporary.push_back(20);

    destination.swap(temporary);
}
```

Nếu việc thêm phần tử vào `temporary` thất bại, `destination` không bị thay đổi.

### 5.3. No-throw Guarantee

No-throw Guarantee đảm bảo hàm không bao giờ để exception thoát ra ngoài.

```cpp
void cleanup() noexcept
{
    // Hàm không được ném exception ra ngoài.
}
```

Các hàm thường cần no-throw guarantee gồm:

* Destructor.
* Hàm cleanup.
* Hàm giải phóng bộ nhớ.
* `swap()`.
* Move constructor.
* Move assignment operator.
* Một số callback cấp thấp hoặc hàm hệ thống.

Ví dụ:

```cpp
class Resource
{
public:
    Resource(Resource&& other) noexcept
        : handle_(other.handle_)
    {
        other.handle_ = nullptr;
    }

    ~Resource() noexcept
    {
        release();
    }

private:
    void* handle_ = nullptr;

    void release() noexcept
    {
        // Giải phóng tài nguyên.
    }
};
```

---

## Tổng kết

Các nguyên tắc quan trọng khi làm việc với exception trong C++:

1. `throw` kích hoạt quá trình stack unwinding.
2. Destructor của object cục bộ được gọi trong quá trình unwinding.
3. Nếu không có `catch` phù hợp, `std::terminate()` được gọi.
4. Cơ chế zero-cost giúp luồng chạy bình thường gần như không chịu overhead.
5. Không để exception thoát ra khỏi destructor.
6. Luôn ưu tiên bắt exception bằng `const reference`.
7. Tránh catch by value để không xảy ra object slicing.
8. Thiết kế code theo Basic, Strong hoặc No-throw Guarantee.
9. Sử dụng RAII để tài nguyên được giải phóng tự động khi exception xảy ra.
10. Đánh dấu `noexcept` cho các hàm thực sự cam kết không ném exception.
