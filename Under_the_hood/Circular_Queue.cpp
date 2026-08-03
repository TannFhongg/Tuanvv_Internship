#include <cstddef>
#include <stdexcept>
#include <vector>
template <typename T>
class CircularQueue
{

private:
    std::vector<T> data;
    std::size_t head_{0};
    std::size_t tail_{0};
    std::size_t size_{0};

public:
    explicit CircularQueue(std::size_t capacity) : data(capacity)
    {
        if (capacity == 0)
        {
            throw std::invalid_argument("Capacity must be greater than 0");
        }
    }

    void push(const T &value)
    {
        if (full())
        {
            throw std::overflow_error("Queue is full");
        }
        data[tail_] = value;
        tail_ = (tail_ + 1) % data.size();
        ++size_;
    }
    void pop()
    {
        if (empty())
        {
            throw std::underflow_error("Queue is empty");
        }
        head_ = (head_ + 1) % data.size();
        --size_;
    }
    T &front()
    {
        if (empty())
        {
            throw std::underflow_error("Queue is empty");
        }
        return data[head_];
    }
    T &back()
    {
        if (empty())
        {
            throw std::underflow_error("Queue is empty");
        }
        std::size_t lastIndex = (tail_ + data.size() - 1) % data.size();
        return data[lastIndex];
    }
    bool full() const
    {
        return size_ == data.size();
    }

    bool empty() const
    {
        return size_ == 0;
    }
    std::size_t size() const noexcept
    {
        return size_;
    }
    std::size_t capacity() const noexcept
    {
        return data.size();
    }
};