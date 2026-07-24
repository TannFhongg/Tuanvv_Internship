
#include <iostream> 
// Copy Constructor 

class Widget {
private: 
int* data_; 
size_t size_; 

public: 
~Widget() {
        delete[] data_;
    }

// Copy Constructor  
Widget(const Widget& other)  : size_(other.size_), data_(nullptr) { 

    if(other.data_ && size_ > 0) { 
        data_ = new int[size_] ;  

    for (size_t i = 0 ; i < size_; ++i ) { 
        data_[i] = other.data_[i]; 
    }

    }
    else { 
        data_ = nullptr;
    }
}


//  Copy Assigment 

Widget& operator=(const Widget& other) { 

    if(this == &other) { return *this; }
    
    delete[] data_; 

    size_ = other.size_; 

    if(other.data_ && size_ > 0) { 
        data_ = new int[size_]; 
        for(size_t i = 0 ; i < size_ ; ++i ) { 
            data_[i] = other.data_[i]; 
        }

    }
    else { 
        data_ = nullptr; 
    }

    return *this; 
}

// move constructor 

 Widget (Widget&& other) noexcept :data_(other.data_) , size_(other.size_){
    
other.data_ = nullptr; 
other.size_ = 0; 
}

// move assigment 

Widget& operator=(Widget&& other) noexcept {
    
    if(this == &other) return *this; 

    delete[] data_ ; 

    data_ = other.data_; 
    size_ = other.size_ ; 

    other.data_ = nullptr; 
    other.size_ = 0; 

    return *this; 
} 
};


