#include "../../include/handlers/EntryHandler.h"

void EntryHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    std::string reqFile("/home/yebidang/workplace/HttpServer/WebAPP/Gomoku/resource/entry.html");
    FileUtil fileUtils(reqFile);
    if (!fileUtils.isValid())
    {
        LOG_WARN << "文件不存在:" << reqFile;
        fileUtils.resetDefaultFile();
    }
    size_t fileSize = fileUtils.size();
    if (fileSize == 0 || fileSize > INT_MAX) // 避免0或过大值
    {
        LOG_ERROR << "文件大小无效: " << reqFile << " size=" << fileSize;
        resp->setStatusCode(http::HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Invalid File Size");
        resp->setCloseConnection(true);
        return;
    }
    std::vector<char> buffer(fileUtils.size());
    fileUtils.readFile(buffer);
    std::string htmlContent(buffer.data(), buffer.size());
    // 构建响应返回成功信息
    server_->packageResp(req.getVersion(), http::HttpResponse::k200Ok
                      , "OK", false, "text/html", htmlContent, htmlContent.length(), resp);

}