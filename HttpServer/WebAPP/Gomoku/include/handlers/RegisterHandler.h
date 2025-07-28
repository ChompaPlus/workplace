#include "../../../HTTP/include/router/RouterHandler.h"
#include "../GomokuServer.h"
#include "../../../HTTP/include/utils/JsonUtils.h"
#include "../../../HTTP/include/utils/MySqlUtil.h"

class RegisterHandler : public http::router::RouterHandler
{
private:
    GomokuServer* server_;
    http::MySqlUtil mysql_;
public:
    explicit RegisterHandler(GomokuServer* server) : server_(server) {}
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:
    // 检查用户名是否已存在
    bool isUserExit(const std::string& username);
    // 给用户分配id
    int insertUser(const std::string& username, const std::string& password);
};