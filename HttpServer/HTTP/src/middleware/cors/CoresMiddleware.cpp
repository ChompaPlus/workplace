#include "../../../include/middlerWare/cors/CorsMiddleware.h"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <muduo/base/Logging.h>

namespace http
{
namespace middleware
{
CorsMiddleware::CorsMiddleware(const CorsConfig& config) : config_(config) {}

void CorsMiddleware::before(HttpRequest& req)
{
    LOG_DEBUG << "CorsMiddleware::before - Processing request";

    if (req.method() == HttpRequest::Method::kOptions)
    {
        LOG_INFO << "Processing CORS preflight request";// 日志记录这是一个预检请求
        HttpResponse response;
        handlePrefightRequest(req, response);
        throw response;// 
    }
}

void CorsMiddleware::after(HttpResponse& resp)
{
    LOG_DEBUG << "CorsMiddleware::after - Processing response";
    if (!config_.allowedOrigins.empty())
    {
        // 如果在源中发现了通配符*，说明允许所有源
        if (std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), "*")
                != config_.allowedOrigins.end())
        {
            addCorsHeaders(resp, "*");
        }
        else
        {
            // 添加第一个允许的源
            addCorsHeaders(resp, config_.allowedOrigins[0]);
        }
    }
}

// 判断是否允许使用当前源
bool CorsMiddleware::isOrigrinAllowed(const std::string& origin) const
{
    return config_.allowedOrigins.empty() || std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), "*") != config_.allowedOrigins.end() ||
            std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), origin) != config_.allowedOrigins.end(); 
}

//
void CorsMiddleware::handlePrefightRequest(const HttpRequest& req, HttpResponse& resp)
{
    // 预检请求是用来查询服务器使用的提供的方法、源、头部
    // 查看请求的源是否被允许访问服务器
    const std::string& origin = req.getHeader("Origin");
    if (!isOrigrinAllowed(origin))
    {
        LOG_WARN << "Origin is not allowed" << origin;
        // 请求的资源被禁止访问返回状态码403
        resp.setStatusCode(HttpResponse::k403Forbidden);// 属于客户端错误
        return;
    }
    addCorsHeaders(resp, origin);// 把可以访问的源构建在响应头中告诉客户端
    resp.setStatusCode(HttpResponse::k204NoContent);// 响应成功但是没有响应主体
    LOG_INFO << "Preflight request processed sucessfully";

}

void CorsMiddleware::addCorsHeaders(HttpResponse& resp, const std::string& origin)
{
    try
    {
        resp.addHeader("Access-Control-Allow-Origin", origin);
        if (config_.allowCredentials)
        {
            resp.addHeader("Access-Control-Allow-Credentials", "true");
        }
        if (!config_.allowedMethod.empty())
        {
            resp.addHeader("Access-Control-Allow-Methods", join(config_.allowedMethod, ", "));
        }
        if (!config_.allowedHeaders.empty())
        {
            resp.addHeader("Access-Control-Allow-Headers", join(config_.allowedHeaders, ", "));
        }
        // 设置预检请求的缓存时间
        resp.addHeader("Access-Control-Max-Age", std::to_string(config_.maxAge));
        LOG_DEBUG << "Cors headers added successfully";
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "Error adding CORS headers:" << e.what();
    }
}

std::string CorsMiddleware::join(const std::vector<std::string>& strings, const std::string& delimiter)
{
    std::ostringstream res;
    for (size_t i = 0; i < strings.size(); ++i)
    {
        if (i > 0) res << delimiter; 
        res << strings[i];
    }
    return res.str();
    // for (size_t i = 0; i < strings.size(); ++i)
    // {
    //     if (i>0) res += delimiter;
    //     res += strings[i];
    // }
}

}
}