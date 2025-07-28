#include "../../../HTTP/include/router/RouterHandler.h"
#include "../GomokuServer.h"

class MenuHandler : public http::router::RouterHandler
{
private:
    GomokuServer* server_;
public:
    explicit MenuHandler(GomokuServer* server) : server_(server) {}
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
};