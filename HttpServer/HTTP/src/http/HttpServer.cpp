#include "../../include/http/HttpServer.h"
#include <functional>
#include <memory>
#include <any>

namespace http
{
HttpServer::HttpServer(int port,
                       const std::string& name,
                       bool useSsL,
                       muduo::net::TcpServer::Option option)
    : listenAddr_(port), server_(&mainLoop_, listenAddr_, name, option), useSsl_(useSsL),httpCallback_(std::bind(&HttpServer::handleRequest, this, std::placeholders::_1, std::placeholders::_2))
{
    initialize();
}

void HttpServer::initialize() 
{
    server_.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    
}

void HttpServer::start()
{
    server_.start();
    mainLoop_.loop(); // 启动事件循环
}

// ssl配置：证书、证书链、私钥、协议版本、加密套件、客户端验证、验证深度、会话超时、会话缓存大小
void HttpServer::setSslConfig(const ssl::SslConfig &config)
{
    if (useSsl_)
    {
        sslCtx_ = std::make_unique<ssl::SslContext>(config);
        if (!sslCtx_->initialize())
        {
            LOG_ERROR << "SslContext initialize failed";
            abort();
        }
    }
}

void HttpServer::onConnection(const muduo::net::TcpConnectionPtr& conn)
{
    if (conn->connected()) // 新TCP连接
    {
        // SSL握手
        if (useSsl_)
        {
            auto sslConn = std::make_unique<ssl::SslConnection>(conn, sslCtx_.get());
            sslConn->setMessageCallback(std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            sslConns_[conn] = std::move(sslConn);
            sslConns_[conn]->startHandShake(); // 启动SSL握手
        }
        conn->setContext(HttpContext()); // 为每个连接创建一个HttpContext对象
    }
    else{
        sslConns_.erase(conn);
    }
}

void HttpServer::onMessage(const muduo::net::TcpConnectionPtr &conn,
                           muduo::net::Buffer *buf,
                           muduo::Timestamp receiveTime)
{
    try
    {
        // 这层判断只是代表是否支持ssl
        if (useSsl_)
        {
            LOG_INFO << "onMessage useSSL_ is true";
            // 1.查找对应的SSL连接
            auto it = sslConns_.find(conn);
            if (it != sslConns_.end())
            {
                LOG_INFO << "onMessage sslConns_ is not empty";
                // 2. SSL连接处理数据
                it->second->onRead(conn, buf, receiveTime);

                // 3. 如果 SSL 握手还未完成，直接返回
                if (!it->second->isHandShakeCompleted())
                {
                    LOG_INFO << "onMessage sslConns_ is not empty";
                    return;
                }

                // 4. 从SSL连接的解密缓冲区获取数据
                muduo::net::Buffer* decryptedBuf = it->second->getDecryptedBuffer();
                if (decryptedBuf->readableBytes() == 0)
                    return; // 没有解密后的数据

                // 5. 使用解密后的数据进行HTTP 处理
                buf = decryptedBuf; // 将 buf 指向解密后的数据
                LOG_INFO << "onMessage decryptedBuf is not empty";
            }
        }
        // HttpContext对象用于解析出buf中的请求报文，并把报文的关键信息封装到HttpRequest对象中
        HttpContext *context = boost::any_cast<HttpContext>(conn->getMutableContext());
        if (!context->parseRequest(buf, receiveTime)) // 解析一个http请求
        {
            // 如果解析http报文过程中出错
            conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
            conn->shutdown();
        }
        // 如果buf缓冲区中解析出一个完整的数据包才封装响应报文
        if (context->gotAll())
        {
            onRequest(conn, context->request());
            context->reset();
        }
    }
    catch (const std::exception &e)
    {
        // 捕获异常，返回错误信息
        LOG_ERROR << "Exception in onMessage: " << e.what();
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
    }
}
// void HttpServer::onMessage(const muduo::net::TcpConnectionPtr& conn,
//                            muduo::net::Buffer* buf,
//                            muduo::Timestamp receiveTime)
// {
//     try
//     {
//         if (useSsl_)
//         {
//             auto it = sslConns_.find(conn);
//             if (it != sslConns_.end())
//             {
//                 it->second->onRead(conn, buf, receiveTime);
//                 if (!it->second->isHandShakeCompleted())
//                 {
//                     return;
//                 }

//                 // 握手完成，开始处理HTTP请求
//                 // 从缓冲区中读取解密后的数据
//                 muduo::net::Buffer* decryptedBuffer = it->second->getDecryptedBuffer();
//                 if (decryptedBuffer->readableBytes() ==  0) return;

//                 buf = decryptedBuffer;
//             }
//         }
//         // 处理HTTP请求
//         auto context = boost::any_cast<HttpContext>(conn->getMutableContext());
//         if (context == nullptr)
//         {
//             LOG_ERROR << "Invalid context type, expected HttpContext";
//             return;
//         }
//         if (!context->parseRequest(buf, receiveTime)) // 封装request
//         {
//             // 解析失败，返回400错误
//             conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
//             conn->shutdown();
//         }
//         if (context->gotAll())
//         {
//             onRequest(conn, context->request()); // 处理请求
//             context->reset();// 重置context
//         }
//     }
//     catch(const std::exception& e)
//     {
//         LOG_ERROR << "Exception in HttpServer::onMessage:" << e.what();
//         conn->send("HTTP/1.1 500 Internal Server Error\r\n\r\n");
//         conn->shutdown();
//     }
    
// }

void HttpServer::onRequest(const muduo::net::TcpConnectionPtr &conn, const HttpRequest &req)
{
    const std::string& connection = req.getHeader("Connection");
    // 如果请求的connection字段为close或者HTTP版本为1.0且connection字段为Keep-Alive
    bool close = connection == "close" ||
                 (req.getVersion() == "HTTP/1.0" && connection != "Keep-Alive");
    HttpResponse response(close); // 封装response
    httpCallback_(req, &response); // 处理请求

    // 如果使用SSL，加密后发送响应
    if (useSsl_)
    {
        auto it = sslConns_.find(conn);
        if (it!= sslConns_.end())
        {
            // 获取相应的长度
            size_t responseLength = response.getContentLength();
            it->second->send(&response, responseLength);
            if (response.closeConnection())
            {
                conn->shutdown();
            }
            return;
        }
    }

    muduo::net::Buffer buf;
    response.appendToBuffer(&buf);// 将response放入发送缓冲区
    conn->send(&buf); // 发送响应
    if (response.closeConnection())
    {
        conn->shutdown();
    }

}

void HttpServer::handleRequest(const HttpRequest &req, HttpResponse *resp)
{
    try
    {
        // 中间件链处理
        HttpRequest mutableReq = req;
        middlewareChain_.processBefore(mutableReq);

        // 路由处理
        if (!router_.route(mutableReq, resp))
        {
            // 路由失败，返回404错误
            resp->setStatusCode(HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setCloseConnection(true);
        }
        
        middlewareChain_.processAfter(*resp);
    }
    catch (const HttpResponse& res)
    {
        *resp = res;// 捕获到预检问请求的响应，直接返回
    }
    catch(const std::exception& e)
    {
        LOG_ERROR << "Exception in HttpServer::handleRequest:" << e.what();
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Server Error");
    }
    
}
}