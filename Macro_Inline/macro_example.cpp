/*
Dùng g++ -E hoặc clang++ -E để xem output macro expansion.
*/
#include <iostream> 

#define SQUARE(x) ((x) * (x))
/* 
# 9 "macro_example.cpp"
int main() {
    int value = ((2 + 3) * (2 + 3)) ;
# 23 "macro_example.cpp"
    std::cout << 3.14159 << '\n';
    std::cout << value << '\n';
# 33 "macro_example.cpp"
}

*/
#define PI 3.14159 

int main() { 
    int value = SQUARE( 2 + 3) ; 
    /* 
    # 9 ".\\macro_example.cpp"
int main() {
    int value = 2 + 3 * 2 + 3 ;
    std::cout << 3.14159 << '\n';
    std::cout << value << '\n';
# 21 ".\\macro_example.cpp"
}
    */

    std::cout << PI << '\n'; 
    std::cout << value << '\n';
    
    /* 
    for compiler gcc : g++ -E macro_example.cpp -o macro_example.i 
    clang: clang++ -E macro_example.cpp -o macro_example.i
     */

     /* 
     Khi nào BẮT BUỘC vẫn phải dùng Macro?
    1/  Conditional Compilation (Biên dịch theo điều kiện) 
#if defined(RASPBERRY_PI)
    #include <wiringPi.h>
#elif defined(STM32)
    #include "stm32f4xx_hal.h"
#endif

    2/ File & Line Information (Log & Assert)
    #define LOG_ERROR(msg) std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] " << msg << '\n'

    3/ Include Guards 
    #ifndef MY_HEADER_H
#define MY_HEADER_H
// ...
#endif
    4/ Stringification (#) & Token Pasting (##): Biến tên biến/token thành string hoặc nối token ở compile-time.
     */
}
 