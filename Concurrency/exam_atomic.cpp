#include <atomic>

std::atomic<int> tickets{10};
bool tryTakeTicket()
{

    int currentTicket = tickets.load();

    while (currentTicket > 0)
    {
        if (tickets.compare_exchange_weak(currentTicket, currentTicket - 1))
        {
            return true;
        }
        // Nếu thất bại, currentTicket tự được cập nhật
        // thành số vé mới nhất, rồi thử lại.
    }
    return false;
}

int main()
{
    std::atomic<int> count{0};

    // load() doc giá trị nguyên tử

    int current = count.load();

    // store() ghi gia tri nguyen tu
    count.store(10);

    // fetch_add() cong nguyên tử và trả về giá trị trước khi cộng
    int old = count.fetch_add(1);

    // Compare and exchange CAS
    // - chỉ đổi giá trị nếu nó vẫn đúng kỳ vọng

    int expected = 0;
    bool updated = count.compare_exchange_strong(expected, 1);

    /*
    Kết quả:
    Nếu count == 0: đặt count = 1, trả true; expected vẫn là 0.
    Nếu count != 0: không thay đổi count, trả false; expected bị ghi đè thành giá trị hiện tại của count.
    */
    std::atomic<int> tickets_1{10};
    auto tryTakeTicket = [&tickets_1]()
    {
        int currentTicket_1 = tickets_1.load();

        while (currentTicket_1 > 0)
        {
            if (tickets.compare_exchange_weak(currentTicket_1, currentTicket_1 - 1))
            {
                return true;
            }
            // Nếu thất bại, currentTicket_1 tự được cập nhật
            // thành số vé mới nhất, rồi thử lại.
        }
        return false;
    };
}