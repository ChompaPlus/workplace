#include "../../include/session/SessionManager.h"
#include <iomanip>
#include <iostream>
#include <sstream>

namespace http
{
namespace session
{
SessionManager::SessionManager(std::unique_ptr<SessionStorage> storage) 
                        : storage_(std::move(storage))
                        , rng_(std::random_device{}())
{}

std::shared_ptr<Session> SessionManager::getSession(const HttpRequest& req, HttpResponse* resp)
{
    // 有一个过程就是要先判断会话是否是已经发生过
    // 没有发生过自然是新创建一个会话

    // 从request_的请求头中的cookie字段获得sessionId
    std::string sessionId = getSessionIdFromCookie(req);
    std::shared_ptr<Session> session;

    // 从内存中找到属于的会话
    if (!sessionId.empty())
    {
        session = storage_->load(sessionId);
    }
    // 创建一个新会话
    if (!session || session->isExpired())
    {
        sessionId = generateSessionId();// 
        session = std::make_shared<Session>(sessionId, this); // 创建一个session对象
        setSessionCookie(sessionId, resp);// 将标识符构建到响应中准备发送给客户端
    }
    else // 否则为现有会话设置管理器
    {
        session->setManager(this);
    }
    session->refresh();// 又开始使用该会话，过期时间延迟/创建了一个新的会话计算过期时间
    storage_->save(session); // 保存到内存中
    return session;
}

std::string SessionManager::generateSessionId()
{
    std::stringstream ss;
    std::uniform_int_distribution<> dist(0, 15);

    // 生成32个字符的会话id，每个字符是一个十六进制数字
    for (int i = 1; i  < 32; ++i)
    {
        ss << std::hex << dist(rng_);
    }
    return ss.str();
}

void SessionManager::destroySession(const std::string& sessionId)
{
    storage_->remove(sessionId);
}

void SessionManager::cleanExpiredSessions()
{
    // 对于内存存储的会话，可以在从内存中加载会话时检查是否过期
    // 其他存储的实现视情况而定，可能需要定时清理会话
    // 总的来说，这个实现依赖于存储的实现
}

std::string SessionManager::getSessionIdFromCookie(const HttpRequest& req)
{
    std::string sessionId;
    std::string cookie = req.getHeader("Cookie");
    if (!cookie.empty())
    {
        size_t pos = cookie.find("sessionId=");
        if (pos != std::string::npos)
        {
            pos += 10;
            size_t end = cookie.find(';', pos);
            if (end != std::string::npos)
            {
                sessionId = cookie.substr(pos, end-pos);
            }
            else
            {
                sessionId = cookie.substr(pos);
            }
        }
    }
    return sessionId;
}

void SessionManager::setSessionCookie(const std::string& sessionId, HttpResponse* resp)
{
    std::string cookie = "sessionId=" + sessionId + "; Path=/; HttpOnly";
    resp->addHeader("Set-Cookie", cookie);
}


}
}
