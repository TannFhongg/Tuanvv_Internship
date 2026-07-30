/* 
Viết factory trả về std::unique_ptr<Base> và dùng polymorphism mà không manual delete.
*/

#include <cstddef>
#include <iostream>
#include <string>
#include <utility>

// UniquePtr<T> owns a dynamically allocated array created with new[].
template <typename T>
class UniquePtr
{
private:
    T* data_;
    std::size_t size_;

public:
    UniquePtr() noexcept : data_(nullptr), size_(0) {}

    explicit UniquePtr(std::size_t size)
        : data_(size == 0 ? nullptr : new T[size]), size_(size) {}

    ~UniquePtr()
    {
        delete[] data_;
    }

    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    UniquePtr(UniquePtr&& other) noexcept
        : data_(other.data_), size_(other.size_)
    {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    UniquePtr& operator=(UniquePtr&& other) noexcept
    {
        if (this != &other)
        {
            delete[] data_;

            data_ = other.data_;
            size_ = other.size_;

            other.data_ = nullptr;
            other.size_ = 0;
        }

        return *this;
    }

    T* get() const noexcept
    {
        return data_;
    }

    // Stop owning the array. The caller must later delete[] the returned pointer
    // or transfer it to another UniquePtr with reset().
    T* release() noexcept
    {
        T* releasedData = data_;
        data_ = nullptr;
        size_ = 0;
        return releasedData;
    }

    void reset(T* newData = nullptr, std::size_t newSize = 0) noexcept
    {
        if (data_ == newData)
        {
            return;
        }

        delete[] data_;
        data_ = newData;
        size_ = newSize;
    }

    T& operator*() const
    {
        return *data_;
    }

    T* operator->() const noexcept
    {
        return data_;
    }

    std::size_t size() const noexcept
    {
        return size_;
    }
};

struct Student
{
    std::string name;
    int age{};
};

int main()
{
    UniquePtr<Student> first(1);
    first->name = "Tuan";
    first->age = 20;

    std::cout << (*first).name << ": " << first->age << '\n';

    UniquePtr<Student> second = std::move(first); // move constructor
    std::cout << "first.get() = " << first.get() << '\n';

    Student* rawStudent = second.release(); // second no longer owns it
    UniquePtr<Student> third;
    third.reset(rawStudent, 1); // third owns it now

    UniquePtr<Student> finalOwner;
    finalOwner = std::move(third); // move assignment
    std::cout << finalOwner->name << " is " << finalOwner->age << " years old\n";

    return 0;
}
