/*
Thực hành:

Viết singly linked list:
push_front
push_back
find
erase
destructor
Vẽ trạng thái con trỏ trước và sau khi xóa node.
*/
#include <iostream>
#include <cstddef>

template <typename T>
class SinglyLinkedList
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
    SinglyLinkedList() = default;

    ~SinglyLinkedList()
    {
        while (head)
        {
            Node *temp = head;
            head = head->next;
            delete temp;
        }
    }

    void push_front(const T &value)
    {
        Node *new_node = new Node(value);
        new_node->next = head;
        head = new_node;
        if (!tail)
        {
            tail = new_node;
        }
        ++size_;
    }

    void push_back(const T &value)
    {
        Node *new_node = new Node(value);
        if (tail)
        {
            tail->next = new_node;
            tail = new_node;
        }
        else
        {
            head = tail = new_node;
        }
        ++size_;
    }

    Node* find(const T &value)
    {
        Node *current = head;
        while (current)
        {
            if (current->data == value)
            {
                return current;
            }
            current = current->next;
        }
        return nullptr;
    }

    void erase(const T &value)
    {
        Node *current = head;
        Node *prev = nullptr;

        while (current)
        {
            if (current->data == value)
            {
                if (prev)
                {
                    prev->next = current->next;
                }
                else
                {
                    head = current->next;
                }

                if (current == tail)
                {
                    tail = prev;
                }

                delete current;
                --size_;
                return;
            }
            prev = current;
            current = current->next;
        }
    }

    std::size_t size() const
    {
        return size_;
    }
};

int main() { 
    SinglyLinkedList<int> list;
    list.push_back(1);
    list.push_back(2);
    list.push_back(3);

    std::cout<< "size:" << list.size() << std::endl;
    std::cout<< "find 2:" << (list.find(2) != nullptr) << std::endl;
    list.erase(2);
    std::cout<< "size:" << list.size() << std::endl;
    std::cout<< "find 2:" << (list.find(2) != nullptr) << std::endl;


}