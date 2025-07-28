#include "../../include/handlers/AiGameStartHandler.h"

void AiGameStartHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    // 获取会话
    auto session = server_->getSessionManager()->getSession(req,resp);
    if (session->getValue("isLoggedIn") !="true")
    {
        json errorResp;
        errorResp["status"] = "error";
        errorResp["message"] = "Unauthorized";

        // 转换成字符串
        std::string errorRespStr = errorResp.dump(4); // 4：缩进4个空格

        server_->packageResp(req.getVersion(), http::HttpResponse::HttpStatusCode::k400BadRequest
                        , "Unauthorized", true, errorRespStr, "application/json", errorRespStr.size(), resp);
        return;   
    }
    // 获取用户ID
    int userId = std::stoi(session->getValue("userId"));

    // 需要menu页面post发送用户id
    {
        std::lock_guard<std::mutex> lock(server_->mutexForAiGames_);
        if (server_->game_map_.find(userId)!=server_->game_map_.end()) server_->game_map_.erase(userId);
        server_->game_map_[userId] = std::make_shared<AiGame>(userId);// 开始游戏
    }

    // 开始游戏，执行人机对战，直到有一方获胜或者平局
    std::string reqFile("/home/yebidang/workplace/HttpServer/WebAPP/Gomoku/resource/ChessGameVsAi.html");
    FileUtil fileUtils(reqFile);
    if (!fileUtils.isValid())
    {
        LOG_WARN << "文件不存在:" << reqFile;
        fileUtils.resetDefaultFile();
    }
    std::vector<char> buffer(fileUtils.size());
    fileUtils.readFile(buffer);
    std::string htmlContent(buffer.data(), buffer.size());
    // 构建响应返回成功信息
    resp->setStatusLine(http::HttpResponse::k200Ok, "OK",req.getVersion());
    resp->setCloseConnection(false);
    resp->setContentType("text/html");
    resp->setContentLength(htmlContent.size());
    resp->setBody(htmlContent);

}