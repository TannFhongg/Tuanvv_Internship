template <typename T> 
void grow_and_push(const T& value)
{
    std::size_t old_size = size();
    std::size_t new_capacity =
        old_size == 0 ? 1 : old_size * 2; // Chỉ là ví dụ

    T* new_memory =
        allocator.allocate(new_capacity);

    T* new_end = new_memory;

    try
    {
        for (T* p = begin_; p != end_; ++p)
        {
            std::construct_at(
                new_end,
                std::move_if_noexcept(*p)
            );

            ++new_end;
        }

        std::construct_at(new_end, value);
        ++new_end;
    }
    catch (...)
    {
        std::destroy(new_memory, new_end);
        allocator.deallocate(new_memory, new_capacity);
        throw;
    }

    std::destroy(begin_, end_);
    allocator.deallocate(begin_, capacity());

    begin_        = new_memory;
    end_          = new_end;
    capacity_end_ = new_memory + new_capacity;
}