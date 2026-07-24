#include <iostream> 

// Primary Class Template
template <typename T>
struct TypePrinter {
    static void print() { std::cout << "General Type\n"; }
};

// 1. Partial Specialization cho tất cả kiểu con trỏ T*
template <typename T>
struct TypePrinter<T*> {
    static void print() { std::cout << "Pointer Type\n"; }
};

// 2. Full Specialization cho riêng kiểu bool
template <>
struct TypePrinter<bool> {
    static void print() { std::cout << "Bool Type\n"; }
};

// --- Function Template ---
template <typename T>
void process(T val) { std::cout << "General Function\n"; }

// Full Specialization cho Function Template (Hợp lệ)
template <>
void process<int>(int val) { std::cout << "Specialized int Function\n"; } 



template <typename T, std::size_t N>  

class Buffer { 
    private: 
    T data_[N]{ }
    public : 
    std::size_t capacity() const noexcept { 
        return N; 
    }
    T& operator [](std::size_t index) { 
        return data_[index]; 
    }
};

int main() { 
    Buffer<int, 10> number_1; 
    Buffer<double , 20> number_2; 
}



