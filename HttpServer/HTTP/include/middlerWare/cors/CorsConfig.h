#pragma once
#include <string>
#include <vector>

namespace http
{
namespace middleware
{
struct CorsConfig
{
    std::vector<std::string> allowedOrigins;// 规定了哪些源可以访问本服务器
    std::vector<std::string> allowedMethod;// 规定了访问本服务器可以使用的方法
    std::vector<std::string> allowedHeaders;// 允许使用的字段
    bool allowCredentials = false; // 是否允许携带cookies
    int maxAge = 3600; // 预检请求被缓存的时间

    static CorsConfig defaultConfig() // 构造函数
    {
        CorsConfig config;
        config.allowedOrigins = {"*"};
        config.allowedMethod = {"GET", "POST", "PUT", "DELETE", "OPTIONS"};
        config.allowedHeaders = {"Content-Type", "Authorization"}; // 响应主体的数据类型以及用于认证的凭证信息
        return config;
    }

};

}
}