# Process, Thread, Race Condition và CPU Cache — Hiểu từ bản chất

## 1. Bức tranh tổng thể

Trong ngữ cảnh này, **cache** được hiểu là **CPU cache**: L1, L2, L3.

Chuỗi hoạt động thực tế:

```text
File chương trình trên ổ đĩa
        ↓ OS nạp
Process: không gian thực thi và tài nguyên
        ↓ tạo các luồng thực thi
Thread: chuỗi lệnh được CPU chạy
        ↓ scheduler phân phối
CPU Core
        ↓ đọc/ghi dữ liệu
Register → L1 Cache → L2 Cache → L3 Cache → RAM
        ↓ nhiều thread cùng truy cập dữ liệu
Race condition có thể xuất hiện
        ↓
Mutex / atomic / condition_variable để đồng bộ
```

Điểm cốt lõi:

> **Process là container tài nguyên. Thread là đơn vị thực thi. CPU core mới là thứ thực sự chạy lệnh.**

---

## 2. Program khác Process như thế nào?

### Program

Program chỉ là một file nằm trên ổ đĩa:

```text
my_app.exe
a.out
```

Nó chứa:

- Mã máy
- Dữ liệu khởi tạo
- Thông tin symbol
- Các section như `.text`, `.data`, `.bss`
- Thông tin thư viện động cần sử dụng

Program ở trên ổ đĩa là một thứ **tĩnh**, chưa chạy.

### Process

Khi chạy program, OS tạo ra một **process**.

Process bao gồm:

```text
Process
├── Virtual address space
├── Page table
├── Code
├── Global/static data
├── Heap
├── Các thread
├── File descriptor / handle
├── Socket
├── Quyền truy cập
└── Thông tin quản lý của kernel
```

Ví dụ, bạn mở Chrome hai lần:

```text
chrome.exe trên ổ đĩa
        ↓
Process Chrome A
Process Chrome B
```

Hai process cùng chạy một program, nhưng mỗi process có:

- Không gian địa chỉ riêng
- Heap riêng
- Biến global riêng
- Tài nguyên quản lý riêng

---

## 3. Khi OS tạo một Process, chuyện gì xảy ra?

Giả sử bạn chạy:

```bash
./app
```

OS không chỉ đơn giản “đọc toàn bộ file vào RAM”. Quy trình gần đúng như sau.

### Bước 1: Kernel tạo cấu trúc quản lý process

Kernel tạo một cấu trúc dữ liệu để lưu:

- Process ID
- Trạng thái process
- Quyền người dùng
- Danh sách thread
- Page table
- File đang mở
- Signal
- Thông tin scheduler
- Thống kê CPU

Cấu trúc này thường được gọi khái quát là **PCB — Process Control Block**.

Trên Linux, phần lớn thông tin liên quan được quản lý trong các cấu trúc như `task_struct`, `mm_struct`, bảng file descriptor...

### Bước 2: Tạo không gian địa chỉ ảo

Process nhìn thấy một không gian địa chỉ giống như:

```text
Địa chỉ cao
+-----------------------+
| Kernel space          |
+-----------------------+
| Stack                 | ↓ phát triển xuống
+-----------------------+
| Memory mapped region  |
| Shared libraries      |
+-----------------------+
| Heap                  | ↑ phát triển lên
+-----------------------+
| BSS                   |
+-----------------------+
| Data                  |
+-----------------------+
| Code / Text           |
+-----------------------+
Địa chỉ thấp
```

Đây là **virtual address space**, không phải sơ đồ RAM vật lý.

Mỗi process thường thấy một không gian địa chỉ riêng. Hai process có thể đều có một biến ở địa chỉ ảo:

```text
0x7ffd12340000
```

nhưng MMU có thể ánh xạ chúng đến hai vùng RAM vật lý hoàn toàn khác nhau.

### Bước 3: OS ánh xạ file thực thi

OS ánh xạ các phần của file executable vào không gian địa chỉ:

```text
.text  → mã máy, thường read + execute
.data  → biến global đã khởi tạo, read + write
.bss   → biến global chưa khởi tạo, ban đầu bằng 0
```

OS thường sử dụng **demand paging**. Nghĩa là không nhất thiết nạp tất cả dữ liệu vào RAM ngay lập tức.

Khi CPU lần đầu truy cập một page chưa có trong RAM:

```text
CPU truy cập địa chỉ
        ↓
Page table báo page chưa hiện diện
        ↓
Page fault
        ↓
Kernel nạp hoặc tạo page
        ↓
Tiếp tục chạy instruction
```

