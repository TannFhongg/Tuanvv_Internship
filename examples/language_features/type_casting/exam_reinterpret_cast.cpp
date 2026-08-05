#include <cstdint>
#include <iostream>


/* 
reinterpret_cast tells the compiler to treat the same bits as a different type.
It performs almost no safety checking and does not create a valid object of the target type. 
*/
int main()
{

    int x = 42;

    // Pointer -> integer: useful for logging/debugging addresses
    std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(&x);
    std::cout<< addr << '\n';

    int y = 10; 
    auto addr_2 = reinterpret_cast<std::uintptr_t>(&y); 
    std::cout << addr_2 << '\n'; 

    // Integer -> pointer: only valid if the address really points to a valid int
    int *ptr = reinterpret_cast<int *>(addr);
    std::cout << *ptr << '\n'; // ok here because addr came from &x

    int* p = reinterpret_cast<int *>(addr_2); 
    std::cout << *p ; 
}