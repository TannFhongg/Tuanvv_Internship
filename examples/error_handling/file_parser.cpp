/*
exception handling FileParser ném ParseError có dòng/cột/lý do khi input sai.

-Phân loại lỗi chuẩn hóa
-Truyền tải thông điệp lỗi
-An toàn khi bắt exception ( khong bi Object slicing)
*/

#include <cstddef>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>


class ParseError : public std::runtime_error
{
private:
    std::size_t line_;
    std::size_t column_;
    std::string reason_;

    static std::string createMessage(
        std::size_t line,
        std::size_t column,
        const std::string &reason)
    {
        return "Parse errror at line " + std::to_string(line) + "column" + std::to_string(column) + ": " + (reason);
    }

public:
    ParseError(
        std::size_t line,
        std::size_t column,
        const std::string &reason) : std::runtime_error(createMessage(line, column, reason)), line_(line), column_(column), reason_(reason) {}

    std::size_t line() const noexcept
    {
        return line_;
    }
    std::size_t comlumn() const noexcept
    {
        return column_;
    }

    const std::string &reason() const noexcept
    {
        return reason_;
    }
};

class FileParser
{

public:
    using Config = std::unordered_map<std::string, std::string>;

private:
    static std::string trim(const std::string &text)
    {
        const std::size_t begin = text.find_first_not_of(" \t\r");

        if (begin == std::string ::npos)
        {
            return {};
        }
        const std::size_t end = text.find_last_not_of(" \t\r");

        return text.substr(begin, end - begin + 1);
    }

public:
    Config parse(const std::string &filePath) const
    {
        std::ifstream input(filePath);

        if (!input)
        {
            throw std::runtime_error(
                "Cannot open file: " + filePath);
        }

        Config config;
        std::string sourceLine;
        std::size_t lineNumber = 0;

        while ((std::getline(input, sourceLine)))
        {
            /* code */
            ++lineNumber;

            const std::size_t firstCharacter =
                sourceLine.find_first_not_of(" \t\r");

            // bo qua dong trong va comment
            if (firstCharacter == std::string::npos || sourceLine[firstCharacter] == '#')
            {
                continue;
            }

            const std::size_t equalPosition = sourceLine.find('=');

            if (equalPosition == std::string::npos)
            {
                throw ParseError(
                    lineNumber,
                    sourceLine.size() + 1,
                    "missing '=' ");
            }

            const std::string key = trim(sourceLine.substr(0, equalPosition));
            const std::string value = trim(sourceLine.substr(equalPosition + 1));

            if (key.empty())
            {
                throw ParseError(
                    lineNumber,
                    equalPosition + 1,
                    "key cannot be empty");
            }

            if (value.empty())
            {

                throw ParseError(
                    lineNumber,
                    equalPosition + 2,
                    "Value can not empty");
            }

            if (config.find(key) != config.end())
            {
                throw ParseError(
                    lineNumber,
                    firstCharacter + 1,
                    "duplicate key : " + key);
            }
            config.emplace(key,value); 
        }
        return config; 
    }
};

int main() { 
    try {
        FileParser parser ; 
        const FileParser::Config config = parser.parse("config.txt"); 

        for(const auto& [key,value] : config ) { 
            std::cout << key << "=" << value<< std::endl; 

        }
    }
    catch (const ParseError& error) {
        std::cerr << "Invalid input \n"; 
        std::cerr << "Line" << error.line() << std::endl; 
        std::cerr<<"Column : " << error.comlumn() << std::endl; 
        std::cerr<< "reason: " << error.reason() << std::endl; 
        std::cerr<< "what()" << error.what() << std::endl; 
        return 1; 
    }

    catch (const std::exception& error) { 
          std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

}