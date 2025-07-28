#pragma once

#include <vector>
#include <memory>
#include "Middleware.h"

namespace http
{
namespace middleware
{
class MiddlewareChain
{
public:
    void addMiddleware(std::shared_ptr<Middleware> middleware);
    // 请求按照顺序流过每一个中间件
    void processBefore(HttpRequest& req);
    // 响应按逆序流过每一个中间件
    void processAfter(HttpResponse& resq);

private:
    std::vector<std::shared_ptr<Middleware>> middlewares_;
};
}
}