#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class UserProfile
{
private:
    std::string name_;
    std::vector<int> scores_;
    std::unique_ptr<int> customId_;

public:
    explicit UserProfile(std::string name)
        : name_(std::move(name)),
          customId_(std::make_unique<int>(0))
    {
    }

    void addScore(int score)
    {
        scores_.push_back(score);
    }

    void display() const
    {
        std::cout << "Name: " << name_ << '\n';

        std::cout << "Scores: ";

        for (int score : scores_)
        {
            std::cout << score << ' ';
        }

        std::cout << '\n';
    }
};

struct Point
{
    int x;
    int y;
};

int main()
{
    // Lvalue và Rvalue Reference
    int number = 10;

    int& lvalueReference = number;
    int&& rvalueReference = 20;

    std::cout << lvalueReference << '\n';
    std::cout << rvalueReference << '\n';

    // Move Semantics
    std::string source = "Hello Modern C++";
    std::string destination = std::move(source);

    std::cout << destination << '\n';

    // Rule of Zero
    UserProfile first("Tuan");
    first.addScore(90);
    first.addScore(85);

    UserProfile second = std::move(first);
    second.display();

    // Structured Bindings với Struct
    Point point{10, 20};

    auto& [x, y] = point;
    x = 100;

    std::cout << "Point: " << point.x << ", " << point.y << '\n';

    // Structured Bindings với unordered_map
    std::unordered_map<std::string, int> tracker{
        {"FPS", 60},
        {"Latency", 15}
    };

    for (const auto& [key, value] : tracker)
    {
        std::cout << key << ": " << value << '\n';
    }

    return 0;
}