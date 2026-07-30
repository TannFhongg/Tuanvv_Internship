#include <generator> // C++23, or use custom promise
#include <iostream>
#include <ranges>

std::generator<int> fibonacci()
{
    int a = 0, b = 1;
    while (true)
    {
        co_yield a; // suspend and produce value
        auto tmp = a;
        a = b;
        b = tmp + b;
    }
}
int main() {
    // Kết hợp Coroutine lười (lazy evaluation) với Ranges API
    for (int n : fibonacci() | std::views::take(10)) {
        std::cout << n << " ";
    }
    std::cout << '\n';
    return 0;
}