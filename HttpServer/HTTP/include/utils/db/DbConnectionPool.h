#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <thread>
#include "DbConnection.h"

namespace http
{
namespace db
{
class DbConnectionPool
{
public:
    // 单例模式
    static DbConnectionPool& getInstance()
    {
        static DbConnectionPool instance;
        return instance;
    };

    // 初始化连接池
    void init(const std::string& host,
                const std::string& user,
                const std::string& password,
                const std::string& database,
                size_t poolsize = 10);

    // 从池里获取连接
    // 检查有无空闲连接
    // 无空闲连接检查最大连接数
    // 没超过最大连接数创建一个新连接
    // 否则告诉线程等待
    std::shared_ptr<DbConnection> getConnection();

private:
    DbConnectionPool();
    ~DbConnectionPool();

    // 禁止拷贝
    DbConnectionPool(const DbConnectionPool&) = delete;
    DbConnectionPool& operator=(const DbConnectionPool&) = delete;

    std::shared_ptr<DbConnection> createConnection();

    void checkConnections();// 连接检查方法

private:
    std::string         host_;
    std::string         user_;
    std::string         password_;
    std::string         database_;
    std::queue<std::shared_ptr<DbConnection>> connections_;

    std::mutex          mutex_;
    std::condition_variable cv_;
    bool                initialized_ = false;
    std::thread         checkThread_;
};
}
}
