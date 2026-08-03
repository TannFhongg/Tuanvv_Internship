#include <iostream>

struct Node
{
    int value;
    Node *left;
    Node *right;

    Node(int value) : value(value), left(nullptr), right(nullptr) {}
};

class BinaryTree
{
private:
    Node *root;

    Node *insert(Node *node, int value)
    {
        if (node == nullptr)
            return new Node(value);

        if (value < node->value)
        {
            node->left = insert(node->left, value);
        }
        else if (value > node->value)
        {
            node->right = insert(node->right, value);
        }

        return node;
    }

    void inorder(Node *node)
    {
        if (node == nullptr)
            return;

        inorder(node->left);
        std::cout << node->value << " ";
        inorder(node->right);
    }

    bool search(Node *node, int value)
    {
        if (node == nullptr)
            return false;

        if (node->value == value)
            return true;

        if (value < node->value)
            return search(node->left, value);

        return search(node->right, value);
    }

public:
    BinaryTree() : root(nullptr) {}

    void insert(int value)
    {
        root = insert(root, value);
    }

    void inorder()
    {
        inorder(root);
    }

    bool search(int value)
    {
        return search(root, value);
    }
};

int main()
{
    BinaryTree tree;
    tree.insert(5);
    tree.insert(3);
    tree.insert(7);
    tree.insert(2);
    tree.insert(4);

    std::cout << "Duyet inorder: ";
    tree.inorder();
    std::cout << std::endl;

    if (tree.search(6))
        std::cout << "Tim thay 6" << std::endl;
    else
        std::cout << "Khong tim thay 6" << std::endl;
    return 0;
}
