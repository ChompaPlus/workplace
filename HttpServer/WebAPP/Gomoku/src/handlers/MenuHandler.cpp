#include "../../include/handlers/MenuHandler.h"

void MenuHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    std::string reqFile("/home/yebidang/workplace/HttpServer/WebAPP/Gomoku/resource/menu.html");
    FileUtil fileUtils(reqFile);
    if (!fileUtils.isValid())
    {
        LOG_WARN << reqFile << " is not valid";
        fileUtils.resetDefaultFile(); // 404 not found
    }
    std::vector<char> buffer(fileUtils.size());
    fileUtils.readFile(buffer);
    
    // 转换成字符串
    std::string buf = std::string(buffer.data(), buffer.size());

    resp->setStatusLine(http::HttpResponse::k200Ok, "OK", req.getVersion());
    resp->setContentType("text/html");
    resp->setBody(buf);
    resp->setContentLength(buf.size());
    resp->setCloseConnection(false);
    
}