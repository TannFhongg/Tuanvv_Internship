# Giai đoạn 4 — Concurrency (Lập trình đồng thời)
Đây là bước ngoặt quan trọng để chuyển mình từ một lập trình viên viết code chạy tuần tự sang kỹ sư hệ thống có khả năng khai thác tối đa sức mạnh phần cứng đa nhân (Multi-core), đặc biệt trong các hệ thống Robotics, Automotive và Real-time Embedded Linux.

# Nền tảng hệ thống & các khái niệm cốt lõi

Trước khi gõ bất kỳ dòng code nào với `std::thread`, bạn phải thấu hiểu cách hệ điều hành và CPU xử lý các luồng thực thi:

## 1. Thread Scheduling & Critical Section

| Khái niệm | Ý nghĩa | Điều cần nhớ |
| --- | --- | --- |
| **Thread Scheduling** | Bộ định thời của hệ điều hành (OS Scheduler) quyết định luồng nào chạy trên nhân CPU nào, thông qua time-slicing và context switching. | Không được mặc định hoặc dự đoán thứ tự chạy trước/sau của các luồng. |
| **Critical Section** (vùng tranh chấp) | Đoạn mã có nhiều luồng truy cập cùng một tài nguyên chia sẻ, trong đó có ít nhất một thao tác ghi. | Cần cơ chế đồng bộ phù hợp, chẳng hạn mutex hoặc atomic. |

## 2. Data Race vs. Race Condition

Hai khái niệm này hay bị nhầm lẫn là một, nhưng bản chất chúng hoàn toàn khác nhau:

| Khái niệm | Định nghĩa | Hậu quả trong C++ |
| --- | --- | --- |
| **Data Race** | Hai luồng truy cập cùng một vùng nhớ đồng thời, có ít nhất một luồng ghi và không có cơ chế đồng bộ hóa. | **Undefined Behavior (UB):** có thể crash, làm hỏng dữ liệu, hoặc tưởng như vẫn chạy đúng tùy lần chạy. |
| **Race Condition** | Lỗi logic do sai lệch về thời gian hoặc thứ tự thực thi giữa các luồng. | Code có thể không có UB nhưng vẫn cho kết quả sai hoặc hành vi không mong muốn. |

## 3. Visibility, Happens-Before & sự thật về `volatile`

| Khái niệm | Ý nghĩa | Điều cần nhớ |
| --- | --- | --- |
| **Visibility** (tính khả kiến) | Các core có cache riêng; một luồng không nhất thiết thấy ngay giá trị mà luồng khác vừa ghi nếu không có đồng bộ hóa. | Đừng dùng biến thường để truyền tín hiệu giữa các luồng. |
| **Happens-Before** | Mutex và atomic tạo quan hệ thứ tự bộ nhớ: ghi trước `unlock()` có thể được quan sát sau `lock()` tương ứng; acquire/release tạo quan hệ tương tự khi dùng đúng cách. | Đồng bộ hóa không chỉ là “chặn luồng”, mà còn đảm bảo khả kiến của dữ liệu. |
| **`volatile`** | Báo cho compiler rằng giá trị có thể thay đổi từ bên ngoài chương trình, thường gặp khi truy cập thanh ghi MMIO trong embedded. | Không ngăn Data Race và không tạo Happens-Before; không dùng `volatile` để đồng bộ luồng C++. |

## 4.1. `std::thread` — Quản lý luồng & thực thi song song

Khi một đối tượng `std::thread` được khởi tạo với một hàm, luồng mới sẽ chạy ngay lập tức. Trước khi đối tượng thread bị tiêu hủy (chạy hết scope), bạn bắt buộc phải gọi `join()` hoặc `detach()`. Nếu quên, chương trình sẽ gọi `std::terminate()` gây crash lập tức.

| Thao tác | Hành vi | Khi nên dùng |
| --- | --- | --- |
| **`join()`** | Luồng gọi bị block đến khi luồng con hoàn thành. | Mặc định nên dùng; giúp bảo đảm lifetime của dữ liệu mà luồng con dùng. |
| **`detach()`** | Tách luồng con chạy độc lập. | Rất hạn chế. Nếu luồng con giữ tham chiếu/pointer đến dữ liệu cục bộ đã hết lifetime, nó có thể truy cập dangling reference. |

