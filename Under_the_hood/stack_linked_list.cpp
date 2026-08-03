#include <iostream>
#include <stdexcept>

template <typename T>
class StackNode
{
private:
    struct Node
    {
        T data;
        Node *next;
        explicit Node(const T &value) : data(value), next(nullptr) {}
    };
    Node *head = nullptr;
    Node *tail = nullptr;
    std::size_t size{0};

public:
    StackNode() = default;
    ~StackNode()
    {
        while (head)
        {
            Node *temp = head;
            head = head->next;
            delete temp;
        }
    }

    void push(const T &value)
    {
        Node *newNode = new Node(value);
        newNode->next = head;
        head = newNode;
        ++size;
    }

    T &top() const 
    {
        if (head == nullptr)
        {
            throw std::out_of_range("Stack is empty");
        }
        return head->data;
    }
    void pop()
    {
        if (head == nullptr)
        {
            throw std::out_of_range("Stack is empty");
        }
        Node *oldTop = head;
        head = head->next;
        delete oldTop;
        size--;
    }
};