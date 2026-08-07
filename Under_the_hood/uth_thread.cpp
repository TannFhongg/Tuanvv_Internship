#include <iostream>
#include <thread>
#include <string>
#include <chrono>

class thread
{
public:
    class id
    {
    public:
        id() noexcept;
        bool operator==(const id &other) const noexcept;
        bool operator!=(const id &other) const noexcept;
        bool operator<(const id &other) const noexcept;
        bool operator<=(const id &other) const noexcept;
        bool operator>(const id &other) const noexcept;
        bool operator>=(const id &other) const noexcept;
    };

    template <class F>
    thread();

    template <class F, class... Args>
    thread(F &&f, Args &&...args);

    ~thread();

    thread(const thread &) = delete;
    thread(thread &&other) noexcept;
    thread &operator=(const thread &) = delete;
    thread &operator=(thread &&other) noexcept;

    void swap(thread &&);
    bool joinable() const noexcept;
    void join();
    void detach();
    id get_id() const noexcept;
    // native_handle_type native_handle();

    static unsigned int hardware_concurrency() noexcept;
};

struct LoopMessage
{
    const std::string message;
    const int delay;

    LoopMessage(std::string message, int delay) : message(message), delay(delay) {}

    // Function-call operator called by the thread class.
    void operator()()
    {
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::seconds(delay));
            std::cout << " [INFO] thread id = " << std::this_thread::get_id()
                      << " ; " << message << std::endl;
        }
    }
};

int main()
{
    thread thread_messageA{LoopMessage{"Hello world", 10}};
    thread thread_messageB{LoopMessage{"Hello world", 10}};

    if (thread_messageA.joinable())
    {
        thread_messageA.join();
    }
    if (thread_messageB.joinable())
    {
        thread_messageB.join();
    }
    
    return 0;
}