### Bước 4: Tạo thread đầu tiên

Mỗi process khi bắt đầu thường có ít nhất một thread:

```text
Process
└── Main thread
```

Main thread bắt đầu chạy mã khởi động runtime, sau đó mới gọi:

```cpp
int main()
```

Trước `main()`, có thể đã xảy ra:

- Loader nạp thư viện động
- Runtime C/C++ được khởi tạo
- Global object được construct
- Stack được thiết lập
- Argument `argc`, `argv` được chuẩn bị

---

## 4. Process có thực sự “chạy” không?

Nói chính xác:

> Process không trực tiếp chạy. **Thread bên trong process mới được scheduler đưa lên CPU để chạy.**

Khi nói “process đang chạy”, thực chất là:

```text
Ít nhất một thread của process đang chạy trên CPU
```

---

## 5. Thread là gì?

Thread là một **luồng thực thi tuần tự**.

Mỗi thread có riêng:

```text
Thread
├── Program counter / instruction pointer
├── CPU registers
├── Stack
├── Stack pointer
├── Thread-local storage
├── Scheduling state
└── Kernel stack
```

Nhưng các thread trong cùng process chia sẻ:

```text
Shared
├── Code
├── Global variables
├── Static variables
├── Heap
├── Memory mappings
├── File descriptors
└── Sockets
```

Ví dụ:

```cpp
int globalCounter = 0;

void worker() {
    int localValue = 10;
}
```

Nếu có hai thread:

- `globalCounter` được cả hai thread truy cập
- Mỗi thread có một bản `localValue` trên stack riêng

Minh họa:

```text
Process address space
+----------------------------------+
| Code          - shared           |
+----------------------------------+
| Global data   - shared           |
+----------------------------------+
| Heap          - shared           |
+----------------------------------+
| Thread 1 stack                   |
+----------------------------------+
| Thread 2 stack                   |
+----------------------------------+
```

Cần hiểu chính xác hơn: stack của từng thread vẫn nằm trong virtual address space chung của process. Nó “riêng” theo quy ước sử dụng, không phải vùng mà thread khác tuyệt đối không thể truy cập.

Nếu thread 1 có con trỏ trỏ vào stack của thread 2, về mặt kỹ thuật nó có thể đọc hoặc ghi vùng đó. Nhưng việc này rất nguy hiểm vì lifetime và đồng bộ.

---

## 6. CPU thực sự chạy Thread như thế nào?

Giả sử máy có:

```text
4 CPU cores
8 threads đang runnable
```

Tại một thời điểm, tối đa khoảng 4 thread có thể thực sự thực thi song song, mỗi core chạy một thread.

Các thread còn lại chờ scheduler.

```text
Core 0 → Thread A
Core 1 → Thread B
Core 2 → Thread C
Core 3 → Thread D

Runnable queue:
Thread E, F, G, H
```

Sau một khoảng thời gian hoặc khi thread bị block, scheduler có thể đổi:

```text
Core 0: Thread A → Thread E
```

Việc đổi thread đang chạy được gọi là **context switch**.

---

## 7. Context switch thực hiện gì?

Giả sử CPU đang chạy thread A và muốn chuyển sang thread B.

### Bước 1: Dừng thread A

Điều này có thể xảy ra do:

- Hết time slice
- Thread gọi system call và phải chờ
- Thread chờ mutex
- Thread chờ I/O
- Có thread ưu tiên cao hơn
- Có interrupt
- Thread chủ động `yield`

### Bước 2: Lưu CPU context của A

Kernel lưu các thông tin như:

```text
Instruction pointer
Stack pointer
General-purpose registers
Flags
SIMD registers khi cần
Scheduling state
```

### Bước 3: Chọn thread B

Scheduler chọn một thread runnable phù hợp dựa trên:

- Priority
- Thời gian đã chạy
- CPU affinity
- Chính sách scheduling
- Load của từng core

### Bước 4: Chuyển môi trường bộ nhớ khi cần

Nếu A và B thuộc hai process khác nhau, kernel có thể phải chuyển sang page table của process B.

Nếu A và B là hai thread trong cùng process, chúng thường dùng chung page table nên không cần đổi toàn bộ address space.

Tuy nhiên, thread context switch vẫn không miễn phí.

### Bước 5: Khôi phục context của B

CPU nạp lại:

- Registers
- Stack pointer
- Instruction pointer

