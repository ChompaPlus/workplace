#include "../../include/session/SessionStorage.h"
#include <iostream>

namespace http
{
namespace session
{
SessionStorage::~SessionStorage() = default;

void MemorySessionStorage::save(std::shared_ptr<Session> session)
{
    sessions_[session->getId()] = session;
}

std::shared_ptr<Session> MemorySessionStorage::load(const std::string& sessionId) 
{
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end())
    {
        if (!it->second->isExpired())
        {

            return it->second;
        }
        else{
            sessions_.erase(it); // 删除过期会话
        }
    }
    return nullptr;
}

void MemorySessionStorage::remove(const std::string& sessionId)
{
    auto it = sessions_.find(sessionId);
    sessions_.erase(it);
}

}
}