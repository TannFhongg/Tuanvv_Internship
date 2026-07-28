#include <dlfcn.h>
#include <iostream>

using AddFunction = int (*)(int, int);

int main()
{ /*
  Mở/nạp thư viện động libmath.so.
./ nghĩa là tìm trong thư mục đang chạy chương trình.
RTLD_LAZY: các ký hiệu phụ thuộc trong thư viện được phân giải khi cần dùng, thay vì kiểm tra toàn bộ ngay lúc nạp.
Nếu thành công, handle là “tay cầm” đại diện thư viện; thất bại sẽ là nullptr.
  */
    void *handle = dlopen("./libmath.dll", RTLD_LAZY);

    if (!handle)
    {
        std::cerr << dlerror() << '\n';
        return 1;
    }

    auto add = reinterpret_cast<AddFunction>(
        dlsym(handle, "add"));
    /*
    dlsym(handle, "add"): tìm địa chỉ của ký hiệu/hàm tên add trong thư viện vừa mở.
    dlsym trả về void*, nên reinterpret_cast<AddFunction> chuyển nó thành con trỏ hàm đúng kiểu.
    Biến add lúc này có thể được gọi như một hàm C++ bình thường.
    */
    if (!add)
    {
        std::cerr << dlerror() << '\n';
        dlclose(handle);
        return 1;
    }

    std::cout << add(10, 20) << '\n';

    dlclose(handle);
}