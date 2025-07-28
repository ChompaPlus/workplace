// #include "../../../include/utils/db/DbConnectionPool.h"
// #include "../../../include/utils/db/DbException.h"
// #include <muduo/base/Logging.h>

// namespace http
// {
// namespace db
// {
// void DbConnectionPool::init(const std::string& host,
//                                 const std::string& user,
//                                 const std::string& password,
//                                 const std::string& database,
//                                 size_t poolsize)
// {
//     LOG_INFO << "DbConnectionPool::init() begin";
//     std::lock_guard<std::mutex> lock(mutex_);
//     // 确保初始化一次，保证一次连接的有效性
//     if (initialized_)
//     {
//         return;
//     }
//     host_ = host;
//     user_ = user;
//     password_ = password;
//     database_ = database;

//     size_t validConnections = 0;
//     for (size_t i = 0; i < poolsize; ++i)
//     {
//         connections_.push(createConnection());
//     }


//     initialized_ = true;
//     LOG_INFO << "Databse connection pool initialized with" << poolsize << " connections";
//     // // 新增：初始化完成后启动检查线程
//     // checkThread_ = std::thread(&DbConnectionPool::checkConnections, this);
//     // checkThread_.detach();
// }

// DbConnectionPool::DbConnectionPool()
// {
//     // // 后台启动一个线程，周期性的检查池中的连接状态
//     checkThread_ = std::thread(&DbConnectionPool::checkConnections, this);
//     checkThread_.detach();// 后台自主运行，不阻塞主线程，主线程结束也不影响他的运行
// }

// DbConnectionPool::~DbConnectionPool()
// {
//     std::lock_guard<std::mutex> lock(mutex_);
//     while (!connections_.empty())
//     {
//         connections_.pop();
//     }
//     LOG_INFO << "Database connection pool destroyed";
// }

// // 某个线程来获取连接
// // 过程就是检查是否有空闲连接
// // 没有就等着
// // 拿到连接释放锁，然后检查连接的状态
// // 连接出问题重新建立连接
// // 然后返回连接，返回的是一个智能指针管理的对象，所以要冲定义释放的方法，因为不能销毁对象而是复用
// std::shared_ptr<DbConnection> DbConnectionPool::getConnection()
// {
//     std::shared_ptr<DbConnection> conn;
//     {
//         // 进入连接池要上锁，用分布式锁，保护临界区，但是避免整段代码都上锁
//         std::unique_lock<std::mutex> lock(mutex_);
//         while (connections_.empty())
//         {
//             if (!initialized_)
//             {
//                 throw DbException("Connection pool not initialized");
//             }
//             LOG_INFO << "WAITING FOR AVAILABLE CONNECTION...";
//             cv_.wait(lock); // 因为没有可用连接所以阻塞在这里等着唤醒
//         }
//         conn = connections_.front();
//         connections_.pop();
//     }
//     // 拿到连接后就退出池，不妨碍其他线程申请连接
//     // 在锁外检查连接
//     try
//     {
//         if (!conn->ping())
//         {
//             conn->reconnect();
//         }
//         // 这是重新定义了智能指针对象的析构方法，就是在结束引用时，把该对象放回池里，并唤醒一个可能在阻塞申请的线程
//         return std::shared_ptr<DbConnection>(conn.get(), [this, conn](DbConnection*){
//             std::lock_guard<std::mutex> lock(mutex_);
//             connections_.push(conn);
//             cv_.notify_one();
//         });
//     }
//     catch(const sql::SQLException& e)
//     {
//         LOG_ERROR << "failed to get conn:" << e.what();
//         {
//             std::lock_guard<std::mutex> lock(mutex_);
//             connections_.push(conn);
//             cv_.notify_one();
//         }
//         throw;
//     }
// }

// std::shared_ptr<DbConnection> DbConnectionPool::createConnection()
// {
//     return std::make_shared<DbConnection>(host_, user_, password_, database_);
// }

// // 修改检查连接的函数
// // 检查空闲连接
// void DbConnectionPool::checkConnections()
// {
//     while (true)
//     {
//         try
//         {
//             std::vector<std::shared_ptr<DbConnection>> connCheck;// 存空闲连接
//             {
//                 std::unique_lock<std::mutex> lock(mutex_); // 拿空闲连接时上锁，其他访问的线程阻塞
//                 if (connections_.empty())
//                 {
//                     std::this_thread::sleep_for(std::chrono::seconds(1));
//                     continue;
//                 }
            
