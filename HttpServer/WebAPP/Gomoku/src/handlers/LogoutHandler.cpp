#include "../../include/handlers/LogoutHandler.h"

void LogoutHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    auto contentType = req.getHeader("Content-Type");
    if (contentType != "application/json" || contentType.empty()|| req.getBody().empty())
    {
        resp->setStatusLine(http::HttpResponse::k400BadRequest, "Bad Request", req.getVersion());
        resp->setCloseConnection(true); 
        resp->setContentType("application/json");
        resp->setContentLength(0);
        resp->setBody("");
        return;
    }

    try
    {
        // 获取会话
        auto session = server_->getSessionManager()->getSession(req, resp);
        int userId = std::stoi(session->getValue("userId"));
        session->clear(); // 清除的内容不包含sessionid
        server_->getSessionManager()->destroySession(session->getId());
        
        json parsed = json::parse(req.getBody());
        int gameType = parsed["gameType"];
        {
            std::lock_guard<std::mutex> lock(server_->mutexForOnlineUsers_);
            server_->online_users.erase(userId);

        }
        if (gameType == GomokuServer::GameType::MAN_VS_AI)
        {
            std::lock_guard<std::mutex> lock(server_->mutexForAiGames_);
            server_->game_map_.erase(userId);
        }
        else if (gameType == GomokuServer::GameType::MAN_VS_MAN)
        {
            // 这个功能还没有实现
            // 实现后释放响应的资源就好
        }

        json response;
        response["message"] = "Logout successful";
        std::string responseBody = response.dump(4);

        resp->setStatusLine(http::HttpResponse::k200Ok, "OK", req.getVersion());
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(responseBody.size());
        resp->setBody(responseBody);

    }
    catch(const std::exception& e)
    {
        json response;
        response["status"] = "error";
        response["message"] = e.what();
        std::string responseBody = response.dump(4);
        resp->setStatusLine(http::HttpResponse::k500InternalServerError, "Internal Server Error", req.getVersion());
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(responseBody.size());
        resp->setBody(responseBody);
    }
    
   
}