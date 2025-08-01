#include "../../include/middlerWare/MiddlewareChain.h"
#include <muduo/base/Logging.h>

namespace http
{
namespace middleware
{
void MiddlewareChain::addMiddleware(std::shared_ptr<Middleware> middleware)
{
    middlewares_.push_back(middleware);
}

void MiddlewareChain::processBefore(HttpRequest& req)
{
    for (size_t i = 0; i < middlewares_.size(); ++i)
    {
        middlewares_[i]->before(req);
    }
}

void MiddlewareChain::processAfter(HttpResponse &response)
{
    try
    {
        // 反向处理响应，以保持中间件的正确执行顺序
        for (auto it = middlewares_.rbegin(); it != middlewares_.rend(); ++it)
        {
            if (*it)
            { // 添加空指针检查
                (*it)->after(response);
            }
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Error in middleware after processing: " << e.what();
    }
}
}
}