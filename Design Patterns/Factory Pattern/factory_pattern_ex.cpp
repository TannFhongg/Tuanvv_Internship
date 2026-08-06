/*
Factory Pattern
Primary befinet :It promotes loose coupling
by delegating object creation to subclasses.

Tao object ma khong can biet chinh xac class cu the nao duoc khoi tao
*/
#include <iostream>
#include <memory>

class Payment
{
public:
    virtual void pay() const = 0;
    virtual ~Payment() = default;
};

class TienMat : public Payment
{
public:
    void pay() const override
    {
        std::cout << "Lua chon thanh toan bang tien mat \n";
    }
};

class TienThe : public Payment
{
public:
    void pay() const override
    {
        std::cout << "Lua chon thanh toan bang tien the \n";
    }
};

class PaymentFactory
{
public:
    virtual std::unique_ptr<Payment> creatorPay() const = 0;
    virtual ~PaymentFactory() = default;
};

class TienViet : public PaymentFactory
{
public:
    std::unique_ptr<Payment> creatorPay() const override
    {
        return std::make_unique<TienMat>();
    }
};
class Momo : public PaymentFactory
{
public:
    std::unique_ptr<Payment> creatorPay() const override
    {
        return std::make_unique<TienThe>();
    }
};

void processPay(const PaymentFactory &factory)
{
    std::unique_ptr<Payment> pay = factory.creatorPay();
    pay->pay();
}

int main()
{
    std::unique_ptr<PaymentFactory> tienViet = std::make_unique<TienViet>();
    std::unique_ptr<Payment> thanhToanTienMaT = tienViet->creatorPay();
    thanhToanTienMaT->pay();

    std::unique_ptr<PaymentFactory> moMo = std::make_unique<Momo>();
    std::unique_ptr<Payment> thanhToanTienThe = moMo->creatorPay();
    thanhToanTienThe->pay();

    TienViet vnd;
    processPay(vnd);
    Momo credit;
    processPay(credit);
}