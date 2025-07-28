#pragma once
#include <stdexcept> // 提供异常类，用于处理数据库中的操作错误
#include <string>


// 封装和传递数据库操作中发生的错误的具体信息
namespace http
{
namespace db
{
class DbException : public std::runtime_error
{
public:
    explicit DbException(const std::string& message): std::runtime_error(message) {}
    explicit DbException(const char* message) : std::runtime_error(message) {}
};
}
}