#include <iostream>
#include <vector>
#include <cstddef>

const std::size_t DEFAULT_SIZE = 10;

struct Node
{
    int key;
    std::string value;
    Node *next;

    Node(int k, std::string value) : key(k), value(value), next(nullptr) {}
};

class HashTable
{
private:
    Node *table[DEFAULT_SIZE];

    int hashFunction(int key)
    {
        return key % DEFAULT_SIZE;
    }

public:
    HashTable()
    {
        for (int i = 0; i < DEFAULT_SIZE; i++)
        {
            table[i] = nullptr;
        }
    }
    void insert(int key, std::string value)
    {
        int index = hashFunction(key);
        Node *current = table[index];

        // cap nhat neu key da ton tai
        while (current != nullptr)
        {
            if (current->key == key)
            {
                current->value = value;
                return;
            }
            current = current->next;
        }

        // them node moi vao dau danh sach linked list
        Node *newNode = new Node(key, value);
        newNode->next = table[index];
        table[index] = newNode;
    }

    std::string get(int key)
    {
        int index = hashFunction(key);
        Node *current = table[index];

        while (current != nullptr)
        {
            if (current->key == key)
            {
                return current->value;
            }
            current = current->next;
        }

        return " Key not found";
    }

    void display()
    {
        for (int i = 0; i < DEFAULT_SIZE; i++)
        {
            std::cout << "Index " << i << ": ";
            Node *current = table[i];

            while (current != nullptr)
            {
                std::cout << "(" << current->key << ", " << current->value << ") -> ";
                current = current->next;
            }
            std::cout << "nullptr" << std::endl;
        }
    }

    void remove(int key)
    {
        int index = hashFunction(key);
        Node *current = table[index];
        Node *prev = nullptr;

        while (current != nullptr)
        {
            if (current->key == key)
            {
                if (prev == nullptr)
                    table[index] = current->next; // Xóa node đầu
                else
                    prev->next = current->next; // Xóa node giữa/cuối

                delete current;
                return;
            }

            prev = current;
            current = current->next;
        }
    }
};

int main()
{
    HashTable ht;

    ht.insert(1, "Apple");
    ht.insert(11, "Banana"); // collision key 1
    ht.insert(21, "Cherry"); // collision key 1
    ht.insert(2, "Orange");  // collision key 1

    ht.display();

    ht.get(2);

    ht.remove(2);
    ht.display();
}