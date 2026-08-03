#include <iostream>
#include <vector>
template <typename T>
class Heap
{

public:
    void shiftUp(std::vector<T> &data, int index)
    {
        while (index > 0)
        {
            int parent_ = (index - 1) / 2;
            if (data[parent_] < data[index])
            {
                std::swap(data[parent_], data[index]);
                index = parent_;
            }
            else
            {
                break;
            }
        }
    }
    // [80, 70, 50, 30, 60, 40] 
    void shiftDown(std::vector<T> &data, int index)
    {
        int size = data.size();
        while (index < size)
        {
            int left_child_ = 2 * index + 1;
            int right_child_ = 2 * index + 2;
            int largest = index;

            if (left_child_ < size && data[left_child_] > data[largest])
            {
                largest = left_child_;
            }
            if (right_child_ < size && data[right_child_] > data[largest])
            {
                largest = right_child_;
            }
            if (largest != index)
            {
                std::swap(data[index], data[largest]);
                index = largest;
            }
            else
            {
                break;
            }
        }
    }
};