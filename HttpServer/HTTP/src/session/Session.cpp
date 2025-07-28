#include "../../include/session/Session.h"
#include "../../include/session/SessionManager.h"

namespace http
{
namespace session
{
Session::Session(const std::string& sessionId, SessionManager* SessionManager, int maxAge)
                : sessionId_(sessionId), maxAge_(maxAge), sessionManager_(SessionManager)
{
    refresh(); // 初始化设置过期时间
}

bool Session::isExpired() const
{
    return std::chrono::system_clock::now() > expiryTime_;
}

void Session::refresh()
{
    expiryTime_ = std::chrono::system_clock::now() + std::chrono::seconds(maxAge_);
}

void Session::setValue(const std::string& key, const std::string& value)
{
    data_[key] = value;
    if (sessionManager_)
    {
        // update memory
        sessionManager_->updateSessionId(shared_from_this());
    }
}

std::string Session::getValue(const std::string& key) const
{
    auto it = data_.find(key);
    return it != data_.end() ? it->second : std::string(); // 没有kv就是返回空字符
}

void Session::remove(const std::string& key)
{
    auto it = data_.find(key);
    if (it != data_.end())
    {
        data_.erase(it);
    }
}

void Session::clear()
{
    data_.clear();
}
}
}