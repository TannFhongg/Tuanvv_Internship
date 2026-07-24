

## 1. Value Categories và References

Value Category mô tả một biểu thức là giá trị lâu dài hay giá trị tạm thời.

### Lvalue

Lvalue là giá trị có danh tính, thường có tên và địa chỉ bộ nhớ rõ ràng.

```cpp
int x = 10;
std::string name = "Tuan";
```

Trong ví dụ trên:

* `x` là lvalue.
* `name` là lvalue.

Lvalue thường vẫn tồn tại sau khi câu lệnh hiện tại kết thúc.

### Rvalue

Rvalue thường là giá trị tạm thời, không có tên và sắp bị hủy.

```cpp
10
x + 5
std::string("Hello")
```

Ví dụ:

```cpp
int result = x + 5;
```

Biểu thức `x + 5` tạo ra một giá trị tạm thời.

---

### Lvalue Reference

Lvalue reference có cú pháp:

```cpp
T&
```

Nó thường tham chiếu đến một lvalue.

```cpp
int x = 10;
int& reference = x;

reference = 20;

std::cout << x; // 20
```

`reference` và `x` cùng tham chiếu đến một vùng nhớ.

---

### Rvalue Reference

Rvalue reference được giới thiệu từ C++11 với cú pháp:

```cpp
T&&
```

Nó có thể tham chiếu đến giá trị tạm thời hoặc object đã được chuyển thành rvalue bằng `std::move`.

```cpp
#include <utility>

int x = 10;

int& lvalueReference = x;

// Không hợp lệ vì x là lvalue.
// int&& invalidReference = x;

int&& temporaryReference = 20;

int&& movedReference = std::move(x);
```

Điểm cần nhớ:

```cpp
T&  // Lvalue reference
T&& // Rvalue reference
```

---

## 2. Move Semantics

Move Semantics cho phép chuyển tài nguyên từ object này sang object khác thay vì sao chép toàn bộ tài nguyên.

Điều này đặc biệt hữu ích với object quản lý:

* Mảng động.
* Chuỗi lớn.
* Bộ nhớ heap.
* File handle.
* Socket.
* Tài nguyên hệ thống.

### Copy và Move

Copy tạo ra một tài nguyên mới:

```cpp
std::string source = "Hello";
std::string destination = source;
```

Sau khi copy:

* `source` vẫn giữ dữ liệu.
* `destination` có một bản sao dữ liệu.

Move chuyển tài nguyên sang object mới:

```cpp
std::string source = "Hello";
std::string destination = std::move(source);
```

Sau khi move:

* `destination` nhận tài nguyên.
* `source` vẫn là object hợp lệ.
* Giá trị cụ thể của `source` không được đảm bảo.

---

### Trạng thái sau khi Move

Object sau khi bị move thường ở trạng thái:

> **Valid but unspecified**

Nghĩa là:

* Object vẫn có thể bị hủy an toàn.
* Có thể gán giá trị mới cho object.
* Không nên phụ thuộc vào giá trị cũ của object.

```cpp
std::string source = "Hello";
std::string destination = std::move(source);

// Hợp lệ
source = "New value";

// Không nên phụ thuộc source còn chứa gì ngay sau move.
```

---

## 3. Move Constructor và Move Assignment

Khi class tự quản lý tài nguyên bằng raw pointer, có thể cần tự viết Move Constructor và Move Assignment Operator.

```cpp
#include <cstddef>
#include <utility>

class DynamicBuffer
{
private:
    std::size_t size_;
    int* data_;

public:
    explicit DynamicBuffer(std::size_t size)
        : size_(size),
          data_(new int[size]{})
    {
    }

    ~DynamicBuffer()
    {
        delete[] data_;
    }

    // Không cho phép copy để tránh shallow copy.
    DynamicBuffer(const DynamicBuffer&) = delete;

    DynamicBuffer& operator=(const DynamicBuffer&) = delete;

    // Move Constructor
    DynamicBuffer(DynamicBuffer&& other) noexcept
        : size_(other.size_),
          data_(other.data_)
    {
        other.size_ = 0;
        other.data_ = nullptr;
    }

    // Move Assignment Operator
    DynamicBuffer& operator=(DynamicBuffer&& other) noexcept
    {
        if (this != &other)
        {
            delete[] data_;

            size_ = other.size_;
            data_ = other.data_;

            other.size_ = 0;
            other.data_ = nullptr;
        }

        return *this;
    }
};
```

### Move Constructor

Move Constructor có dạng:

```cpp
ClassName(ClassName&& other);
```

Nó được gọi khi tạo object mới từ một rvalue.

```cpp
DynamicBuffer first(100);

DynamicBuffer second(std::move(first));
```