### Bài tập 4.1: Tính tổng mảng song song (Parallel Array Sum)

Đoạn code dưới đây chia một vector kích thước lớn thành K đoạn bằng nhau và giao cho K luồng xử lý song song, sau đó so sánh hiệu năng với bản tuần tự:

```cpp
#include <iostream>
#include <vector>
#include <thread>
#include <numeric>
#include <chrono>

// Hàm worker xử lý tính tổng cho một phân đoạn [start, end)
void partialSumWorker(const std::vector<long long>& arr, size_t start, size_t end, long long& result) {
    long long sum = 0;
    for (size_t i = start; i < end; ++i) {
        sum += arr[i];
    }
    result = sum; // Ghi kết quả ra biến tham chiếu (mỗi luồng ghi vào một ô nhớ riêng -> Không Data Race!)
}

int main() {
    const size_t N = 100000000; // 100 triệu phần tử
    std::vector<long long> data(N, 1); // Mảng toàn số 1, tổng mong đợi = N

    // --- 1. TÍNH TUẦN TỰ (Sequential) ---
    auto startSeq = std::chrono::high_resolution_clock::now();
    long long seqSum = std::accumulate(data.begin(), data.end(), 0LL);
    auto endSeq = std::chrono::high_resolution_clock::now();
    std::cout << "[Sequential] Sum: " << seqSum << " | Time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(endSeq - startSeq).count() << " ms\n";

    // --- 2. TÍNH SONG SONG (Parallel với 4 threads) ---
    const unsigned int numThreads = 4;
    std::vector<std::thread> threads;
    std::vector<long long> partialResults(numThreads, 0);

    size_t chunkSize = N / numThreads;
    auto startPar = std::chrono::high_resolution_clock::now();

    for (unsigned int i = 0; i < numThreads; ++i) {
        size_t startIdx = i * chunkSize;
        // Luồng cuối cùng nhận luôn phần dư nếu N không chia hết cho numThreads
        size_t endIdx = (i == numThreads - 1) ? N : startIdx + chunkSize;

        // Truyền tham chiếu vào thread phải bao bọc bằng std::ref()
        threads.emplace_back(partialSumWorker, std::cref(data), startIdx, endIdx, std::ref(partialResults[i]));
    }

    // Bắt buộc Join tất cả các luồng trước khi tổng hợp kết quả
    for (auto& th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }

    long long parSum = std::accumulate(partialResults.begin(), partialResults.end(), 0LL);
    auto endPar = std::chrono::high_resolution_clock::now();
    std::cout << "[Parallel]   Sum: " << parSum << " | Time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(endPar - startPar).count() << " ms\n";

    return 0;
}
```

## 4.2. `std::mutex` và Lock RAII — Bảo vệ dữ liệu

`std::mutex` (Mutual Exclusion) bảo vệ Critical Section bằng cách chỉ cho phép 1 luồng được giữ khóa tại một thời điểm.

**Quy tắc quan trọng:** Tránh gọi `lock()` và `unlock()` thủ công. Nếu code giữa hai lệnh này ném exception hoặc `return` sớm, `unlock()` có thể không được gọi và gây deadlock. Hãy dùng wrapper RAII:

| Wrapper | Hành vi | Phù hợp khi |
| --- | --- | --- |
| **`std::lock_guard`** | Khóa khi khởi tạo, tự mở khóa khi ra khỏi scope. | Cần khóa đơn giản, không mở khóa giữa chừng. |
| **`std::unique_lock`** | Linh hoạt hơn: có thể defer lock, `unlock()` giữa chừng và chuyển quyền sở hữu khóa. | Dùng với `std::condition_variable` hoặc cần điều khiển lock linh hoạt. |
| **`std::scoped_lock`** (C++17) | Quản lý RAII cho một hay nhiều mutex và dùng thuật toán khóa tránh deadlock. | Cần khóa đồng thời nhiều mutex. |

