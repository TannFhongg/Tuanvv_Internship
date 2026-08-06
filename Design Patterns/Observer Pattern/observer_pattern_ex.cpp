/*
Observer Pattern
Intent: Define a one-to-many dependency between objects
so that when one object changes state,
all its dependents are notified and updated automatically.

Mot thiet ke cho phep mot object tu dong thong bao cho nhieu object khac
khi trang thai cua no thay doi
*/

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

enum class EventType
{
    OrderCreated,
    LowStock,
    PaymentFailed
};

class Observer
{
public:
    virtual ~Observer() = default;
    virtual bool shouldNotify(EventType eventType) const = 0;
    virtual void update(const std::string &message) = 0;
};

class Subject
{

private:
    std::vector<Observer *> observers;

public:
    void attach(Observer *observer)
    {
        observers.push_back(observer);
    }

    void detach(Observer *observer)
    {
        observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
    }

    void notify(EventType eventType, const std::string &message)
    {
        for (auto ob : observers)
        {
            if (ob->shouldNotify(eventType))
            {
                ob->update(message);
            }
        }
    }
};

class CustomerEmailObserver : public Observer
{
public:
    bool shouldNotify(EventType eventType) const override
    {
        return eventType == EventType::OrderCreated;
    }

    void update(const std::string &message) override
    {
        std::cout << "Customer email: " << message << std::endl;
    }
};

class InventoryObserver : public Observer
{
public:
    bool shouldNotify(EventType eventType) const override
    {
        return eventType == EventType::LowStock;
    }

    void update(const std::string &message) override
    {
        std::cout << "Inventory team: " << message << std::endl;
    }
};

class AdminAlertObserver : public Observer
{
public:
    bool shouldNotify(EventType eventType) const override
    {
        return eventType == EventType::PaymentFailed;
    }

    void update(const std::string &message) override
    {
        std::cout << "Admin alert: " << message << std::endl;
    }
};

class AuditLogObserver : public Observer
{
public:
    bool shouldNotify(EventType) const override
    {
        return true;
    }

    void update(const std::string &message) override
    {
        std::cout << "Audit log: " << message << std::endl;
    }
};

int main()
{
    Subject subject;
    CustomerEmailObserver customerEmail;
    InventoryObserver inventory;
    AdminAlertObserver adminAlert;
    AuditLogObserver auditLog;

    subject.attach(&customerEmail);
    subject.attach(&inventory);
    subject.attach(&adminAlert);
    subject.attach(&auditLog);

    subject.notify(EventType::OrderCreated, "Order #101 was created");
    subject.notify(EventType::LowStock, "Product A has only 3 items left");
    subject.notify(EventType::PaymentFailed, "Payment for order #101 failed");
}
