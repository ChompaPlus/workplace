#include "../../include/handlers/AiMoveHandler.h"
#include <muduo/base/Logging.h>
// ai下棋
void AiMoveHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp) 
{
    try
    {
        // 获取会话
        auto session = server_->getSessionManager()->getSession(req, resp);
        // 检查是否登录
        if (session->getValue("isLoggedIn") != "true")
        {
            json errResp;
            errResp["status"] = "error";
            errResp["message"] = "Unauthorized";
            std::string errorBody = errResp.dump(4); // 将json对象转换为字符串
            // 构建响应返回错误信息，未登录
            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized
                        , "Unauthorized", true, "application/json", errorBody, errorBody.length(), resp);
            return;
        }
        LOG_INFO << "开始处理移动";
        int userId = std::stoi(session->getValue("userId"));
        // 解析请求体
        json request = json::parse(req.getBody());
        // 检查请求体是否包含x和y坐标

        int x = request["x"];
        int y = request["y"];

        if (server_->game_map_.find(userId) == server_->game_map_.end())
        {
            std::lock_guard<std::mutex> lock(server_->mutexForAiGames_);
            server_->game_map_[userId] = std::make_shared<AiGame>(userId);
        }
        auto &game = server_->game_map_[userId];
        // 处理用户的移动
        if (!game->humanMove(x, y))
        {
            LOG_ERROR << "用户移动失败";
            json errResp =
            {
                {"status", "error"},
                {"message", "Invalid move"}
            };
            std::string errorBody = errResp.dump(); // 将json对象转换为字符串
            // 构建响应返回错误信息，无效的移动
            resp->setStatusLine(http::HttpResponse::k400BadRequest, "Bad Request",req.getVersion());
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(errorBody.size());
            resp->setBody(errorBody);
            return;
        }
        LOG_INFO << "用户移动成功";

        // 检查游戏是否结束
        if (game->isGameOver())
        {
            json respBody =
            {
                {"status", "ok"},
                {"board", game->getBoard()},
                {"winner", game->getWinner()},
                {"next_turn", "none"}
            };
            std::string respBodyStr = respBody.dump(); // 将json对象转换为字符串
            // 构建响应返回成功信息
            resp->setStatusLine(http::HttpResponse::k200Ok, "OK",req.getVersion());
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(respBodyStr.size());
            resp->setBody(respBodyStr);
            {
                std::lock_guard<std::mutex> lock(server_->mutexForAiGames_);
                server_->game_map_.erase(userId);// 反正每次都要创建userid
            }
            return;
        }

        // 检查平局
        if (game->isDraw())
        {
            json respBody =
            {
                {"status", "ok"},
                {"board", game->getBoard()},
                {"winner", "draw"},
                {"next_turn", "none"}
                
            };
            std::string respBodyStr = respBody.dump(); // 将json对象转换为字符串
            // 构建响应返回成功信息

            resp->setStatusLine(http::HttpResponse::k200Ok, "OK",req.getVersion());
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(respBodyStr.size());
            resp->setBody(respBodyStr);
            {
                std::lock_guard<std::mutex> lock(server_->mutexForAiGames_);
                server_->game_map_.erase(userId);// 反正每次都要创建userid
            }
            return;
        }
        // AI移动
        LOG_INFO << "AI开始移动";
        game->aiMove();
        LOG_INFO << "ai移动成功";
        // 检查游戏是否结束
        if (game->isGameOver())
        {
            json respBody =
            {
                {"status", "ok"},
                {"board", game->getBoard()},
                {"winner", game->getWinner()},
                {"next_turn", "none"},
                {"last_move", {{"x", game->getLastMove().first}, {"y", game->getLastMove().second}}}
            };
            std::string respBodyStr = respBody.dump(); // 将json对象转换为字符串
            // 构建响应返回成功信息
            resp->setStatusLine(http::HttpResponse::k200Ok, "OK",req.getVersion());
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(respBodyStr.size());
            resp->setBody(respBodyStr);    
            {
                std::lock_guard<std::mutex> lock(server_->mutexForAiGames_);
                server_->game_map_.erase(userId);// 反正每次都要创建userid
            }

            return;

        }
        // 检查平局
        if (game->isDraw())
        {
            json respBody =
            {
                {"status", "ok"},
                {"board", game->getBoard()},
                {"winner", "draw"},
                {"next_turn", "none"},
                {"last_move", {{"x", game->getLastMove().first}, {"y", game->getLastMove().second}}}
            };
            std::string respBodyStr = respBody.dump(); // 将json对象转换为字符串
            // 构建响应返回成功信息
            resp->setStatusLine(http::HttpResponse::k200Ok, "OK",req.getVersion());
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(respBodyStr.size());
            resp->setBody(respBodyStr);
            {
                std::lock_guard<std::mutex> lock(server_->mutexForAiGames_);
                server_->game_map_.erase(userId);// 反正每次都要创建userid
            }
            return;
        }


        // 游戏继续
        LOG_INFO << "游戏继续";
        json respBody =
        {
            {"status", "ok"},
            {"board", game->getBoard()},
            {"winner", "none"},
            {"next_turn", "human"},
            {"last_move", {{"x", game->getLastMove().first}, {"y", game->getLastMove().second}}}
        };
        std::string respBodyStr = respBody.dump(); // 将json对象转换为字符串
        // 构建响应返回成功信息
        resp->setStatusLine(http::HttpResponse::k200Ok, "OK",req.getVersion());
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(respBodyStr.size());
        resp->setBody(respBodyStr);
    }
    catch(const std::exception& e)
    {
        LOG_ERROR << "ai移动失败:" << e.what();
        json errResp =
        {
            {"status", "error"},
            {"message", e.what()}
        };
        std::string errorBody = errResp.dump(); // 将json对象转换为字符串
        // 构建响应返回错误信息
        server_->packageResp(req.getVersion(), http::HttpResponse::k500InternalServerError
                   , "Internal Server Error", false, "application/json", errorBody, errorBody.length(), resp);

    }
}