Sau đó B tiếp tục từ nơi nó đã dừng trước đó.

### Cache có bị xóa khi context switch không?

Thông thường **không xóa toàn bộ cache**.

Nhưng dữ liệu của thread mới có thể không nằm trong cache. Khi đó cache miss tăng lên và CPU phải lấy dữ liệu từ cache cấp thấp hơn hoặc RAM.

Đây là một lý do context switching quá nhiều làm giảm hiệu năng.

---

## 8. Process và Thread khác nhau ở đâu?

| Đặc điểm | Process | Thread |
|---|---|---|
| Address space | Thường riêng biệt | Chia sẻ trong cùng process |
| Heap | Riêng giữa các process | Chia sẻ |
| Global variables | Riêng giữa các process | Chia sẻ |
| Stack | Có các stack của thread | Mỗi thread có stack riêng |
| File/socket | Process sở hữu | Các thread thường chia sẻ |
| Giao tiếp | IPC, shared memory, socket | Đọc/ghi bộ nhớ chung |
| Isolation | Cao hơn | Thấp hơn |
| Lỗi memory | Thường giới hạn trong process | Một thread có thể phá toàn process |
| Context switch | Thường nặng hơn | Thường nhẹ hơn |
| Đồng bộ | Khi dùng tài nguyên chung | Thường xuyên cần thiết |

“Thread nhẹ hơn process” là một cách nói tương đối, không có nghĩa thread miễn phí.

Tạo quá nhiều thread gây ra:

- Nhiều stack
- Nhiều context switch
- Scheduler overhead
- Cache miss
- Lock contention
- Memory consumption

---

## 9. Race Condition là gì?

Race condition xảy ra khi:

> Kết quả chương trình phụ thuộc vào thứ tự hoặc thời điểm thực thi giữa nhiều thread/process.

Ví dụ:

```cpp
int counter = 0;

void increment() {
    ++counter;
}
```

Hai thread cùng gọi `increment()`.

Bạn có thể nghĩ:

```text
Thread A tăng counter 1 lần
Thread B tăng counter 1 lần
Kết quả phải là 2
```

Nhưng `++counter` thường không phải một thao tác duy nhất.

Nó có thể được tách thành:

```text
1. Load counter từ memory vào register
2. Cộng register với 1
3. Store register trở lại memory
```

Interleaving có thể xảy ra:

```text
Ban đầu counter = 0

Thread A: load counter → A.register = 0
Thread B: load counter → B.register = 0

Thread A: A.register = 1
Thread B: B.register = 1

Thread A: store 1 vào counter
Thread B: store 1 vào counter

Kết quả counter = 1
```

Một lần tăng đã bị mất. Đây được gọi là **lost update**.

---

## 10. Race Condition và Data Race không hoàn toàn giống nhau

### Race condition

Khái niệm rộng:

```text
Kết quả logic phụ thuộc vào timing hoặc thứ tự execution
```

Có thể xảy ra ngay cả khi mọi thao tác riêng lẻ đều atomic.

Ví dụ:

```cpp
if (balance >= amount) {
    balance -= amount;
}
```

Giả sử `balance` là atomic. Hai thread vẫn có thể cùng kiểm tra thấy đủ tiền rồi cùng trừ.

Từng load/store có thể atomic, nhưng toàn bộ thao tác:

```text
check → then act
```

không atomic.

### Data race trong C++

C++ định nghĩa data race khi:

- Hai thread truy cập cùng một memory location
- Ít nhất một thao tác là write
- Các truy cập không được đồng bộ
- Các truy cập không phải tất cả đều atomic

Ví dụ:

```cpp
int counter = 0;

// Thread A
counter++;

// Thread B
counter++;
```

Đây là data race.

Trong C++, data race trên biến non-atomic dẫn đến **undefined behavior**.

Không chỉ là “đôi lúc kết quả sai”. Compiler được phép tối ưu dựa trên giả định rằng data race không tồn tại.

---

## 11. Race Condition có cần nhiều CPU core không?

Không.

Ngay cả máy một core vẫn có race condition vì scheduler có thể ngắt thread tại bất kỳ thời điểm phù hợp nào.

```text
Thread A: load counter
       ← context switch
Thread B: load, add, store
       ← context switch
Thread A: add, store
```

Nhiều core chỉ làm các race xảy ra thực sự song song và thường khó dự đoán hơn.

---

## 12. CPU Cache là gì?

RAM chậm hơn CPU rất nhiều.

