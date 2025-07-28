#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <chrono>


namespace http
{
namespace session
{

class SessionManager;

class Session : public std::enable_shared_from_this<Session>
{
private:
    std::string  sessionId_; // 标识符，由manager生成
    std::unordered_map<std::string, std::string> data_; // 以k-v的形式存该session对象的一切内容，sessionid,过期时间
    std::chrono::system_clock::time_point expiryTime_;
    int maxAge_; //过期时间，这是默认设置的，在构造session对象时会从那时的系统时间计算绝对时间
    SessionManager* sessionManager_;

public:
    Session(const std::string& sessionId, SessionManager* sessionManager, int maxAge = 3600);

    const std::string& getId() const {return sessionId_;}
    
    // 会话管理
    bool isExpired() const;
    void refresh(); // ExpiredTime

    // 关联管理器，通过管理器才能和内存建立关系
    void setManager(SessionManager* SessionManager) { sessionManager_ = SessionManager; }
    SessionManager* getManager() const { return sessionManager_; }

    // 操作自身属性的一些方法，修改属性需要更新内存（缓存）
    void setValue(const std::string& key, const std::string& value);
    std::string getValue(const std::string& key) const;
    void remove(const std::string& key);

    void clear();

};

}
}