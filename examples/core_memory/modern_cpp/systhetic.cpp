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
    std::unique_ptr<int> custom_Id_;

public:
    explicit UserProfile(
        std::string name) : name_(std::move(name)), custom_Id_(std::make_unique<int>(0)) {}

    void addScore(int score)
    {
        scores_.push_back(score);
    }

    void display() const
    {
        std::cout << "Name: " << name_ << std::endl;

        for (auto &x : scores_)
        {
            std::cout << "Score:" << x << " " << std::endl;
        }
        std::cout << '\n';
    }
};

struct Point
{
    /* data */
    int x;
    int y;
};

int main()
{

    int number = 1;
    // number la lvalue
    // 1 la rvalue

    int &lvalueReference = number;
    int &&rvalueReference = 20;

    std::cout << lvalueReference << '\n';
    std::cout << rvalueReference << '\n';

    // Move semantics

    std::string source = "Tuan vv inetern";
    std::string des = std::move(source);
    std::cout << des << "\n";

    // Rule of 0
    UserProfile first("Tuan");
    first.addScore(100);
    first.addScore(99);

    UserProfile second = std::move(first);
    second.display();

    // Struct binding
    Point point{10, 20};
    auto &[x, y] = point;
    x = 100;
    std::cout << point.x << '\n';

    // Struct binding unoderedmap

    std::unordered_map<std::string, int> track{
        {"Xin chao", 1},
        {"Hello", 2}};

    for (const auto &[key, value] : track)
    {
        std::cout << key << " " << value << '\n';
    }

    return 0;
}