CPU có thể xử lý instruction nhanh hơn nhiều so với thời gian đợi dữ liệu từ RAM. Vì vậy CPU sử dụng nhiều tầng lưu trữ:

```text
Nhanh, nhỏ
┌─────────────────┐
│ Registers       │
├─────────────────┤
│ L1 Cache        │
├─────────────────┤
│ L2 Cache        │
├─────────────────┤
│ L3 Cache        │
├─────────────────┤
│ RAM             │
├─────────────────┤
│ SSD / Disk      │
└─────────────────┘
Chậm, lớn
```

Thông thường:

- Registers thuộc core đang thực thi
- L1 thường riêng cho từng core
- L2 thường riêng hoặc gần từng core
- L3 thường được chia sẻ giữa nhiều core
- RAM nằm ngoài CPU

Các chi tiết chính xác phụ thuộc kiến trúc CPU.

---

## 13. CPU Cache không đọc từng byte riêng lẻ

CPU cache hoạt động theo **cache line**.

Trên nhiều CPU hiện đại, cache line thường có kích thước 64 byte, dù đây không phải quy luật bắt buộc cho mọi kiến trúc.

Giả sử bạn đọc:

```cpp
array[0]
```

CPU có thể nạp cả một vùng:

```text
array[0] ... array[15]
```

vào cache nếu mỗi phần tử là `int` 4 byte.

Lý do là chương trình thường có **spatial locality**:

> Nếu vừa dùng một địa chỉ, khả năng cao sắp tới sẽ dùng địa chỉ gần nó.

Ngoài ra còn có **temporal locality**:

> Nếu vừa dùng một dữ liệu, khả năng cao sắp tới sẽ dùng lại nó.

Đó là lý do duyệt `std::vector` liên tục thường cache-friendly hơn duyệt linked list với các node nằm rải rác trên heap.

---

## 14. Con trỏ, MMU và Cache phối hợp như thế nào?

Giả sử code:

```cpp
int value = *ptr;
```

Quy trình khái quát:

```text
ptr chứa virtual address
        ↓
MMU tra TLB / page table
        ↓
Xác định physical address
        ↓
CPU kiểm tra cache
        ↓
Cache hit: lấy dữ liệu nhanh
Cache miss: lấy từ cache cấp thấp hơn hoặc RAM
        ↓
Đưa dữ liệu vào register
```

Không phải mỗi lần đọc biến đều trực tiếp đọc RAM.

Phần lớn truy cập bộ nhớ hiệu năng cao diễn ra qua cache.

---

## 15. Vấn đề khi nhiều Core có Cache riêng

Giả sử:

```cpp
int counter = 0;
```

Thread A chạy trên Core 0. Thread B chạy trên Core 1.

```text
Core 0 cache: cache line chứa counter
Core 1 cache: cache line chứa counter
RAM:         vùng vật lý chứa counter
```

Có vẻ như mỗi core đang có một bản sao.

CPU giải quyết vấn đề này bằng **cache coherence protocol**.

Ý tưởng:

- Khi Core 0 muốn ghi một cache line
- Nó phải giành quyền sở hữu line đó
- Các bản sao tương ứng trong cache của core khác sẽ bị invalidated hoặc cập nhật theo protocol
- Khi Core 1 đọc lại, nó nhận giá trị mới theo quy tắc coherence

Các protocol thường được giải thích bằng các trạng thái kiểu MESI:

```text
Modified
Exclusive
Shared
Invalid
```

Không cần nhớ từng trạng thái ngay. Điều quan trọng là:

> Cache coherence cố gắng duy trì sự nhất quán của cùng một cache line giữa các core.

Nhưng cache coherence **không tự động làm chương trình thread-safe**.

---

## 16. Tại sao Cache Coherence không ngăn Race Condition?

Với:

```cpp
++counter;
```

Cache coherence có thể đảm bảo các core cuối cùng thống nhất về cache line.

Nhưng thao tác vẫn gồm:

```text
load → modify → store
```

Hai core vẫn có thể cùng load giá trị cũ trước khi một core store kết quả.

```text
Core 0 load 0
Core 1 load 0
Core 0 store 1
Core 1 store 1
```

Cache coherence đảm bảo cuối cùng cả hai thấy `1`.

Nhưng kết quả đúng lẽ ra phải là `2`.

Vì vậy:

> **Coherence giải quyết “các cache nhìn thấy cùng dữ liệu”. Atomicity giải quyết “thao tác có bị xen ngang hay không”.**

