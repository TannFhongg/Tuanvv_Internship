/*
BankAccount::transfer đạt strong exception guarantee: nếu thao tác thất bại, dữ liệu không bị thay đổi một phần
*/

#include <iostream>
#include <stdexcept>
#include <string>
#include <memory>
#include <cassert>
class BankAccount
{
private:
    /* data */
    std::string owner_;
    long long balance_;
    long long max_balance_;

public:
    BankAccount(std::string &owner, long long balance, long long max_balance) : owner_(std::move(owner)), balance_(balance), max_balance_(max_balance) {}
    long long balance() const noexcept
    {
        return balance_;
    }
    ~BankAccount() = default;

    void transferTo(
        BankAccount &receiver,
        long long amount)
    {
        if (amount <= 0)
        {
            throw std::invalid_argument("Transfer amout mus be > 0");
        }
        if (balance_ < amount)
        {
            throw std::runtime_error("SO du khong du");
        }
        // thay doi tai khoan gui qua som
        /*
        Tai khoan gui tien truoc khi kiem tra gioi han cua tai khoan nhan
        khien giao dich that bai nhung tai khoan cu dang bi tru tien
        */
        balance_ -= amount;

        if (receiver.balance_ + amount > receiver.max_balance_)
        {
            throw std::runtime_error("So tien vuot qua suc chua cua the");
            /* code */
        }
        receiver.balance_ += amount;
    }
    void tranferToFix(BankAccount &receiver, long long amount) {}
};

void BankAccount::tranferToFix(BankAccount &receiver, long long amount)
{
    if (amount <= 0)
    {
        throw std::invalid_argument("Transfer amout mus be > 0");
    }
    if (balance_ < amount)
    {
        throw std::runtime_error("SO du khong du");
    }

    if (this == &receiver)
        return; // khong the gui cho chinh ban than

    const long long newSenderBalance = balance_ - amount;

    const long long newReceiverBalance = receiver.balance_ + amount;
    // commit cac phep gan so nguyen nay khong nem exception
    balance_ = newSenderBalance;
    receiver.balance_ = newReceiverBalance;
}

int main()
{

    auto alice = std::make_unique<BankAccount>(
        "Alice",
        1'000,
        5'000);
    auto bob = std::make_unique<BankAccount>("Bob", 1'000, 3'000);

    const long long aliceBefore = alice->balance();
    const long long bobBefore = bob->balance();

    try
    {
        alice->transferTo(*bob, 200);
    }
    catch (const std::exception &error)
    {
        std::cout << "Transfer failed" << error.what() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }

    std::cout << "alice" << aliceBefore << std::endl;
    std::cout << "bob" << bobBefore << std::endl;

    assert(alice->balance() == aliceBefore);
    assert(bob->balance() == bobBefore);
}