`second` nhận tài nguyên của `first`.

### Move Assignment Operator

Move Assignment có dạng:

```cpp
ClassName& operator=(ClassName&& other);
```

Nó được gọi khi object đích đã tồn tại.

```cpp
DynamicBuffer first(100);
DynamicBuffer second(50);

second = std::move(first);
```

Trước khi nhận tài nguyên mới, `second` phải giải phóng tài nguyên cũ của nó.

---

## 4. Bản chất của `std::move`

Header:

```cpp
#include <utility>
```

`std::move` không tự di chuyển dữ liệu.

Nó chỉ chuyển một biểu thức thành rvalue để compiler có thể chọn Move Constructor hoặc Move Assignment Operator.

```cpp
std::string text = "Hello";

std::move(text);
```

Đoạn trên chưa thực sự di chuyển dữ liệu vì không có object nào nhận kết quả.

```cpp
std::string destination = std::move(text);
```

Lúc này Move Constructor của `std::string` có thể được gọi.

Có thể hiểu đơn giản:

```cpp
std::move(object)
```

gần giống một phép ép kiểu thành rvalue reference:

```cpp
static_cast<T&&>(object)
```

### Ý nghĩa của `std::move`

Khi viết:

```cpp
std::move(object)
```

lập trình viên đang thể hiện rằng:

> Tôi không còn cần giữ giá trị hiện tại của object này và cho phép chuyển tài nguyên của nó.

---

## 5. `noexcept` với Move Operation

Move Constructor và Move Assignment thường nên được đánh dấu `noexcept` nếu chúng thực sự không thể ném exception.

```cpp
DynamicBuffer(DynamicBuffer&& other) noexcept;
```

```cpp
DynamicBuffer& operator=(DynamicBuffer&& other) noexcept;
```

### Tại sao `noexcept` quan trọng?

Khi `std::vector` hết dung lượng, nó phải:

1. Cấp phát vùng nhớ mới.
2. Chuyển các phần tử từ vùng nhớ cũ sang vùng nhớ mới.
3. Giải phóng vùng nhớ cũ.

Nếu Move Constructor có `noexcept`, `std::vector` có thể ưu tiên move các phần tử.

Nếu Move Constructor có khả năng ném exception và kiểu dữ liệu vẫn copy được, `std::vector` có thể chọn copy để bảo vệ dữ liệu cũ.

```cpp
class Item
{
public:
    Item(Item&& other) noexcept
    {
        // Move tài nguyên.
    }
};
```

Không nên thêm `noexcept` nếu bên trong hàm vẫn có thao tác có thể ném exception.

---

## 6. Rule of Five

Nếu một class tự quản lý tài nguyên thô như raw pointer, file handle hoặc socket, cần xem xét năm hàm đặc biệt:

1. Destructor.
2. Copy Constructor.
3. Copy Assignment Operator.
4. Move Constructor.
5. Move Assignment Operator.

```cpp
class Resource
{
public:
    ~Resource();

    Resource(const Resource& other);

    Resource& operator=(const Resource& other);

    Resource(Resource&& other) noexcept;

    Resource& operator=(Resource&& other) noexcept;
};
```

Điều này được gọi là:

> **Rule of Five**

Tuy nhiên, trong Modern C++, nên hạn chế tự quản lý tài nguyên bằng raw pointer.

---

## 7. Rule of Zero

Rule of Zero khuyên sử dụng các class đã tự quản lý tài nguyên như:

* `std::string`
* `std::vector`
* `std::unique_ptr`
* `std::shared_ptr`

Khi đó class thường không cần tự viết:

* Destructor.
* Copy Constructor.
* Copy Assignment.
* Move Constructor.
* Move Assignment.

```cpp
#include <memory>
#include <string>
#include <utility>
#include <vector>

class UserProfile
{
private:
    std::string name_;
    std::vector<int> scores_;
    std::unique_ptr<int> customId_;

public:
    explicit UserProfile(std::string name)
        : name_(std::move(name)),
          customId_(std::make_unique<int>(0))
    {
    }
};
```

Class trên không trực tiếp dùng `new` hoặc `delete`.

### Lưu ý với `std::unique_ptr`

Vì `std::unique_ptr` không cho phép copy nên `UserProfile` cũng không thể copy mặc định.

```cpp
UserProfile first("Tuan");

// Không hợp lệ vì class chứa unique_ptr.
// UserProfile second = first;
```

Tuy nhiên, class vẫn có thể được move:

```cpp
UserProfile first("Tuan");
UserProfile second = std::move(first);
```

Rule of Zero không có nghĩa compiler luôn tạo đầy đủ cả copy và move.