Đây là hai vấn đề khác nhau.

---

## 17. Memory Ordering: lệnh không nhất thiết diễn ra đúng thứ tự bạn viết

Xét:

```cpp
data = 42;
ready = true;
```

Bạn có thể nghĩ CPU luôn thực hiện theo đúng thứ tự đó.

Nhưng compiler và CPU có thể reorder thao tác để tăng hiệu năng, miễn là trong một thread kết quả quan sát được vẫn hợp lệ.

Ngoài ra CPU có:

- Store buffer
- Load buffer
- Out-of-order execution
- Speculative execution

Thread khác có thể quan sát trạng thái theo thứ tự khác nếu không có đồng bộ phù hợp.

Ví dụ không an toàn:

```cpp
int data = 0;
bool ready = false;

// Thread A
data = 42;
ready = true;

// Thread B
if (ready) {
    std::cout << data;
}
```

Đây có data race trên cả `ready` và `data`.

Không thể chỉ dựa vào trực giác “A ghi data trước rồi mới ghi ready”.

C++ cung cấp memory model với quan hệ **happens-before** để định nghĩa khi nào một thread chắc chắn quan sát được ghi của thread khác.

Mutex và atomic với memory ordering thích hợp tạo ra các quan hệ này.

---

## 18. `volatile` không giải quyết Thread Safety

Nhiều người viết:

```cpp
volatile bool ready = false;
```

và nghĩ rằng nó giúp các thread nhìn thấy giá trị mới.

Trong C++, `volatile` chủ yếu nói với compiler rằng việc đọc/ghi có side effect đặc biệt và không nên bị loại bỏ theo một số kiểu tối ưu.

Nó không đảm bảo đầy đủ:

- Atomicity
- Mutual exclusion
- Happens-before
- Thread synchronization
- Memory ordering giữa các thread

Cho giao tiếp thread, dùng:

```cpp
std::atomic<bool>
```

hoặc mutex.

---

## 19. Mutex giải quyết Race Condition như thế nào?

Ví dụ:

```cpp
#include <mutex>

int counter = 0;
std::mutex mtx;

void increment() {
    std::lock_guard<std::mutex> lock(mtx);
    ++counter;
}
```

Mutex tạo ra **critical section**:

```text
lock
  ↓
Chỉ một thread được vào
  ↓
đọc → sửa → ghi
  ↓
unlock
```

Interleaving:

```text
Thread A: lock thành công
Thread B: lock thất bại, phải chờ

Thread A: counter++
Thread A: unlock

Thread B: lock thành công
Thread B: counter++
Thread B: unlock
```

Kết quả đúng là `2`.

### Mutex dưới tầng thấp

Một mutex hiệu quả thường có hai đường đi.

#### Fast path

Khi mutex đang rảnh:

```text
Atomic compare-and-swap
        ↓
Chiếm mutex ngay trong user space
```

Không cần chuyển vào kernel.

#### Slow path

Khi mutex đang bị giữ:

```text
Thử atomic nhưng thất bại
        ↓
Có thể spin trong thời gian ngắn
        ↓
System call
        ↓
Kernel đưa thread vào trạng thái sleeping
        ↓
Thread giữ mutex unlock
        ↓
Kernel đánh thức thread chờ
```

Trên Linux, cơ chế kiểu `futex` thường được dùng để tối ưu mô hình này.

Ý tưởng của futex:

> Trường hợp không tranh chấp xử lý ở user space; chỉ vào kernel khi thật sự phải ngủ hoặc đánh thức.

---

## 20. Atomic hoạt động như thế nào?

Ví dụ:

```cpp
#include <atomic>

std::atomic<int> counter{0};

void increment() {
    counter.fetch_add(1);
}
```

`fetch_add` là một atomic read-modify-write operation.

Về mặt logic:

```text
Đọc counter
Cộng 1
Ghi lại
```

được xem như một đơn vị không thể bị thread khác quan sát ở trạng thái giữa.

CPU có thể sử dụng:

- Atomic instruction
- Cache-line ownership
- Cache coherence
- Memory barriers
- Bus locking trong một số trường hợp đặc biệt

Trên x86, compiler có thể sinh instruction dạng:

```asm
lock xadd
```

tùy code và compiler.

Điều quan trọng:

> Atomic không nhất thiết khóa toàn bộ memory bus. Thường CPU độc quyền cache line liên quan thông qua cache-coherence protocol.

---

