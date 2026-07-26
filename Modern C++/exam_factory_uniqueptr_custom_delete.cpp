/*
Viết factory trả về std::unique_ptr<Base> và dùng polymorphism mà không manual delete.
*/

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

class Animal
{
private:
    /* data */

public:
    Animal(/* args */);
    virtual ~Animal()
    {
        std::cout << "Destroy Animal\n";
    }

    virtual void speak() const = 0; 
};

class Dog : public Animal
{

public:
    ~Dog() override
    {
        std::cout << "Destroy Dog \n";
    }

    void speak() const override { 
        std::cout <<"Gou gou\n"; 
    }
};

class  Cat : public Animal
{
private:
    /* data */
public:
  
     ~ Cat() override { 
        std::cout << "Destroy Cat\n"; 
     }

     void speak() const override { 
        std::cout <<"meow meow\n"; 
     }
};

std::unique_ptr<Animal> createAnimal(
    std::string_view type
) { 
    if(type == "dog") { 
        return std::make_unique<Dog>(); 
    }
    if(type == "cat") {
        return std::make_unique<Cat>(); 
    }

    throw std::invalid_argument("unknow type"); 
}

int main() { 
    std::vector<std::unique_ptr<Animal>> animals; 

    animals.push_back(createAnimal("dog"));
    animals.push_back(createAnimal("cat")); 

    for(const auto& animal : animals) { 
        animal -> speak(); 
    }

}



