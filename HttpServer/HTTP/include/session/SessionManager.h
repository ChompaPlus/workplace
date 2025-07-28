#pragma once

#include "SessionStorage.h"
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"
#include <memory>
#include <random> // 利用随机数生成器

namespace http
{
namespace session
{
class SessionManager
{
public:
    explicit SessionManager(std::unique_ptr<SessionStorage> storage);

    // 从请求中获取或者创建会话
    std::shared_ptr<Session> getSession(const HttpRequest& req, HttpResponse* resp);

    // 销毁会话
    void destroySession(const std::string& SessionId);
    // 清理过期会话
    void cleanExpiredSessions();
    // 更新会话
    void updateSessionId(std::shared_ptr<Session> session)
    {
        storage_->save(session);
    }

private:
    std::string generateSessionId();
    std::string getSessionIdFromCookie(const HttpRequest& req);
    void setSessionCookie(const std::string& sessionId, HttpResponse* resq);

private:
    std::mt19937 rng_;
    std::unique_ptr<SessionStorage> storage_;
};
}
}