## 21. Mutex hay Atomic?

### Dùng atomic khi

- Trạng thái đơn giản
- Counter
- Flag
- Pointer trạng thái
- Một số cấu trúc lock-free đã được thiết kế cẩn thận

Ví dụ:

```cpp
std::atomic<bool> stopped{false};
std::atomic<int> count{0};
```

### Dùng mutex khi

- Nhiều biến phải thay đổi cùng nhau
- Có invariant cần bảo vệ
- Critical section phức tạp
- Container như vector/map được truy cập chung

Ví dụ:

```cpp
std::mutex mtx;
std::vector<int> values;

void add(int value) {
    std::lock_guard<std::mutex> lock(mtx);
    values.push_back(value);
}
```

Chỉ biến `size` atomic không làm toàn bộ `vector` thread-safe.

---

## 22. Một Atomic Variable không tự động làm thuật toán Atomic

Ví dụ:

```cpp
std::atomic<int> balance{100};

void withdraw(int amount) {
    if (balance.load() >= amount) {
        balance.fetch_sub(amount);
    }
}
```

Hai thread cùng rút 80:

```text
Thread A thấy balance = 100
Thread B thấy balance = 100

Thread A trừ 80 → 20
Thread B trừ 80 → -60
```

Mỗi thao tác atomic, nhưng chuỗi:

```text
check → update
```

không phải một atomic transaction.

Có thể cần:

- Mutex
- Compare-and-swap loop
- Transactional logic

Ví dụ với mutex:

```cpp
std::mutex balanceMutex;
int balance = 100;

bool withdraw(int amount) {
    std::lock_guard<std::mutex> lock(balanceMutex);

    if (balance < amount) {
        return false;
    }

    balance -= amount;
    return true;
}
```

---

## 23. False Sharing: không dùng chung biến nhưng vẫn chậm

Giả sử:

```cpp
struct Counters {
    int counterA;
    int counterB;
};
```

Thread A chỉ cập nhật `counterA`.

Thread B chỉ cập nhật `counterB`.

Về logic, hai thread không dùng chung một biến. Nhưng hai biến có thể nằm trên cùng một cache line:

```text
Cache line 64 bytes
+----------------------------------+
| counterA | counterB | ...        |
+----------------------------------+
```

Khi Core 0 ghi `counterA`, nó giành quyền sở hữu toàn cache line.

Khi Core 1 ghi `counterB`, nó lại giành quyền sở hữu cùng cache line.

Cache line bị chuyển qua lại giữa các core:

```text
Core 0 ↔ Core 1 ↔ Core 0 ↔ Core 1
```

Hiện tượng này gọi là **false sharing**.

Không có data race nếu mỗi thread dùng biến riêng, nhưng hiệu năng có thể giảm mạnh.

Có thể tách chúng ra các cache line khác nhau:

```cpp
struct alignas(64) Counter {
    std::atomic<int> value{0};
};

Counter counterA;
Counter counterB;
```

Con số `64` phụ thuộc giả định kiến trúc. Trong C++ còn có `std::hardware_destructive_interference_size`, nhưng mức hỗ trợ và giá trị phụ thuộc implementation.

---

## 24. Cache Locality ảnh hưởng đến lựa chọn cấu trúc dữ liệu

So sánh:

```cpp
std::vector<int>
std::list<int>
```

### Vector

Dữ liệu liên tục:

```text
[1][2][3][4][5][6]
```

CPU đọc một cache line có thể lấy được nhiều phần tử.

### Linked list

Các node có thể nằm rải rác:

```text
Node A → Node B → Node C
0x1000   0xA830   0x5230
```

Mỗi lần theo con trỏ có thể:

- Cache miss
- TLB miss
- Phải đợi memory

Vì vậy, dù cả hai đều duyệt `O(n)`, vector thường nhanh hơn đáng kể nhờ cache locality.

Big-O không mô tả toàn bộ chi phí phần cứng.

---

## 25. Process riêng biệt có Race Condition không?

Có, nếu chúng chia sẻ tài nguyên.

Hai process mặc định có address space riêng, nhưng vẫn có thể race trên:

- Shared memory
- Memory-mapped file
- File trên ổ đĩa
- Database record
- Socket protocol
- Named semaphore
- Kernel object
- Thiết bị phần cứng

Ví dụ hai process cùng làm:

```text
Đọc số dư từ file
Trừ tiền
Ghi số dư mới
```

Nếu không có file lock hoặc transaction, chúng có thể gây lost update tương tự thread.

