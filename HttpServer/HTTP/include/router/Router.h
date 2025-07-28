#pragma once
#include <iostream>
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <regex>
#include <vector>

#include "RouterHandler.h"
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http
{
namespace router
{
// 两种处理方式：简单的回调函数执行，复杂的处理逻辑交由处理器(执行多个相关函数)
// 两种路由：静态路由注册回调，动态路由使用正则表达式统一，这样可以用一个处理器处理所有的动态路由
class Router
{
public:
    using HandlerPtr = std::shared_ptr<RouterHandler>;
    // void(const HttpRequest&, const HttpResponse*) 这是一个仿函数
    using HandlerCallback = std::function<void(const HttpRequest&, HttpResponse*)>;
    
    // 路由键
    struct RouterKey
    {
        HttpRequest::Method method;
        std::string path;
        // 这个方法就是比较请求的url和method是否完全匹配
        // operator==的作用就是定义两个RouterKey对象的相等条件
        bool operator==(const RouterKey &other) const // equalKey
        {
            return method == other.method && path == other.path;
        }
    };
    // 为路由键定义哈希函数
    struct RouteKeyHash
    {
        // 自定义哈希函数对象
        size_t operator()(const RouterKey& key) const
        {
            // 生成hashcode，尽量不要重复
            size_t methodHash = std::hash<int>{}(static_cast<int>(key.method)); // 将枚举值转换为整型计算哈希
            size_t pathHash = std::hash<std::string>{}(key.path); // 通过字符串计算哈希
            return methodHash * 31 + pathHash; // 将二者的哈希值组合用于unordered_map
            // *31避免哈希冲突
        }
    };
    
    // 静态路由的精准注册，就是将key{方法，url}和value{回调}注册到哈希表中，当有url和方法完全匹配时执行value
    void registerHandler(HttpRequest::Method method, const std::string& path, HandlerPtr handler); //注册到处理器，哈希表 key：方法+路径 value：处理器/回调
    void registerCallBack(HttpRequest::Method method, const std::string& path, const HandlerCallback& callback);// 注册到回调

    // 动态路由的注册
    void addRegexHandler(HttpRequest::Method method, const std::string& path, HandlerPtr handler)
    {
        std::regex pathRegex = convertToRegex(path); // 转化成正则
        regexHandlers_.emplace_back(method, pathRegex, handler);

    }
    void addRegexCallback(HttpRequest::Method method, const std::string& path, const HandlerCallback& callback)
    {
        std::regex pathRegex = convertToRegex(path);
        regexCallbacks_.emplace_back(method, pathRegex, callback);
    }

    // 处理请求,执行回调
    bool route(const HttpRequest& req, HttpResponse* resp);

private:
    std::regex convertToRegex(const std::string& path)
    {
        // 动态路由统一由一个处理器处理，但是如何让每个动态路由匹配到同一个处理器呢：正则
        // ([^/]+) : 捕获非斜杠的任意字符序列
        // /: ：匹配冒号前缀
        std::string regexPattern = "^" + std::regex_replace(path, std::regex(R"(/:([^/]+))"), R"(/([^/]+))");
        return std::regex(regexPattern);
    }

    // 提取路径参数
    void extractPathParameters(const std::smatch &match, HttpRequest& req)
    {
        
        for(size_t i = 1; i < match.size(); ++i)
        {// i = 0:整个匹配的字符串
            // i >= 1: 匹配正则表达式中的捕获组（已经捕获到正则表达式了）
            // /post/:123/05
            // param1 : "123" , param2 : "05"
            req.setPathParameters("param" + std::to_string(i), match[i].str());
        }
    }

private:
    struct RouteCallbackObj
    {
        HttpRequest::Method method_;
        std::regex pathRegex_;
        HandlerCallback callback_;
        RouteCallbackObj(HttpRequest::Method method, std::regex pathRegex, const HandlerCallback& callback) 
                        : method_(method), pathRegex_(pathRegex), callback_(callback) {}
    };

    struct RouteHandlerObj
    {
        HttpRequest::Method method_;
        std::regex pathRegex_;
        HandlerPtr handler_;
        RouteHandlerObj (HttpRequest::Method m, std::regex p, HandlerPtr h) : method_(m), pathRegex_(p), handler_(h) {}
    };

    // 第一个参数为key， 第二个参数为value， 第三个参数是哈希函数，如果不使用就是默认的红黑树的map
    std::unordered_map<RouterKey, HandlerPtr, RouteKeyHash> handlers_;
    std::unordered_map<RouterKey, HandlerCallback, RouteKeyHash> callbacks_;

    std::vector<RouteHandlerObj> regexHandlers_;
    std::vector<RouteCallbackObj> regexCallbacks_;


};
} // namespace router

} //namespace http
