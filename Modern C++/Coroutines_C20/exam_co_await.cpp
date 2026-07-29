#include <generator>
#include <iostream>

Task<std::string> fecth_data(std::string url)
{
    auto response = co_await http_get(url);
    auto body = co_await response.read_body();

    co_return body;
}

Task<void> main_task()
{
    std::string data = co_await fecth_data("http://example.com");

    std::cout << data;
}