#include <iostream>
#include <atomic>
#include <thread>
#include <vector>

/*
minh hoạ spinlock tự viết để bảo vệ biến đếm dùng chung khi nhiều luồng cùng tăng nó.
Mục tiêu là tránh race condition — nếu 4 luồng cùng thực hiện g_sharedCounter++ mà không khóa,
kết quả có thể nhỏ hơn 40000.
*/
class CustomSpinlock
{
private:
    /* data */
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT; //  ATOMIC_FLAG_INIT đưa cờ về trạng thái clear (false - chưa khóa)

public:
    void lock()
    {
        /*
        test_and_set() đọc giá trị cũ và lập tức gán bằng true(Atomic)
        //Nếu giá trị cũ là true(đã có luồng khác khóa) vòng lặp lại liên tục(Spinning)
        */
        while (flag_.test_and_set(std::memory_order_acquire))
        {
            /*
Nếu trước đó là false: không ai giữ khóa. Hàm đặt cờ thành true, trả về false; điều kiện while sai, luồng thoát vòng lặp và đã lấy được khóa.
Nếu trước đó là true: khóa đã bị giữ. Hàm trả về true; luồng tiếp tục lặp, gọi đây là “spinning” hoặc “busy waiting”.

std::memory_order_acquire đảm bảo sau khi lấy khóa thành công, luồng sẽ nhìn thấy dữ liệu mà luồng trước đã cập nhật trước khi nhả khóa.
            */
        }
    }

    void unlock()
    {
        // trả cờ về false, cho phép luồng đang spin thoát vòng lặp
// memory_order_release đảm bảo mọi thay đổi trong vùng khóa (ví dụ tăng biến đếm) được hoàn tất/hiển thị trước khi khóa được nhả.
        flag_.clear(std::memory_order_release);
    }
    CustomSpinlock() = default;
    ~CustomSpinlock() = default;
};

CustomSpinlock g_spinLock;
int g_sharedCounter = 0;
void spinWorker()
{
    for (int i = 0; i < 10000; ++i)
    {
        g_spinLock.lock();
        g_sharedCounter++;
        g_spinLock.unlock();
    }
}

int main()
{
    std::vector<std::thread> threads;

    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back(spinWorker);
    }
    for (auto &th : threads)
        th.join();
    std::cout << "Spinlock Counter Result: " << g_sharedCounter << " (Expected: 40000)\n";
    return 0;
}