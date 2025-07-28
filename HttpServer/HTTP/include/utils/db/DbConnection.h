#pragma once
#include <memory>
#include <string>
#include <mutex>
#include <cppconn/prepared_statement.h> // 定义预处理语句类，用于执行参数化SQL查询（防止SQL注入）
#include <cppconn/resultset.h> // 结果集，用于处理SQL查询返回的数据
#include <cppconn/connection.h> // 数据库连接类
#include <mysql_driver.h> // 用于创建数据库连接
#include <mysql/mysql.h> // 提供底层数据库操作接口（提供以上操作的接口）
#include <muduo/base/Logging.h>
#include "DbException.h"

namespace http
{
namespace db
{
class DbConnection
{
private:
    /* data */
    std::mutex mutex_;
    std::shared_ptr<sql::Connection> conn_;

    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;

public:

    // mysql -h ip -u user -t password
    DbConnection(const std::string& host, 
                const std::string& user,
                const std::string& password,
                const std::string& database);
    ~DbConnection();
    
    // 禁止拷贝
    DbConnection(const DbConnection&) = delete;
    DbConnection& operator=(const DbConnection&) = delete;
    
    bool isValid();
    void reconnect();
    void cleanup();

    // sql为查询模板包含占位符？
    // 后面为具体的查询参数
    template<typename... Args>
    sql::ResultSet* executeQuery(const std::string& sql, Args&&... args)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        try // 尝试执行可能出现异常的代码段
        {
            // 直接创建新的预处理语句，不使用缓存
            // 预处理语句允许开发者预先定义SQL的查询结构（模板），并且动态绑定参数
            // 数据库可以预编译SQL模板，重复执行时无需解析
            // 简化参数查询（避免手动拼接字符串）
            std::unique_ptr<sql::PreparedStatement> stmt(
                conn_->prepareStatement(sql)// 创建预处理语句
            );
            bindParams(stmt.get(), 1, std::forward<Args>(args)...);// 实现完美转发（左值或者右值）
            return stmt->executeQuery();
        }
        catch (const sql::SQLException& e) // 捕获出现的异常
        {
            LOG_ERROR << "Query faild:" << e.what() << ", SQL:" << sql;
            throw DbException(e.what());
        }
    }

    // 
    template<typename... Args>
    int executeUpdate(const std::string& sql, Args&&... args)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        try
        {
            std::unique_ptr<sql::PreparedStatement> stmt(
                conn_->prepareStatement(sql) // 创建更新的预处理语句
            );
            bindParams(stmt.get(), 1, std::forward<Args>(args)...);
            return stmt->executeUpdate();
        }
        catch(const sql::SQLException& e)
        {
            LOG_ERROR << "Update faild:" << e.what() << ", SQL:" << sql;
            throw DbException(e.what());
        }
        
    }
    bool ping();// 测试连接状态
private:
    // stmt- 预处理语句
    // index- 占位符“？”的位置，约定是从1开始
    // value可能是主键值，比如id = 25

    // 辅助函数，递归终止条件
    // 具体的过程就是当参数被绑定完了，执行一个空的函数体然后结束
    void bindParams(sql::PreparedStatement*, int) {}

    // 辅助函数：绑定参数
    template<typename T, typename... Args>
    void bindParams(sql::PreparedStatement* stmt, int index, T&& value, Args&&... args)
    {
        stmt->setString(index, std::to_string(std::forward<T>(value)));
        bindParams(stmt, index + 1, std::forward<Args>(args)...);// 递归绑定参数
    }
    // 特化string的类型参数
    template<typename... Args>
    void bindParams(sql::PreparedStatement* stmt, int index, const std::string& value, Args... args)
    {
        stmt->setString(index, value);
        bindParams(stmt, index + 1, std::forward<Args>(args)...);// 递归
    }

};


} // namespace db

}
