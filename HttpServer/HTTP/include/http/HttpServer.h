#pragma once

#include <sys/types.h> // 基本的数据类型
#include <sys/stat.h> // 文件的状态信息的结构和函数
// 常用的方法stat用来获取文件属性，如文件大小、权限、时间戳
#include <unistd.h>
// 这个就是POSIX操作系统的一些系统调用，读写，close，fork等

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <unordered_map>

#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h> // 日志系统
#include <muduo/net/TcpServer.h>

#include "../ssl/SslConnection.h"
#include "../ssl/SslContext.h"
#include "../middlerWare/cors/CorsMiddleware.h"
#include "../middlerWare/MiddlewareChain.h"
#include "../session/SessionManager.h"
#include "../router/Router.h"
#include "HttpContext.h"
#include "HttpResponse.h"
#include "HttpRequest.h"

namespace http
{
class HttpServer : muduo::noncopyable
{
public:
    using HttpCallback = std::function<void(const HttpRequest& , HttpResponse*)>;

    HttpServer(int port,
               const std::string& name,
               bool useSsl = false,
               muduo::net::TcpServer::Option option = muduo::net::TcpServer::kNoReusePort); //不允许重用本地端口
    
    void start();// 启动服务器
    
    void setThreadNum(int numThreads)
    {
        server_.setThreadNum(numThreads);
    }

    muduo::net::EventLoop* getLoop() const
    {
        return server_.getLoop();
    }

    void setHttpCallback(HttpCallback& cb)
    {
        httpCallback_ = cb;// 这不应该根据路由的结果来决定么
    }
    // 处理静态路由
    void Get(const std::string& path, const HttpCallback& cb)
    {
        router_.registerCallBack(HttpRequest::kGet, path, cb);
    }
    void Post(const std::string& path, const HttpCallback& cb)
    {
        router_.registerCallBack(HttpRequest::kPost, path, cb);
    }
    void Get(const std::string& path, router::Router::HandlerPtr handler)
    {
        router_.registerHandler(HttpRequest::kGet, path, handler);
    }
    void Post(const std::string& path, router::Router::HandlerPtr handler)
    {
        router_.registerHandler(HttpRequest::kPost, path, handler);
    }

    // void addStaticRoute(HttpRequest::Method method, const std::string& path, const HttpCallback& cb)
    // {
    //     router_.registerCallBack(method, path, cb);
    // } 

    // 处理动态路由
    void addRoute(HttpRequest::Method method, const std::string& path, const HttpCallback& cb)
    {
        router_.addRegexCallback(method, path, cb);
    }
    void addRoute(HttpRequest::Method method, const std::string& path, router::Router::HandlerPtr handler)
    {
        router_.addRegexHandler(method, path, handler);
    }

    // 设置会话管理器
    void setSessionManager(std::unique_ptr<session::SessionManager> sessionManager)
    {
        sessionManager_ = std::move(sessionManager);
    }

    session::SessionManager* getSessionManager() const
    {
        return sessionManager_.get();
    }

    // 添加中间链
    void addMiddleware(std::shared_ptr<middleware::Middleware> middleware)
    {
        middlewareChain_.addMiddleware(middleware);
    }

    void enableSSL(bool enable)
    {
        useSsl_ = enable;
    }
    void setSslConfig(const ssl::SslConfig& config);
    
private:
    void initialize();// 初始化httpserver
    // 新连接执行回调-》设置一个HttpContext对象用于后续解析请求
    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    // 收到连接数据执行回调-》封装request对象
    void onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp receieveTime);
    // 收到请求request执行回调-》封装response
    void onRequest(const muduo::net::TcpConnectionPtr& conn, const HttpRequest& req);

    void handleRequest(const HttpRequest& req, HttpResponse* resp);

    
private:
    ssl::SslConfig sslConfig_;
    muduo::net::InetAddress listenAddr_;// 监听地址
    muduo::net::TcpServer server_;// 处理socketfd，监听、执行回调创建conn对象、分发连接
    muduo::net::EventLoop mainLoop_;// 事件循环

    HttpCallback httpCallback_; //回调

    router::Router router_;// 路由

    std::unique_ptr<session::SessionManager> sessionManager_; // 会话管理

    middleware::MiddlewareChain middlewareChain_;

    std::unique_ptr<ssl::SslContext> sslCtx_; // ssl上下问对象
    bool                             useSsl_;
    // 管理所有HTTPS连接
    std::map<muduo::net::TcpConnectionPtr, std::unique_ptr<ssl::SslConnection>> sslConns_;
};
}