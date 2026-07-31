template <typename T, typename... Args>
T *create_object(Args &&...args)
{
    void *storage = ::operator new(sizeof(T));
    T *ptr = new (storage) T(std::forward<Args>(args)...);
    return ptr;
}

template <typename T>

void destroy_object(T *ptr)
{
    if (ptr)
    {
        ptr->~T();
        ::operator delete(ptr);
    }
}
int main()
{

    int *p = new int(42);

    void *rawMemory = ::operator new(sizeof(int));

    try
    {
        int *ptr = new (rawMemory) int(42);
        return ptr;
    }
    catch (...)
    {
        ::operator delete(rawMemory);
        throw;
    }
}