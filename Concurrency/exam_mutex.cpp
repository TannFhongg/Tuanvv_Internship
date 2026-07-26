
/*
Mutex : Bao ve Critical Section = chỉ cho phép 1 luồng được giữ khóa tại một thời điểm.
*/
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

// Lock Guard theo tieu chuan RAII  (Non-copyble)
template <typename MuTexType>

class CustomLockGuard
{
private:
    MuTexType &mtx_; 
    /*
    Không tạo bản sao mutex
vì mutex không nên copy và guard cần khóa đúng mutex được truyền vào. 
    */

public:
    explicit CustomLockGuard( // nhan mutex va goi lock ngay lap tuc
        MuTexType &mtx) : mtx_(mtx)
    {
        mtx_.lock();
    }

    ~CustomLockGuard()
    { // tu dong goi unlock() khi object bi huy (thu hoi bo nho stack)
        mtx_.unlock();
    }

    // Non-copy
    CustomLockGuard(const CustomLockGuard&) = delete;
    CustomLockGuard& operator=(const CustomLockGuard&) = delete;
};

std::mutex g_counterMutex;
int g_counter = 0;

void incrementWorker()
{
    for (int i = 0; i < 1000; ++i)
    {
        // khoi tao RAII Lock Guard -> Tu khoa g_printMutex
        CustomLockGuard<std::mutex> lock(g_counterMutex);
        g_counter++;

        // khi vong lap ket thuc scope, lock bi huy tu dong goi unlock()
    }
}
int main()
{
    std::vector<std::thread> workers;
    /*
    LockGuard theo RAII để bảo vệ biến đếm dùng chung khi 10 thread cùng tăng nó. 
    */
    for (int i = 0; i < 10; ++i)
    {
        workers.emplace_back(incrementWorker, i);
    }

    for (auto &th : workers)
        th.join();

    std::cout << "Final Counter expect 10000 :" << g_counter << "\n";
    return 0;
}