---

## 26. Các trạng thái chính của Thread/Process

Một thread thường ở một trong các trạng thái khái quát:

```text
New
  ↓
Runnable
  ↓ scheduler chọn
Running
  ↓
Blocked / Sleeping
  ↓ sự kiện hoàn thành
Runnable
  ↓
Terminated
```

Phân biệt:

- **Running**: đang chạy trên CPU
- **Runnable**: sẵn sàng chạy nhưng đang chờ CPU
- **Blocked/Sleeping**: chưa thể chạy vì đang chờ I/O, mutex, condition...
- **Terminated**: đã kết thúc

Một thread đang chờ network không nhất thiết tiêu thụ CPU. Kernel có thể cho nó ngủ và chạy thread khác.

---

## 27. Thread kết thúc thì điều gì xảy ra?

Khi thread kết thúc:

- Stack của thread được giải phóng sau khi runtime/kernel dọn dẹp
- Thread-local objects bị destruct
- Kernel scheduling state được xóa
- Giá trị trả về có thể được lưu cho `join`
- Các tài nguyên process dùng chung vẫn tồn tại nếu process còn thread khác

Khi thread cuối cùng kết thúc, process kết thúc:

- Address space được giải phóng
- File descriptor được đóng
- Memory mappings được hủy
- Kernel thu hồi tài nguyên
- Parent process có thể nhận exit status

Trên Unix-like system, một process đã kết thúc nhưng parent chưa thu thập exit status có thể ở trạng thái zombie. Zombie không còn chạy code; nó chỉ còn thông tin trạng thái tối thiểu để parent đọc.

---

## 28. `std::thread` dưới tầng thấp

Khi viết:

```cpp
std::thread worker(task);
```

Thư viện chuẩn C++ gọi API thread của hệ điều hành.

Khái quát:

```text
std::thread
    ↓
Runtime / standard library
    ↓
OS thread API
    ↓
Kernel tạo scheduling entity
    ↓
Cấp stack
    ↓
Khởi tạo instruction pointer
    ↓
Đưa thread vào runnable queue
```

`std::thread` không phải “thread riêng của C++”. Nó là abstraction trên native thread của OS.

Khi gọi:

```cpp
worker.join();
```

Thread hiện tại chờ đến khi worker kết thúc.

Khi gọi:

```cpp
worker.detach();
```

Đối tượng `std::thread` không còn quản lý việc join thread đó. Thread vẫn có thể chạy độc lập, khiến việc quản lý lifetime rất nguy hiểm.

---

## 29. Mối quan hệ tổng hợp

### Process cung cấp

- Không gian địa chỉ
- Tài nguyên
- Isolation
- Security boundary
- Môi trường cho các thread

### Thread cung cấp

- Chuỗi instruction
- Registers
- Stack
- Đơn vị được scheduler chạy

### Scheduler cung cấp

- Chọn thread nào chạy
- Chọn core nào
- Chuyển đổi giữa các thread
- Quản lý priority và fairness

### Cache cung cấp

- Dữ liệu gần CPU
- Giảm độ trễ RAM
- Tăng hiệu suất nhờ locality

### Race condition xuất hiện khi

- Nhiều execution flow
- Có trạng thái chung
- Có ít nhất một thao tác thay đổi
- Không có synchronization đúng

---

## 30. Mô hình tư duy chính xác

Hãy hình dung CPU core như một người công nhân.

### Process là một xưởng

Xưởng có:

- Kho vật liệu: heap
- Tài liệu chung: global data
- Máy móc và file: resources
- Bản đồ địa chỉ: page table

### Thread là một công nhân

Mỗi công nhân có:

- Sổ ghi chú riêng: stack
- Trạng thái đang làm: registers
- Vị trí trong quy trình: instruction pointer

### Cache là bàn làm việc gần công nhân

Thay vì mỗi lần đều đi tới kho RAM, công nhân giữ dữ liệu thường dùng trên bàn.

### Race condition

Hai công nhân cùng đọc tờ giấy:

```text
Số lượng = 0
```

Cả hai đều viết:

```text
Số lượng mới = 1
```

Kết quả cuối cùng là `1`, dù cả hai đều đã tăng.

### Mutex

Chỉ người giữ chìa khóa mới được sửa tờ giấy.

```text
Lấy chìa khóa
Đọc
Sửa
Ghi
Trả chìa khóa
```

### Atomic

Một loại máy đặc biệt thực hiện thao tác tăng như một hành động không thể bị chen ngang.

