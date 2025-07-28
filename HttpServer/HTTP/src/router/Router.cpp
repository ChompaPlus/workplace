#include "../../include/router/Router.h"
#include <muduo/base/Logging.h>

namespace http
{
namespace router
{
void Router::registerHandler(HttpRequest::Method method, const std::string& path, HandlerPtr handler)
{
    RouterKey key{method, path};
    handlers_[key] = std::move(handler); // 内部自动生成hashcode
}

void Router::registerCallBack(HttpRequest::Method method, const std::string& path, const HandlerCallback &callback)
{
    RouterKey key{method, path};
    callbacks_[key] = std::move(callback); // 把callback转移到value
}

// 执行回调
bool Router::route(const HttpRequest& req, HttpResponse* resp)
{
    RouterKey key{req.method(), req.path()}; // 在表里进行匹配

    // 先去匹配处理器
    auto handlerIt = handlers_.find(key);
    if (handlerIt != handlers_.end())
    {
        handlerIt->second->handle(req, resp); // value（处理器）->执行
        return true;
    }
    //否则执行回调
    auto callbackIt = callbacks_.find(key);
    if (callbackIt != callbacks_.end())
    {
        callbackIt->second(req, resp);
        return true; 
    }

    // 如果是不是静态， 查找动态路由处理器
    for  (const auto &[method, pathRegex, handler] : regexHandlers_)
    {
        std::smatch match;
        std::string pathStr(req.path());
        // 检查方法匹配不匹配，然后检查路径参数是否和正则表达式匹配，这就是动态路由匹配的过程
        // 类似于静态路由匹配，只不过静态路由匹配方法和路径，动态路由检查方法和路径参数
        // 如果pathStr和pathRegex匹配，就会填充match对象，就可以提取参数
        if (method == req.method() && std::regex_match(pathStr, match, pathRegex))
        {
            HttpRequest newReq(req);
            // 原来的req的路径参数还没有封装完成，匹配之后重新封装，这样才是一个完整的请求对象
            extractPathParameters(match, newReq);

            handler->handle(newReq, resp);
            return true;
        }
    }
    // 否则，查动态路由回调
    for (const auto& [method, pathRegex, callback] : regexCallbacks_)
    {
        std::smatch match;
        std::string pathStr(req.path());
        if ( method == req.method() && std::regex_match(pathStr, match, pathRegex))
        {
            // 提取路径参数
            HttpRequest newReq(req);
            extractPathParameters(match, newReq);

            callback(newReq, resp);
            return true;
        }

    }
    return false;
}

}
}