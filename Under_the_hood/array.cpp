/*
Thực hành:

Viết DynamicArray<T> đơn giản:
push_back
pop_back
operator[]
reserve
destructor
In địa chỉ các phần tử để thấy chúng nằm liên tục.
Theo dõi size() và capacity() của std::vector.
*/
#include <iostream>

template <typename T>
class DynamicArray
{
private:
    T *data;
    size_t size_;
    size_t capacity_;

public:
    DynamicArray() : data(nullptr), size_(0), capacity_(0) {}

    ~DynamicArray()
    {
        delete[] data;
    }

    void push_back(const T &value)
    {
        if (size_ >= capacity_)
        {
            reserve(capacity_ == 0 ? 1 : capacity_ * 2);
        }
        data[size_++] = value;
    }

    void pop_back()
    {
        if (size_ > 0)
        {
            size_--;
        }
    }

    T &operator[](size_t index)
    {
        return data[index];
    }

    const T &operator[](size_t index) const
    {
        return data[index];
    }

    void reserve(size_t new_capacity)
    {
        if (new_capacity > capacity_)
        {
            T *new_data = new T[new_capacity];
            for (size_t i = 0; i < size_; i++)
            {
                new_data[i] = data[i];
            }
            delete[] data;
            data = new_data;
            capacity_ = new_capacity;
        }
    }

    size_t size() const
    {
        return size_;
    }

    size_t capacity() const
    {
        return capacity_;
    }
};

int main()
{
    DynamicArray<int> arr;
    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);

    for (size_t i = 0; i < arr.size(); ++i)
    {
        std::cout << "Element at index " << i << ": " << arr[i] << ", Address: " << &arr[i] << std::endl;
    }

    std::cout << "Size: " << arr.size() << ", Capacity: " << arr.capacity() << std::endl;

    arr.pop_back();
    std::cout << "After pop_back, Size: " << arr.size() << ", Capacity: " << arr.capacity() << std::endl;

    return 0;
}