Ẩn dụ này chỉ để hình dung. Bản chất thật vẫn là:

```text
CPU instructions
Cache coherence
Atomic read-modify-write
Memory ordering
OS scheduling
```

---

## 31. Những hiểu lầm quan trọng

### “Mỗi thread có heap riêng”

Sai. Các thread trong cùng process thường chia sẻ heap.

Một số memory allocator có **per-thread cache** để tăng hiệu năng, nhưng vùng nhớ được cấp vẫn thuộc address space chung của process.

### “Một core thì không có race”

Sai. Context switch vẫn tạo interleaving.

### “Cache coherence làm code thread-safe”

Sai. Nó không đảm bảo atomicity của thuật toán.

### “Dùng atomic cho một biến là toàn bộ class thread-safe”

Sai. Thread safety phụ thuộc invariant của toàn bộ trạng thái.

### “Mutex luôn gọi kernel”

Sai. Trường hợp không contention thường có thể lock bằng atomic fast path trong user space.

### “Nhiều thread luôn nhanh hơn”

Sai. Có thể chậm hơn do:

- Context switch
- Lock contention
- Cache miss
- False sharing
- Memory bandwidth
- Công việc không đủ lớn
- Phần tuần tự của thuật toán

### “Sleep là cách đồng bộ thread”

Sai. `sleep` chỉ thay đổi timing, không tạo quan hệ đồng bộ chính xác.

---

## 32. Cách giải thích lại cho người mới

> Program là file chứa mã lệnh nằm trên ổ đĩa. Khi chạy program, hệ điều hành tạo một process. Process là môi trường chứa bộ nhớ và tài nguyên của chương trình. Trong process có một hoặc nhiều thread. Thread là luồng lệnh mà CPU thực sự thực thi.
>
> Mỗi thread có stack và thanh ghi riêng, nhưng các thread trong cùng process chia sẻ heap và biến global. Vì chia sẻ dữ liệu nên nếu hai thread cùng sửa một biến mà không phối hợp, kết quả có thể phụ thuộc vào thời điểm chạy. Đó là race condition.
>
> CPU không đọc RAM trực tiếp cho mọi thao tác mà giữ dữ liệu thường dùng trong cache. Mỗi core có thể có cache riêng, nên phần cứng cần cache coherence để các core thống nhất dữ liệu. Tuy nhiên coherence không làm các thao tác như `counter++` trở thành atomic.
>
> Để nhiều thread làm việc an toàn, ta dùng mutex để chỉ cho một thread vào vùng critical section, hoặc dùng atomic cho các thao tác đơn giản không được phép bị xen ngang.

---

## 33. Bốn câu kiểm tra bản chất

### 1. Vì sao `counter++` không thread-safe?

Vì nó thường là chuỗi:

```text
load → add → store
```

Hai thread có thể cùng load giá trị cũ và ghi đè kết quả của nhau.

### 2. Vì sao mỗi thread cần stack riêng?

Vì mỗi thread cần giữ riêng:

- Local variables
- Function call frames
- Return addresses
- Saved registers
- Stack pointer

Nếu dùng chung một stack cho việc gọi hàm thông thường, các call frame sẽ ghi đè nhau.

### 3. Cache coherence có tác dụng gì?

Nó duy trì tính nhất quán của các bản sao cache line giữa nhiều CPU core.

Nó không biến một chuỗi nhiều instruction thành một atomic operation.

### 4. Vì sao context switch tốn chi phí?

Vì hệ thống phải:

- Dừng execution hiện tại
- Lưu registers
- Chạy scheduler
- Khôi phục thread khác
- Có thể chuyển page table
- Làm giảm cache/TLB locality
- Có thể chuyển user mode ↔ kernel mode

---

## Kết luận

Bản chất của toàn bộ chủ đề nằm trong sáu ý:

1. **Process là môi trường tài nguyên và không gian địa chỉ.**
2. **Thread là đơn vị thực thi được scheduler đưa lên CPU.**
3. **Các thread có execution state riêng nhưng chia sẻ phần lớn memory của process.**
4. **Race condition xuất hiện khi kết quả phụ thuộc vào interleaving không được kiểm soát.**
5. **CPU cache làm truy cập bộ nhớ nhanh hơn nhưng tạo thêm vấn đề về visibility, ordering và false sharing.**
6. **Mutex, atomic và các synchronization primitive tạo atomicity, ordering và happens-before cần thiết.**
