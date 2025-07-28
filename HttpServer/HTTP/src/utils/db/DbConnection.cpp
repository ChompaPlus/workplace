// #include "../../../include/utils/db/DbConnection.h"
// #include "../../../include/utils/db/DbException.h"

// #include <muduo/base/Logging.h>


// namespace http
// {
// namespace db
// {


// db::DbConnection::DbConnection(const std::string &host, const std::string &user, const std::string &password, const std::string &database)
//             : host_(host), user_(user), 
//                 password_(password), database_(database) 
// {
//     try
//     {
//         // 这里获得了一个数据库操作接口的实例(单例)
//         // 通过该实例可以执行对数据库的操作
//         // conn_是连接对象
//         sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
//         conn_.reset(driver->connect(host_, user_, password_)); // 重新建立连接
//         if (conn_)
//         {
//             conn_->setSchema(database_); //选择要操作的数据库

//             // 设置连接属性
//             conn_->setClientOption("OPT_RECONNECT", "true");// 启用自动重连（由网络中断等因素引起的断联）
//             conn_->setClientOption("OPT_CONNECT_TIMEOUT", "10"); // 10s没有建立连接就超时
//             conn_->setClientOption("muti_statements", "false");// 禁用多语句执行

//             /* 设置字符编码 */
//             std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
//             stmt->execute("SET NAMES utf8mb4"); // 支持中文等特殊字符

//             LOG_INFO << "Database connection established";
//         }
//     }
//     catch(const sql::SQLException& e)
//     {
//         LOG_ERROR << "Failed to create database connection:" << e.what();
//         throw DbException(e.what());
//     }
    
// }

// DbConnection::~DbConnection()
// {
//     try
//     {
//         cleanup();
//     }
//     catch(...)
//     {
//         /*析构函数不抛出异常*/
//     }
//     LOG_INFO << "Database connection closed";
// }

// bool DbConnection::ping() // 检查连接状态
// {
//     try
//     {
//         // 不使用getStmt，直接创建新语句
//         std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
//         std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery("SELECT 1"));
//         return true;
//     }
//     catch(const sql::SQLException& e)
//     {
//         LOG_ERROR << "Ping failed:" << e.what();
//         return false;
//     }    
// }

// bool DbConnection::isValid() // 检查连接是否有效
// {
//     try
//     {
//         if (!conn_) return false;
//         std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
//         stmt->execute("SELECT 1");
//         return true;
//     }
//     catch(const sql::SQLException&)
//     {
//         return false;
//     }
    
// }

// void DbConnection::reconnect()
// {
//     try
//     {
//         if (conn_)
//         {
//             conn_->reconnect();
//         }
//         else
//         {
//             // 获取操作接口的实例
//             sql::mysql::MySQL_Driver* driver = sql::mysql::get_driver_instance();
//             // 建立连接
//             conn_.reset(driver->connect(host_, user_, password_));
//             // 选择要操作的数据库
//             conn_->setSchema(database_);
//         }
//     }
//     catch(const sql::SQLException& e)
//     {
//         LOG_ERROR << "Reconnect failed:" << e.what();
//         throw DbException(e.what());
//     }
    
// }

// void db::DbConnection::cleanup()
// {
//     std::lock_guard<std::mutex> lock(mutex_);
//     try
//     {
//         if (conn_)
//         {
            
//             if (!conn_->getAutoCommit())
//             {
//                 // 有事务未完成
//                 conn_->rollback();// 把进行的事务都回滚并停止
//                 conn_->setAutoCommit(true);// 设置标识位，所有事务都完成
//             }
//             std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
//             while (stmt->getMoreResults())
//             {
//                 auto result = stmt->getResultSet();// 
//                 while (result && result->next())
//                 {
//                     // 消费所有结果
//                 }
//             }
//         }
//     }
//     catch(const sql::SQLException& e)
//     {
//         LOG_WARN << "Error cleaning up connection:" << e.what();
//         try
//         {
//             reconnect();
//         }
//         catch(...)
//         {
//             // 忽略重连错误
//         }
        
//     }
    
// }

// }
// }
#include "../../../include/utils/db/DbConnection.h"
#include "../../../include/utils/db/DbException.h"
#include <muduo/base/Logging.h>

namespace http 
{
namespace db 
{

DbConnection::DbConnection(const std::string& host,
                         const std::string& user,
                         const std::string& password,
                         const std::string& database)
    : host_(host)
    , user_(user)
    , password_(password)
    , database_(database)
{
    try 
    {
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        conn_.reset(driver->connect(host_, user_, password_));
        if (conn_) 
        {
            conn_->setSchema(database_);
            
            // 设置连接属性
            conn_->setClientOption("OPT_RECONNECT", "true");
            conn_->setClientOption("OPT_CONNECT_TIMEOUT", "10");
            conn_->setClientOption("multi_statements", "false");
            
            // 设置字符集
            std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
            stmt->execute("SET NAMES utf8mb4");
            
            LOG_INFO << "Database connection established";
        }
    } 
    catch (const sql::SQLException& e) 
    {
        LOG_ERROR << "Failed to create database connection: " << e.what();
        throw DbException(e.what());
    }
}

DbConnection::~DbConnection() 
{
    try 
    {
        cleanup();
    } 
    catch (...) 
    {
        // 析构函数中不抛出异常
    }
    LOG_INFO << "Database connection closed";
}

bool DbConnection::ping() 
{
    try 
    {
        // 不使用 getStmt，直接创建新的语句
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery("SELECT 1"));
        return true;
    } 
    catch (const sql::SQLException& e) 
    {
        LOG_ERROR << "Ping failed: " << e.what();
        return false;
    }
}

bool DbConnection::isValid() 
{
    try 
    {
        if (!conn_) return false;
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        stmt->execute("SELECT 1");
        return true;
    } 
    catch (const sql::SQLException&) 
    {
        return false;
    }
}

void DbConnection::reconnect() 
{
    try 
    {
        if (conn_) 
        {
            conn_->reconnect();
        } 
        else 
        {
            sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
            conn_.reset(driver->connect(host_, user_, password_));
            conn_->setSchema(database_);
        }
    } 
    catch (const sql::SQLException& e) 
    {
        LOG_ERROR << "Reconnect failed: " << e.what();
        throw DbException(e.what());
    }
}

void DbConnection::cleanup() 
{
    std::lock_guard<std::mutex> lock(mutex_);
    try 
    {
        if (conn_) 
        {
            // 确保所有事务都已完成
            if (!conn_->getAutoCommit()) 
            {
                conn_->rollback();
                conn_->setAutoCommit(true);
            }
            
            // 清理所有未处理的结果集
            std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
            while (stmt->getMoreResults()) 
            {
                auto result = stmt->getResultSet();
                while (result && result->next()) 
                {
                    // 消费所有结果
                }
            }
        }
    } 
    catch (const std::exception& e) 
    {
        LOG_WARN << "Error cleaning up connection: " << e.what();
        try 
        {
            reconnect();
        } 
        catch (...) 
        {
            // 忽略重连错误
        }
    }
}

} // namespace db
} // namespace http
