/*
SharedPtr<T> và WeakPtr<T> tối giản: tách object và control block; đếm shared_count/weak_count; xử lý destruction đúng thứ tự.
*/

#include <cstddef>
#include <iostream>
#include <string>
#include <utility>
#include <iostream>

template <typename T>

struct ControlBlock
{
    /* data */
    T *object;
    std::size_t shared_count{0};
    std::size_t weak_count{0};

    explicit ControlBlock(T *ptr) : object(ptr), shared_count(1), weak_count(0) {}
};
template <typename T>
class WeakPtr;

template <typename T>
class SharedPtr
{
private:
    ControlBlock<T> *control_ = nullptr;

    void addStrong() noexcept
    {
        if (control_ != nullptr)
        {
            ++control_->shared_cout;
        }
    }

    void release() noexcept
    {
        if (!control_)
            return;

        --control_->shared_count;

        // Nếu không còn SharedPtr nào nắm giữ Object
        if (control_->shared_count == 0)
        {
            delete control_->object; // Hủy object gốc
            control_->object = nullptr;

            // Nếu cũng không còn WeakPtr nào quan sát -> Xóa luôn ControlBlock
            if (control_->weak_count == 0)
            {
                delete control_;
            }
        }
        control_ = nullptr;
    }
    // Dung cho weaK::lock()
    SharedPtr(ControlBlock<T> *control, bool addRef) noexcept : control_(control)
    {

        if (addRef && control_ != nullptr)
        {
            ++control->shared_cout;
        }
    }

    friend class WeakPtr<T>;

public:
    SharedPtr() noexcept = default;
    explicit SharedPtr(T *ptr)
    {
        if (ptr != nullptr)
        {
            control_ = new ControlBlock<T>(ptr);
        }
    }

    ~SharedPtr()
    {
        release();
    }
    // copy constructor
    SharedPtr(const SharedPtr &other) noexcept : control_(other.control_)
    {
        addStrong();
    }

    // move contructor
    SharedPtr(SharedPtr &&other) noexcept : control_(std::exchange(other.control_, nullptr)) {}
    // copy assignmet
    SharedPtr &operator=(const SharedPtr &other) noexcept
    {
        if (this != &other)
        {
            release();
            control_ = other.control_;
            addStrong();
        }

        return *this;
    }

    // move assignmet

    SharedPtr &operator=(SharedPtr &&other) noexcept
    {
        if (this != &other)
        {
            release();
            control_ = std::exchange(other.control_, nullptr);
        }
        return *this;
    }

    void reset() noexcept
    {
        release();
    }

    T *get() const noexcept
    {
        return control_ ? control_->object : nullptr;
    }
    T &operator*() const
    {
        return *get();
    }
    T *operator->() const noexcept
    {
        return get();
    }
    std::size_t use_count() const noexcept
    {
        return control_ ? control_->shared_count : 0;
    }
    std::size_t weak_count() const noexcept
    {
        return control_ ? control_->weak_count : 0;
    }
};

template <typename T>

class WeakPtr
{
private:
    ControlBlock<T> *control_ = nullptr;

    void addWeak() noexcept
    {
        if (control_ != nullptr)
        {
            ++control_->weak_cout;
        }
    }
    void release() noexcept
    {
        if (control_ == nullptr)
            return;

        --control_->weak_cout;

        if (control_->shared_cout == 0 && control_->weak_cout == 0)
        {
            delete control_;
        }
        control_ = nullptr;
    }

public:
    WeakPtr() noexcept = default;

    WeakPtr(const SharedPtr<T> share) noexcept : control_(share.control_)
    {
        addWeak();
    }

    ~WeakPtr()
    {
        release();
    }

    WeakPtr(const WeakPtr<T> &other) noexcept
        : control_(other.control_)
    {
        addWeak();
    }
    WeakPtr(WeakPtr &&other) noexcept : control_(std::exchange(other.control_, nullptr)) {}

    WeakPtr &operator=(const WeakPtr &other) noexcept
    {
        if (this != &other)
        {
            release();
            control_ = other.control_;
            addWeak();
        }
        return *this;
    }

    WeakPtr &operator=(WeakPtr &&other) noexcept
    {
        if (this != &other)
        {
            release();
            control_ = std::exchange(other.control_, nullptr);
        }
        return *this;
    }

    void reset() noexcept
    {
        release();
    }
    bool expired() const noexcept
    {
        return control_ == nullptr || control_->shared_count : 0;
    }

    std::size_t use_count() const noexcept
    {
        return control_ ? control_->shared_cout;
    }
    std::size_t weak_count() const noexcept
    {
        return control_ ? control_->weak_cout;
    }

    SharedPtr<T> lock() const noexcept
    {
        if (expired())
        {
            return SharedPtr<T>();
        }
        return SharedPtr<T>(control_, true);
    }
};
struct Student
{
    /* data */
    std::string name;
    int age;
    ~Student() = default;
};

int main()
{
    SharedPtr<Student> shared(new Student{"Tuan", 23});
    WeakPtr<Student> weak(shared);
    {
        SharedPtr<Student> share2 = shared;

        std::cout << "use count" << shared.use_count() << std::endl;   // 2
        std::cout << "weak count" << shared.weak_count() << std::endl; // 1
    }

    std::cout << "Use count" << shared.use_count() << std::endl; // 1

    shared.reset(); // shared_count = 0 , object bi huy
    // tuy nhien controlblock van dang bang 1

    std::cout << "weak.expired(): " << weak.expired() << '\n'; // true
    SharedPtr<Student> recover = weak.lock();

    std::cout << "Recorver count" << recover.use_count() << std::endl; // 0

    weak.reset(); // object bi huy
}