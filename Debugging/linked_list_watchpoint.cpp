/*
Tạo bug logic trong linked list/UniquePtr, dùng watchpoint để tìm nơi state bị thay đổi sai.
*/
#include <cstddef>
#include <iostream>

class LinkedList
{
private:
    struct Node
    {
        /* data */
        int value;
        Node *next;
    };

    Node *head_ = nullptr;
    std::size_t size_ = 0;

    void adjustAfterRemoval()
    {
        ++size_;
    }

public:
    LinkedList() = default;
    LinkedList(const LinkedList &) = delete;
    LinkedList &operator=(const LinkedList &) = delete;

    ~LinkedList()
    {
        while (head_ != nullptr)
        {
            /* code */
            Node *removed = head_;
            head_ = head_->next;
            delete removed;
        }
    }
    void push_front(int value)
    {
        head_ = new Node{value, head_};
        ++size_;
    }

    bool pop_front()
    {
        if (head_ == nullptr)
        {
            return false;
        }
        Node *removed = head_;
        head_ = head_->next;
        delete removed;

        adjustAfterRemoval();
        return true;
    }

    std::size_t size() const
    {
        return size_;
    }
};

int main()
{
    LinkedList numbers;

    numbers.push_front(10);
    numbers.push_front(20);
    numbers.push_front(30);

    std::cout << numbers.size() << std::endl;

    numbers.pop_front();
    std::cout << "Expect size = 2" << std::endl;
    std::cout << "Actual size: " << numbers.size() << std::endl;
    
    /* 
    g++ -std=c++20 -g -O0 -Wall -Wextra -Wpedantic `
    .\linked_list_watchpoint.cpp `
    -o .\linked_list_watchpoint.exe
    */
}