Nó có nghĩa là class để các member tự quản lý tài nguyên thay vì tự viết logic giải phóng tài nguyên thủ công.

---

## 8. Structured Bindings

Structured Bindings được giới thiệu từ C++17.

Nó cho phép tách dữ liệu từ:

* `struct`
* `std::pair`
* `std::tuple`
* Array
* Phần tử của `std::map` hoặc `std::unordered_map`

Cú pháp:

```cpp
auto [first, second] = object;
```

---

### Structured Binding với Struct

```cpp
#include <iostream>

struct Point
{
    int x;
    int y;
};

int main()
{
    Point point{10, 20};

    auto [x, y] = point;

    std::cout << x << ' ' << y;
}
```

`x` và `y` là các bản sao.

Thay đổi chúng không làm thay đổi `point`.

```cpp
x = 100;

// point.x vẫn bằng 10.
```

---

### Binding bằng Reference

Dùng `auto&` để tham chiếu trực tiếp đến dữ liệu gốc.

```cpp
Point point{10, 20};

auto& [x, y] = point;

x = 100;

// point.x bây giờ bằng 100.
```

---

### Duyệt Map

Structured Bindings thường được dùng khi duyệt `std::map` hoặc `std::unordered_map`.

```cpp
#include <iostream>
#include <string>
#include <unordered_map>

int main()
{
    std::unordered_map<std::string, int> tracker{
        {"FPS", 60},
        {"Latency", 15}
    };

    for (const auto& [key, value] : tracker)
    {
        std::cout << key << ": " << value << '\n';
    }
}
```

Nên dùng:

```cpp
const auto& [key, value]
```

để tránh copy từng phần tử trong container.

---

# Mã nguồn thực hành tổng hợp

```cpp
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class UserProfile
{
private:
    std::string name_;
    std::vector<int> scores_;
    std::unique_ptr<int> customId_;

public:
    explicit UserProfile(std::string name)
        : name_(std::move(name)),
          customId_(std::make_unique<int>(0))
    {
    }

    void addScore(int score)
    {
        scores_.push_back(score);
    }

    void display() const
    {
        std::cout << "Name: " << name_ << '\n';

        std::cout << "Scores: ";

        for (int score : scores_)
        {
            std::cout << score << ' ';
        }

        std::cout << '\n';
    }
};

struct Point
{
    int x;
    int y;
};

int main()
{
    // Lvalue và Rvalue Reference
    int number = 10;

    int& lvalueReference = number;
    int&& rvalueReference = 20;

    std::cout << lvalueReference << '\n';
    std::cout << rvalueReference << '\n';

    // Move Semantics
    std::string source = "Hello Modern C++";
    std::string destination = std::move(source);

    std::cout << destination << '\n';

    // Rule of Zero
    UserProfile first("Tuan");
    first.addScore(90);
    first.addScore(85);

    UserProfile second = std::move(first);
    second.display();

    // Structured Bindings với Struct
    Point point{10, 20};

    auto& [x, y] = point;
    x = 100;

    std::cout << "Point: " << point.x << ", " << point.y << '\n';

    // Structured Bindings với unordered_map
    std::unordered_map<std::string, int> tracker{
        {"FPS", 60},
        {"Latency", 15}
    };

    for (const auto& [key, value] : tracker)
    {
        std::cout << key << ": " << value << '\n';
    }

    return 0;
}
```
---

# Nội dung cần nhớ

1. Lvalue thường là biến có tên và tồn tại lâu dài.
2. Rvalue thường là giá trị tạm thời.
3. `T&` là lvalue reference.
4. `T&&` là rvalue reference.
5. Move Semantics chuyển tài nguyên thay vì sao chép.
6. Object sau khi move vẫn hợp lệ nhưng không nên phụ thuộc vào giá trị cũ.
7. `std::move` chỉ chuyển biểu thức thành rvalue.
8. Move Constructor tạo object mới bằng cách nhận tài nguyên.
9. Move Assignment chuyển tài nguyên vào object đã tồn tại.
10. Move operation nên dùng `noexcept` khi thực sự không ném exception.
11. Rule of Five áp dụng khi class tự quản lý tài nguyên.
12. Rule of Zero nên được ưu tiên trong Modern C++.
13. Structured Bindings giúp tách dữ liệu từ struct, pair và map.

## Chưa cần học sâu

* Chi tiết đầy đủ về `glvalue`, `prvalue` và `xvalue`.
* Reference collapsing.
* Forwarding Reference.
* Perfect Forwarding.
* `std::forward`.
* Template deduction nâng cao.
* Cách triển khai nội bộ của `std::move`.
* Exception Guarantee chi tiết của các container.