### Deadlock & cách phòng tránh

Deadlock xảy ra khi Thread A giữ Mutex 1 và chờ Mutex 2, trong khi Thread B giữ Mutex 2 và chờ Mutex 1.

| Cách phòng tránh | Cách áp dụng |
| --- | --- |
| **Thứ tự khóa cố định** | Luôn khóa các mutex theo cùng một thứ tự trên toàn hệ thống. |
| **`std::scoped_lock`** (C++17) | Dùng `std::scoped_lock lock(m1, m2, m3);` để thư viện quản lý việc khóa nhiều mutex theo thuật toán tránh deadlock. |

### Bài tập 4.2: Tự thiết kế `CustomLockGuard` bằng OOP

Để hiểu sâu cơ chế RAII, hãy tự viết một class bọc `std::mutex`:

```cpp
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

// Tự viết LockGuard theo chuẩn RAII (Non-copyable)
template <typename MutexType>
class CustomLockGuard {
private:
    MutexType& mtx_; // Lưu tham chiếu đến mutex gốc

public:
    // Constructor: Nhận mutex và gọi lock() ngay lập tức
    explicit CustomLockGuard(MutexType& mtx) : mtx_(mtx) {
        mtx_.lock();
    }

    // Destructor: Tự động gọi unlock() khi object bị hủy (thu hồi bộ nhớ stack)
    ~CustomLockGuard() {
        mtx_.unlock();
    }

    // Xóa Copy Constructor và Copy Assignment để cấm sao chép LockGuard
    CustomLockGuard(const CustomLockGuard&) = delete;
    CustomLockGuard& operator=(const CustomLockGuard&) = delete;
};

// --- Áp dụng vào code thật ---
std::mutex g_printMutex;
int g_counter = 0;

void incrementWorker(int id) {
    for (int i = 0; i < 1000; ++i) {
        // Khởi tạo RAII Lock Guard -> Tự khóa g_printMutex
        CustomLockGuard<std::mutex> lock(g_printMutex);
        g_counter++;
        // Khi vòng lặp kết thúc scope, 'lock' bị hủy -> Tự động gọi unlock()
    }
}

int main() {
    std::vector<std::thread> workers;
    for (int i = 0; i < 10; ++i) {
        workers.emplace_back(incrementWorker, i);
    }
    for (auto& th : workers) th.join();

    std::cout << "Final Counter (Expected 10000): " << g_counter << "\n";
    return 0;
}
```

## 4.3. `std::condition_variable` — Đồng bộ hóa trạng thái

Khi một luồng cần chờ điều kiện xảy ra, có hai cách tiếp cận chính:

| Cách chờ | Điều xảy ra | Nhận xét |
| --- | --- | --- |
| **Busy-waiting / spinning**<br>`while (!hasData) {}` | Luồng liên tục kiểm tra điều kiện. | Có thể đốt gần 100% một CPU core; chỉ phù hợp cho trường hợp rất đặc biệt và critical section cực ngắn. |
| **`std::condition_variable`** | Luồng ngủ/block, nhường CPU và được đánh thức bằng `notify_one()` hoặc `notify_all()`. | Lựa chọn phù hợp cho hàng đợi, producer-consumer và các điều kiện chờ thông thường. |

### Spurious Wakeup (đánh thức giả) & Predicate

Do cơ chế thiết kế của hệ điều hành phía dưới (Linux NPTL/POSIX), một luồng đang ngủ tại CV có thể tự đột ngột thức dậy dù không có bất kỳ ai gọi notify. Đây gọi là Spurious Wakeup.

Vì vậy, KHÔNG BAO GIỜ dùng lệnh cv.wait(lock); trần trụi. Luôn truyền kèm một Predicate (Lambda function kiểm tra điều kiện):

```cpp
// Cú pháp chuẩn luôn phải áp dụng:
cv.wait(lock, [] { return condition_is_true; });

// Bên dưới compiler sẽ tự dịch dòng trên thành vòng lặp chống Spurious Wakeup:
// while (!condition_is_true) {
//     cv.wait(lock);
// }
```

