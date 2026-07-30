#include <cstddef>
#include <iostream>
#include <vector>

int readValue(const std::vector<int>& values, std::size_t index) {
    return values.at(index);
}

int calculateTotal(const std::vector<int>& values) {
    return readValue(values, 5) + 10;
}

int main() {
    const std::vector<int> values{10, 20, 30};
    std::cout << "Total: " << calculateTotal(values) << '\n';
}
