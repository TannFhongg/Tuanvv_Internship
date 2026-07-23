/*

UniquePtr<T>: move constructor, move assignment, get, release, reset, operator*, operator->, destructor.

*/

#include <cstddef>
#include <iostream>
#include <string>
#include <utility>
#include <iostream>

template <typename T> 


class UniquePtr
{
private:
    T* data_;
    std::size_t size_;

public:
    UniquePtr() noexcept : data_(nullptr), size_(0) { }

    explicit UniquePtr(std::size_t size) :data_(size > 0 ? new T[size] :nullptr) , size_(size) {}
    ~UniquePtr()
    {
        delete[] data_;
    }

    UniquePtr(const UniquePtr &other) = delete; // cam copy constructor
    UniquePtr &operator=(const UniquePtr &other) = delete; // cam copy assignment 

    UniquePtr(UniquePtr &&other) noexcept : data_(other.data_), size_(other.size_) // move constructor
    {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    UniquePtr& operator=(UniquePtr &&other) noexcept { //move asssigment 
         
        if(this == &other)  return *this;
        
        delete[] data_; 
        data_ = nullptr; 

        data_ = other.data_; 
        size_ = other.size_ ; 

        other.data_ = nullptr; 
        other.size_ = 0; 

        return *this; 

    }

    T* get() const noexcept { // lay con tro tho , van giu quyen so huu  
        return data_; 
    }

    T* release() noexcept { // Tra con tro tho va tu bo quyen so huu 
        T* result = data_ ; 
        data_ = nullptr; 
        size_ = 0; 
        return result; 
    }
    
    void reset(T* newData = nullptr, std::size_t newSize = 0 ) noexcept {
        if(data_ = newData) return;   // xoa du lieu cu va quan li vung nho moi 

        delete[] data_; 
        
        data_ = newData; 
        size_ = newSize; 

    }

    T& operator*() { 
        return *data_; 
    }
    const T& operator*() const { 
        return *data_; 
    }
    T* operator -> () {
        return data_; 
    }
    const T* operator->() const { 
        return data_; 
    }

    std::size_t size() const noexcept {
        return size_; 
    }
};

struct  Student 
{
    /* data */

    std::string name; 
    int age{}; 
};




int main()
{
    UniquePtr<Student> ptr(1); 

    ptr -> name = "Tuanvv"; 
    ptr -> age = 23;

    std::cout << (*ptr).name << " " << (*ptr).age << std::endl; 

    UniquePtr<Student> ptr2 = std::move(ptr); // move constructor 
    std::cout << ptr2.get() << std::endl; 

    //releases 
    Student* rawPtr = ptr2.release(); 

    UniquePtr<Student> rs; 
    rs.reset(rawPtr,1); 

    UniquePtr<Student> ptr3; // move assigment constructor 
    ptr3 = std::move(rs); 
    std::cout << (*ptr3).name << " " << (*ptr3).age << std::endl; 

}