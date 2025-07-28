#pragma once

#include "../../../HTTP/include/router/RouterHandler.h"
#include "../GomokuServer.h"


class AiMoveHandler : public http::router::RouterHandler
{
private:
    GomokuServer* server_;    
public:
    explicit AiMoveHandler(GomokuServer* server) : server_(server) {}
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
    
};
