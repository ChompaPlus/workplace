#pragma once

#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"


namespace http
{
namespace middleware
{
class Middleware
{
public:
    virtual ~Middleware() = default;

    // 这个是请求预处理（比如处理预检请求，当方法是head或者options）
    virtual void before(HttpRequest& req) = 0;
    // 发送响应之前，对响应在进行处理（如果是cors，就是通过响应头告诉浏览器是否支持当前的跨域请求）
    virtual void after(HttpResponse& resp) = 0;
    // 每个中间件都要实现自己的逻辑

    // 共有的方法，因为要实现链
    void setNext(std::shared_ptr<Middleware> next)// 这个就是把自己和下一个中间件穿起来
    {
        nextMiddleware_ = next;
    }

protected:
    std::shared_ptr<Middleware> nextMiddleware_;

};
}
}