### Bài tập 4.3: Hàng đợi chặn có kích thước giới hạn (Bounded Blocking Queue)

Đây là mẫu kiến trúc Producer-Consumer kinh điển trong các hệ thống viễn thông và xử lý luồng camera nhúng:

```cpp
#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <optional>
#include <chrono>

template <typename T>
class BoundedBlockingQueue {
private:
    std::queue<T> queue_;
    const size_t capacity_;
    bool closed_ = false;

    std::mutex mtx_;
    std::condition_variable cv_not_full_;  // Producer chờ ở đây nếu queue đầy
    std::condition_variable cv_not_empty_; // Consumer chờ ở đây nếu queue rỗng

public:
    explicit BoundedBlockingQueue(size_t capacity) : capacity_(capacity) {}

    // Push dữ liệu vào queue (Dành cho Producer)
    bool push(T item) {
        std::unique_lock<std::mutex> lock(mtx_);

        // Chờ đến khi queue có chỗ trống HOẶC queue bị đóng
        cv_not_full_.wait(lock, [this] {
            return queue_.size() < capacity_ || closed_;
        });

        if (closed_) return false; // Không nhận thêm dữ liệu nếu đã đóng

        queue_.push(std::move(item));

        // Đánh thức 1 Consumer đang ngủ vì queue rỗng
        cv_not_empty_.notify_one();
        return true;
    }

    // Pop dữ liệu ra khỏi queue (Dành cho Consumer)
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mtx_);

        // Chờ đến khi queue có dữ liệu HOẶC queue bị đóng
        cv_not_empty_.wait(lock, [this] {
            return !queue_.empty() || closed_;
        });

        // Nếu queue đã rỗng và bị đóng lại -> Kết thúc
        if (queue_.empty() && closed_) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop();

        // Đánh thức 1 Producer đang ngủ vì queue đầy
        cv_not_full_.notify_one();
        return item;
    }

    // Đóng queue, đánh thức tất cả các luồng đang chờ
    void close() {
        std::lock_guard<std::mutex> lock(mtx_);
        closed_ = true;
        cv_not_full_.notify_all();  // Đánh thức các Producer đang chờ
        cv_not_empty_.notify_all(); // Đánh thức các Consumer đang chờ
    }
};

// --- Test hệ thống Multi-Producer / Multi-Consumer ---
int main() {
    BoundedBlockingQueue<int> bbq(5); // Queue tối đa 5 phần tử

    // 2 Luồng Producer: Mỗi luồng sinh 10 con số
    auto producer = [&](int id) {
        for (int i = 1; i <= 10; ++i) {
            int val = id * 100 + i;
            if (bbq.push(val)) {
                std::cout << "[Producer " << id << "] Pushed: " << val << "\n";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    };

    // 2 Luồng Consumer: Liên tục rút dữ liệu cho đến khi queue close
    auto consumer = [&](int id) {
        while (true) {
            auto item = bbq.pop();
            if (!item.has_value()) break; // Queue đóng và hết dữ liệu
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

    std::cout << "All threads finished safely.\n";
    return 0;
}
```

## 4.4. `std::atomic` — Đồng bộ hóa atomic

Khi chỉ cần đồng bộ một biến đơn lẻ, như bộ đếm hoặc cờ trạng thái, `std::atomic` thường phù hợp hơn mutex. Các thao tác atomic có thể dùng chỉ lệnh phần cứng chuyên dụng; tuy nhiên không phải mọi `std::atomic<T>` đều được đảm bảo lock-free. `std::atomic_flag` là kiểu được chuẩn C++ bảo đảm lock-free.

| Thao tác | Mục đích |
| --- | --- |
| `load()` / `store()` | Đọc hoặc ghi giá trị atomic. |
| `fetch_add()` / `fetch_sub()` | Cập nhật giá trị và nhận lại giá trị cũ. |
| `compare_exchange_weak()` / `compare_exchange_strong()` | So sánh rồi cập nhật có điều kiện (CAS). |

