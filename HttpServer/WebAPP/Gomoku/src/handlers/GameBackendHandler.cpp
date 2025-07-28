#include "../../include/handlers/GameBackendHandler.h"


void GameBackendHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    // 展示后台界面
    std::string reqFile("/home/yebidang/workplace/HttpServer/WebAPP/Gomoku/resource/Backend.html");
    FileUtil fileUtils(reqFile);
    if (!fileUtils.isValid())
    {
        LOG_WARN << reqFile << " is not a valid file";
        fileUtils.resetDefaultFile();
    }

    std::vector<char> buffer(fileUtils.size());
    fileUtils.readFile(buffer); // 读取文件内容到buffer
    std::string buf = std::string(buffer.data(), buffer.size());

    // 构建响应返回信息
    resp->setStatusLine(http::HttpResponse::k200Ok, "OK", req.getVersion());
    resp->setContentType("text/html");
    resp->setContentLength(buf.size());
    resp->setBody(buf);
    resp->setCloseConnection(false);

    return;

}