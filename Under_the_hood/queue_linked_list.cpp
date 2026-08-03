#include <iostream>
#include <cstddef>
template <typename T>
class QueueNode
{
private:
    struct Node
    {
        T data;
        Node *next;
        explicit Node(const T &value) : data(value), next(nullptr) {}
    };
    Node *head{nullptr};
    Node *tail{nullptr};
    std::size_t size_{0};

public:
    QueueNode() = default;
    ~QueueNode()
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

        if (tail)
        {
            tail->next = newNode;
            tail = newNode;
        }
        else
        {
            head = tail = newNode;
        }
        ++size_;
    }

    void pop()
    {
        if (head == nullptr)
        {
            throw std::out_of_range("Queue is empty");
        }

        if (head)
        {
            Node *temp = head;
            head = head->next;
            delete temp;
        }
    }
    std::size_t size() const
    {
        return size_;
    }
};