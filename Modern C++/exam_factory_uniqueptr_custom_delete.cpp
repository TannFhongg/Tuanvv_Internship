/*
 * Factory returns std::unique_ptr<Base, CustomDeleter> so polymorphic
 * objects are released through one ownership policy without manual delete.
 */

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

struct AnimalDeleter;

class Animal
{
public:
    Animal() = default;
    Animal(const Animal &) = delete;
    Animal &operator=(const Animal &) = delete;
    Animal(Animal &&) = delete;
    Animal &operator=(Animal &&) = delete;

    virtual void speak() const = 0;

protected:
    virtual ~Animal() = default;

    friend struct AnimalDeleter;
};

class Dog final : public Animal
{
public:
    void speak() const override
    {
        std::cout << "Gou gou\n";
    }

    ~Dog() override
    {
        std::cout << "Destroy Dog\n";
    }
};

class Cat final : public Animal
{
public:
    void speak() const override
    {
        std::cout << "Meow meow\n";
    }

    ~Cat() override
    {
        std::cout << "Destroy Cat\n";
    }
};

struct AnimalDeleter
{
    void operator()(Animal *animal) const noexcept
    {
        if (animal == nullptr)
        {
            return;
        }

        std::cout << "Custom deleter releases animal\n";
        delete animal;
    }
};

using AnimalPtr = std::unique_ptr<Animal, AnimalDeleter>;

AnimalPtr createAnimal(std::string_view type)
{
    if (type == "dog")
    {
        return AnimalPtr(new Dog());
    }

    if (type == "cat")
    {
        return AnimalPtr(new Cat());
    }

    throw std::invalid_argument("Unknown animal type");
}

int main()
{
    std::vector<AnimalPtr> animals;

    animals.push_back(createAnimal("dog"));
    animals.push_back(createAnimal("cat"));

    for (const AnimalPtr &animal : animals)
    {
        animal->speak();
    }
}