**Compare-And-Swap (CAS)** là nền tảng của nhiều cấu trúc dữ liệu lock-free: “Nếu giá trị hiện tại bằng giá trị kỳ vọng, hãy đổi nó thành giá trị mới; nếu không, trả lại giá trị hiện tại.”

### Memory Ordering (Mô hình thứ tự bộ nhớ)

| Memory order | Ý nghĩa thực hành | Khuyến nghị |
| --- | --- | --- |
| `std::memory_order_seq_cst` | Mặc định, an toàn và dễ suy luận nhất; tạo một thứ tự tuần tự toàn cục cho các thao tác atomic `seq_cst`. | Dùng khi mới học hoặc khi chưa chứng minh được ordering yếu hơn là đúng. |
| `acquire` / `release` / `relaxed` | Kiểm soát ordering chi tiết hơn, có thể giảm chi phí đồng bộ trong tình huống phù hợp. | Chỉ dùng khi đã mô tả rõ quan hệ Happens-Before cần thiết. |

### Bài tập 4.4: Tự xây dựng Spinlock tối giản bằng `std::atomic_flag`

`std::atomic_flag` là kiểu atomic được chuẩn C++ đảm bảo lock-free trên mọi kiến trúc:

```cpp
#include <iostream>
#include <atomic>
#include <thread>
#include <vector>

class CustomSpinlock {
private:
    // ATOMIC_FLAG_INIT đưa cờ về trạng thái Clear (false - Chưa khóa)
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

public:
    void lock() {
        // test_and_set(): Đọc giá trị cũ và lập tức gán bằng true (Atomic).
        // Nếu giá trị cũ là true (đã có luồng khác khóa), vòng lặp lặp lại liên tục (Spinning).
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // Từ C++20, nên gọi flag_.wait(true) hoặc lệnh pause phần cứng
            // để tránh đốt CPU và giảm nghẽn bus bộ nhớ.
        }
    }

    void unlock() {
        // Trả cờ về false, cho phép luồng đang spin thoát vòng lặp
        flag_.clear(std::memory_order_release);
    }
};

CustomSpinlock g_spinLock;
int g_sharedCounter = 0;

void spinWorker() {
    for (int i = 0; i < 10000; ++i) {
        g_spinLock.lock();
        g_sharedCounter++;
        g_spinLock.unlock();
    }
}

int main() {
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) threads.emplace_back(spinWorker);
    for (auto& th : threads) th.join();

    std::cout << "Spinlock Counter Result: " << g_sharedCounter << " (Expected: 40000)\n";
    return 0;
}
```

## Đánh giá chuyên sâu: Spinlock vs Mutex & các hạn chế

Trong môi trường lập trình nhúng thời gian thực (RTOS / Automotive), spinlock là một con dao hai lưỡi cực kỳ nguy hiểm:

| Tiêu chí | `std::mutex` | Spinlock (`std::atomic_flag`) |
|---|---|---|
| Cơ chế chờ | Sleep/Block: Nhường CPU cho luồng khác khi không lấy được khóa. | Busy-wait: Liên tục kiểm tra cờ và tiêu tốn chu kỳ CPU. |
| Ngữ cảnh áp dụng | Phù hợp với Critical Section lâu hoặc có I/O. | Chỉ phù hợp với Critical Section cực ngắn, không có I/O. |
| Fairness | Thường có cơ chế xếp hàng, giảm Starvation. | Không đảm bảo công bằng, dễ gây Starvation. |
| Priority Inversion | Hệ điều hành có thể hỗ trợ Priority Inheritance. | Có thể rất nghiêm trọng nếu luồng ưu tiên cao spin trong khi luồng ưu tiên thấp giữ khóa. |
Bạn có muốn thử thách sâu hơn vào phần kỹ thuật cao nào dưới đây để chuẩn bị cho phỏng vấn Embedded/Automotive không?

- Phân tích chi tiết Memory Ordering (Acquire/Release/Relaxed)

- Cài đặt Lock-free Stack bằng cơ chế CAS (Compare-And-Swap)

- Cách giải quyết Priority Inversion trong RTOS / Automotive