//                 auto temp = connections_; // 拷贝不是引用，但是成员是智能指针，引用的还是原连接对象只不过引用计数增加
//                 while (!temp.empty())
//                 {
//                     connCheck.push_back(temp.front());
//                     temp.pop();
//                 }
//             }
//             // 释放锁
//             // 检查连接
//             for (auto& conn: connCheck)
//             {
//                 if (!conn->ping())
//                 {
//                     // 连接断开，尝试重连
//                     try
//                     {
//                         conn->reconnect(); //
//                     }
//                     catch(const sql::SQLException& e)
//                     {
//                         LOG_ERROR << "failed to reconnect:" << e.what();
//                     }
                    
//                 }
//             }
//             std::this_thread::sleep_for(std::chrono::seconds(60)); //避免后台线程频繁的霸占池导致，连接效率降低
//         }
//         catch(const std::exception& e)
//         {
//             LOG_ERROR << "ERROR IN CHECK THREAD:" << e.what();
//             std::this_thread::sleep_for(std::chrono::seconds(5));
//         }
        
//     }
// }

// } // namespace db

// } // namespace http
#include "../../../include/utils/db/DbConnectionPool.h"
#include "../../../include/utils/db/DbException.h"
#include <muduo/base/Logging.h>

namespace http 
{
namespace db 
{

void DbConnectionPool::init(const std::string& host,
                          const std::string& user,
                          const std::string& password,
                          const std::string& database,
                          size_t poolSize) 
{
    // 连接池会被多个线程访问，所以操作其成员变量时需要加锁
    std::lock_guard<std::mutex> lock(mutex_);
    // 确保只初始化一次
    if (initialized_) 
    {
        return;
    }

    host_ = host;
    user_ = user;
    password_ = password;
    database_ = database;

    // 创建连接
    for (size_t i = 0; i < poolSize; ++i) 
    {
        connections_.push(createConnection());
    }

    initialized_ = true;
    LOG_INFO << "Database connection pool initialized with " << poolSize << " connections";
    // 仅在初始化完成后启动一次检查线程（关键修复）
    checkThread_ = std::thread(&DbConnectionPool::checkConnections, this);
    checkThread_.detach();
}

DbConnectionPool::DbConnectionPool() 
{
    // checkThread_ = std::thread(&DbConnectionPool::checkConnections, this);
    // checkThread_.detach();
}

DbConnectionPool::~DbConnectionPool() 
{
    std::lock_guard<std::mutex> lock(mutex_);
    while (!connections_.empty()) 
    {
        connections_.pop();
    }
    LOG_INFO << "Database connection pool destroyed";
}

// 修改获取连接的函数
std::shared_ptr<DbConnection> DbConnectionPool::getConnection() 
{
    std::shared_ptr<DbConnection> conn;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        
        while (connections_.empty()) 
        {
            if (!initialized_) 
            {
                throw DbException("Connection pool not initialized");
            }
            LOG_INFO << "Waiting for available connection...";
            cv_.wait(lock);
        }
        
        conn = connections_.front();
        connections_.pop();
    } // 释放锁
    
    try 
    {
        // 在锁外检查连接
        if (!conn->ping()) 
        {
            LOG_WARN << "Connection lost, attempting to reconnect...";
            conn->reconnect();
        }
        
        return std::shared_ptr<DbConnection>(conn.get(), 
            [this, conn](DbConnection*) {
                std::lock_guard<std::mutex> lock(mutex_);
                connections_.push(conn);
                cv_.notify_one();
            });
    } 
    catch (const std::exception& e) 
    {
        LOG_ERROR << "Failed to get connection: " << e.what();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            connections_.push(conn);
            cv_.notify_one();
        }
        throw;
    }
}

std::shared_ptr<DbConnection> DbConnectionPool::createConnection() 
{
    return std::make_shared<DbConnection>(host_, user_, password_, database_);
}

// 修改检查连接的函数
void DbConnectionPool::checkConnections() 
{
    while (true) 
    {
        try 
        {
            std::vector<std::shared_ptr<DbConnection>> connsToCheck;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (connections_.empty()) 
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
                
                auto temp = connections_;
                while (!temp.empty()) 
                {
                    connsToCheck.push_back(temp.front());
                    temp.pop();
                }
            }
            
            // 在锁外检查连接
            for (auto& conn : connsToCheck) 
            {
                if (!conn->ping()) 
                {
                    try 
                    {
                        conn->reconnect();
                    } 
                    catch (const std::exception& e) 
                    {
                        LOG_ERROR << "Failed to reconnect: " << e.what();
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(60));
        } 
        catch (const std::exception& e) 
        {
            LOG_ERROR << "Error in check thread: " << e.what();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

} // namespace db
} // namespace http