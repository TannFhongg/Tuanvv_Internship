#include <iostream>
#include <unordered_map>
#include <string>

int main() { 

    std::unordered_map<std::string, int > track { 
        {"so muoi", 10}, 
        {"so mot" , 1}
    }; 


    for(const auto& [x,y] : track) { 
        std::cout << x << " : " << y << "\n"; 
    }
}