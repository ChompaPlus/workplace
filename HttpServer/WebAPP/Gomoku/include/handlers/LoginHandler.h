#pragma once
#include "../../../HTTP/include/router/RouterHandler.h"
#include "../GomokuServer.h"
#include "../../../HTTP/include/utils/JsonUtils.h"
#include "../../../HTTP/include/utils/MySqlUtil.h"

class LoginHandler : public http::router::RouterHandler
{
private:
    GomokuServer* server_;
    http::MySqlUtil mysql_;
public:
    explicit LoginHandler(GomokuServer* server) : server_(server) {}
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:
    // 检查用户名和密码是否正确
    int queryUser(const std::string& username, const std::string& password);
};