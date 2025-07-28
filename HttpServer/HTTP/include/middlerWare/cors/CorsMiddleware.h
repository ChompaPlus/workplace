#pragma once
#include "../Middleware.h"
#include "../../http/HttpRequest.h"
#include "../../http/HttpRequest.h"
#include "CorsConfig.h"

namespace http
{
namespace middleware
{
class CorsMiddleware : public Middleware
{
public:
    explicit CorsMiddleware(const CorsConfig& config = CorsConfig::defaultConfig());
    
    void before(HttpRequest& req) override; // 处理预检请求
    void after(HttpResponse& resp) override;

    // 负责将字符串数组里的内容串起来形成一个更长的字符串
    std::string join(const std::vector<std::string>& strings, const std::string& delimiter);
private:
    bool isOrigrinAllowed(const std::string& origrin) const;
    void handlePrefightRequest(const HttpRequest& req, HttpResponse& resp);
    // 真正配置响应头的地方，内部会调用join
    void addCorsHeaders(HttpResponse& resp, const std::string& origin);// 处理预检请求，生成对应的响应

private:
    CorsConfig config_;

};
}
}