/*
Bài tập: tự viết bounded blocking queue (producer–consumer)
bằng std::queue, std::mutex, hai condition_variable;
 có push, pop, close và nhiều producer/consumer.
 Producer → push() → Queue tối đa 5 phần tử → pop() → Consumer

 std::condition_variable
 Luồng ngủ/block, nhường CPU và được đánh thức bằng notify_one() hoặc notify_all().

 Spurious Wakeup (đánh thức giả) & Predicate
 // Cú pháp chuẩn luôn phải áp dụng:
cv.wait(lock, [] { return condition_is_true; });



// while (!condition_is_true) {
//     cv.wait(lock);
// }
*/

#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <optional>
#include <chrono>

template <typename T>
class BoundedBlockingQueue
{

private:
    std::queue<T> queue_;
    const size_t capacity_;
    bool closed_ = false;

    std::mutex mtx_;
    std::condition_variable cv_not_full_;  // Producer chờ ở đây nếu queue đầy
    std::condition_variable cv_not_empty_; // Consumer chờ ở đây nếu queue rỗng

public:
    explicit BoundedBlockingQueue(size_t capacity) : capacity_(capacity) {}

    // push du lieu vao queue (producer)

    bool push(T item)
    {
        std::unique_lock<std::mutex> lock(mtx_);

        // cho den khi queue co cho trong hoac queue bi dong
        cv_not_full_.wait(lock, [this]
                           { return queue_.size() < capacity_ || closed_; });

        if (closed_)
            return false; // khong nhan them du lieu neu queue da dong

        queue_.push(std::move(item));

        cv_not_empty_.notify_one(); // danh thud comsumer vi queue dang rong
        return true;
    }

    std::optional<T> pop()
    {
        std::unique_lock<std::mutex> lock(mtx_);

        // cho den khi queue co du lieu hoac queue bi dong
        cv_not_empty_.wait(lock, [this]()
                           { return (!queue_.empty()) || closed_; });

        if (queue_.empty() && closed_)
        {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop();

        // danh thuc 1 producer dang ngu day vi queue day
        cv_not_full_.notify_one();
        return item;
    }
    // dong queue, danh thuc tat ca cac luong dang cho
    void close()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        closed_ = true;
        cv_not_empty_.notify_all(); // danh thuc cac consumer dang cho
        cv_not_full_.notify_all();  // danh thuc cac producer dang cho
    }
};

int main()
{
    BoundedBlockingQueue<int> bbq(5); // queue voi 5 phan tu

    // 2 luong producer, moi luong sinh 10 con so
    auto producer = [&](int id)
    {
        for (int i = 1; i <= 10; ++i)
        {
            int val = id * 100 + i;
            if (bbq.push(val))
            {
                std::cout << "[Producer " << id << "] Pushed: " << val << "\n";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    };

    auto consumer = [&](int id)
    {
        while (true)
        {
            /* code */
            auto item = bbq.pop();
            if (!item.has_value())
                break; // queue dong va het du lieu
            std::cout << "   -> [Consumer " << id << "] Popped: " << item.value() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    };

    std::thread p1(producer, 1);
    std::thread p2(producer, 2);
    std::thread c1(consumer, 1);
    std::thread c2(consumer, 2);

    p1.join();
    p2.join();
    bbq.close(); // Đóng queue khi các Producer đã sinh hết hàng
    c1.join();
    c2.join();

    return 0;
}