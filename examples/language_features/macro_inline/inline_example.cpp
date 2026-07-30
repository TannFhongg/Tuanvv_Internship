#include <iostream>

/* 
Cho phép định nghĩa cùng một hàm trong nhiều file .cpp thông qua header
mà không bị lỗi trùng định nghĩa khi link.

Gợi ý compiler có thể thay lời gọi hàm bằng chính nội dung của hàm, để giảm chi phí gọi hàm.

Trong C++ hiện đại, inline thường quan trọng nhất khi viết hàm ngắn trong file header:
*/
inline int square(int x) {
    return x * x;
}

int main() {
    int value = square(2 + 3);

    std::cout << value << '\n';
}

/*
g++ -O2 -S .\inline_example.cpp -o .\inline_example.s

	movl	$25, %edx 
*/