/*
Viết Buffer sở hữu dynamic array, log copy/move/destructor; dùng trong std::vector<Buffer> để quan sát reallocation.
*/

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <vector>

class Buffer {
private:
    std::size_t size_;
    int* data_;

public:
    Buffer(std::size_t size)
        : size_(size),
          data_(new int[size]) {
        std::cout << "Create Buffer with size = "
                  << size_ << '\n';
    }

    // Copy constructor: tạo vùng nhớ mới.
    Buffer(const Buffer& other)
        : size_(other.size_),
          data_(new int[other.size_]) {
        std::copy(
            other.data_,
            other.data_ + size_,
            data_
        );

        std::cout << "Copy Buffer with size = "
                  << size_ << '\n';
    }

    // Move constructor: lấy vùng nhớ của object cũ.
    Buffer(Buffer&& other) noexcept
        : size_(other.size_),
          data_(other.data_) {
        other.size_ = 0;
        other.data_ = nullptr;

        std::cout << "Move Buffer size = "
                  << size_ << '\n';
    }

    ~Buffer() {
        std::cout << "Destroy Buffer size = "
                  << size_ << '\n';

        delete[] data_;
    }

    Buffer& operator=(const Buffer&) = delete;
    Buffer& operator=(Buffer&&) = delete;
};

int main() {

    Buffer first(5);
    Buffer copied = first;

    std::vector<Buffer> buffers;

    // Vector chỉ có chỗ cho một phần tử.
    buffers.reserve(1);

    buffers.emplace_back(10);

    // Vector hết chỗ nên phải cấp phát lại
    // và move Buffer cũ sang vùng nhớ mới.
    buffers.emplace_back(20);
} 