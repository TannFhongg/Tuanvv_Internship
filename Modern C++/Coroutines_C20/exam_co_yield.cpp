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

for (int n : fibonacci() | std::views::take(10))
{
    std::cout << n << " "; // 0 1 1 2 3 5 8 13 21 34
}