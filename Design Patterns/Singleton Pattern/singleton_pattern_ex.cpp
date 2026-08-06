/*
Singleton Pattern
How to the Singleton Pattern ensure the class has only one instance
-> by providing a static method that returns a single instance

Đảm bảo một class chỉ có duy nhất một instance (thể hiện) tồn tại trong suốt vòng đời của ứng dụng.

Cung cấp một điểm truy cập toàn cục (global access point) đến instance duy nhất đó.

*/
#include <iostream>
#include <string>
class Singleton
{
public:
    static Singleton &getInstance()
    {
        static Singleton instance;
        return instance;
    }

    Singleton(const Singleton &) = delete;
    Singleton &operator=(const Singleton &) = delete;

    void showMsg(const std::string msg)
    {
        std::cout << "Msg:" << msg << std::endl;
    }

private:
    Singleton() = default;
};

int main()
{
    // Singleton &instance = Singleton::getInstance();
    // instance.showMsg("Singleton instance accessed");

    Singleton::getInstance().showMsg("Hello");
    return 0;
}