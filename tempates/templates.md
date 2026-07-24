# 2.2. Templates trong C++ — Fresher Level

## 1. Template Instantiation và Non-Type Template Parameter

### Template Instantiation

Template là một bản thiết kế chung.

Compiler chỉ sinh ra code cụ thể khi template được sử dụng với một kiểu dữ liệu hoặc giá trị cụ thể.

```cpp
template <typename T>
T add(T a, T b)
{
    return a + b;
}

int main()
{
    int result1 = add<int>(2, 3);
    double result2 = add<double>(2.5, 3.5);
}
```

Compiler sẽ tạo các phiên bản tương ứng cho:

```cpp
add<int>()
add<double>()
```

Thông thường compiler có thể tự suy luận kiểu:

```cpp
int result = add(2, 3);
```

---

### Non-Type Template Parameter

Template parameter không nhất thiết phải là một kiểu dữ liệu. Nó cũng có thể là một giá trị cố định tại compile time.

```cpp
#include <cstddef>

template <typename T, std::size_t N>
class StaticBuffer
{
private:
    T data_[N]{};

public:
    constexpr std::size_t capacity() const noexcept
    {
        return N;
    }

    T& operator[](std::size_t index)
    {
        return data_[index];
    }
};
```

Sử dụng:

```cpp
StaticBuffer<int, 10> numbers;
StaticBuffer<double, 20> values;
```

Trong đó:

* `T` là Type Template Parameter.
* `N` là Non-Type Template Parameter.
* Kích thước mảng được xác định tại compile time.

Hai kiểu sau là hai kiểu dữ liệu khác nhau:

```cpp
StaticBuffer<int, 10>
StaticBuffer<int, 20>
```

---

## 2. Template Specialization

Template Specialization cho phép viết cách xử lý riêng cho một kiểu dữ liệu cụ thể.

### Primary Template

```cpp
#include <iostream>

template <typename T>
struct TypePrinter
{
    static void print()
    {
        std::cout << "General Type\n";
    }
};
```

### Full Specialization

Full Specialization áp dụng cho một kiểu cụ thể.

```cpp
template <>
struct TypePrinter<bool>
{
    static void print()
    {
        std::cout << "Bool Type\n";
    }
};
```

Sử dụng:

```cpp
int main()
{
    TypePrinter<int>::print();
    TypePrinter<bool>::print();
}
```

Kết quả:

```text
General Type
Bool Type
```

### Partial Specialization

Partial Specialization áp dụng cho một nhóm kiểu.

Ví dụ, xử lý tất cả kiểu con trỏ:

```cpp
template <typename T>
struct TypePrinter<T*>
{
    static void print()
    {
        std::cout << "Pointer Type\n";
    }
};
```

Sử dụng:

```cpp
TypePrinter<int*>::print();
TypePrinter<double*>::print();
```

### Điểm cần nhớ

* Class Template hỗ trợ Full Specialization.
* Class Template hỗ trợ Partial Specialization.
* Function Template không hỗ trợ Partial Specialization.
* Với function template, thường dùng function overloading.

Ví dụ:

```cpp
template <typename T>
void process(T value)
{
    std::cout << "General Function\n";
}

template <typename T>
void process(T* value)
{
    std::cout << "Pointer Function\n";
}
```

---

## 3. Variadic Templates và Fold Expression

Variadic Template cho phép hàm nhận số lượng đối số không cố định.

```cpp
template <typename... Args>
void function(Args... args)
{
}
```

`Args...` được gọi là Parameter Pack.

### Tính tổng nhiều giá trị

Từ C++17 có thể sử dụng Fold Expression:

```cpp
template <typename... Args>
auto sum(Args... args)
{
    return (... + args);
}
```

Sử dụng:

```cpp
#include <iostream>

int main()
{
    std::cout << sum(1, 2, 3, 4, 5) << '\n';
}
```

Kết quả:

```text
15
```

Biểu thức:

```cpp
(... + args)
```

được hiểu gần giống:

```cpp
((((1 + 2) + 3) + 4) + 5)
```

### In nhiều giá trị

```cpp
template <typename... Args>
void printAll(const Args&... args)
{
    (std::cout << ... << args) << '\n';
}
```

Sử dụng:

```cpp
printAll("Error: ", 404, " - Not Found");
```

Kết quả:

```text
Error: 404 - Not Found
```

---

## 4. Type Traits cơ bản

Type Traits giúp kiểm tra đặc điểm của kiểu dữ liệu tại compile time.

Header:

```cpp
#include <type_traits>
```

Một số Type Traits Fresher nên biết:

```cpp
std::is_integral_v<T>
std::is_floating_point_v<T>
std::is_pointer_v<T>
std::is_same_v<T, U>
```

### Kiểm tra kiểu số nguyên

```cpp
#include <iostream>
#include <type_traits>

template <typename T>
void inspectType(T value)
{
    if constexpr (std::is_integral_v<T>)
    {
        std::cout << "Integral type\n";
    }
    else
    {
        std::cout << "Not an integral type\n";
    }
}
```

Sử dụng:

```cpp
inspectType(10);
inspectType(3.14);
```

Kết quả:

```text
Integral type
Not an integral type
```

### Kiểm tra hai kiểu giống nhau

```cpp
template <typename T>
void checkType(T value)
{
    if constexpr (std::is_same_v<T, float>)
    {
        std::cout << "T is float\n";
    }
}
```

### `if constexpr`

`if constexpr` được giới thiệu từ C++17.

Điều kiện được kiểm tra tại compile time. Nhánh không phù hợp sẽ không được compiler tạo code.

```cpp
if constexpr (std::is_integral_v<T>)
{
    // Chỉ compile khi T là kiểu số nguyên.
}
```

---

## 5. SFINAE và C++20 Concepts

Ở Fresher level, chỉ cần hiểu mục đích chung:

> Giới hạn template để nó chỉ chấp nhận một số kiểu dữ liệu nhất định.

### SFINAE với `std::enable_if_t`

```cpp
#include <type_traits>

template <typename T>
std::enable_if_t<std::is_integral_v<T>, T>
addIntegers(T a, T b)
{
    return a + b;
}
```

Sử dụng:

```cpp
int result = addIntegers(5, 10);
```

Đoạn sau không hợp lệ vì `double` không phải kiểu số nguyên:

```cpp
// addIntegers(3.14, 2.5);
```

Fresher chỉ cần biết:

* SFINAE thường dùng để bật hoặc tắt một function template.
* `std::enable_if_t` là cách phổ biến trước C++20.
* Cú pháp SFINAE khá khó đọc.

---

### C++20 Concepts

Concepts là cách hiện đại và dễ đọc hơn để giới hạn template.

```cpp
#include <concepts>

template <std::integral T>
T addIntegersModern(T a, T b)
{
    return a + b;
}
```

Sử dụng:

```cpp
int result = addIntegersModern(5, 10);
```

Có thể viết bằng `requires`:

```cpp
template <typename T>
requires std::integral<T>
T multiplyIntegers(T a, T b)
{
    return a * b;
}
```

### So sánh ngắn

SFINAE:

```cpp
template <typename T>
std::enable_if_t<std::is_integral_v<T>, T>
add(T a, T b)
{
    return a + b;
}
```

Concepts:

```cpp
template <std::integral T>
T add(T a, T b)
{
    return a + b;
}
```
