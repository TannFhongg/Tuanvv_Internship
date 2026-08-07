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
    native_handle_type native_handle();

    static unsigned int hardware_concurrency() noexcept;
};