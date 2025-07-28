#pragma once

#include <map>
#include <muduo/net/TcpServer.h>

namespace http
{
class HttpResponse
{
public:
    enum HttpStatusCode
    {
        kUnknow,
        k200Ok = 200, // 请求成功
        // k201 = 201, // 请求成功并且创建了新资源
        k204NoContent = 204, // 请求成功，但是服务器没有返回任何数据
        k301MovedPermanently = 301, // 资源永久重定向到新位置
        // k302 = 302, // 资源临时重定向到新位置
        // k304 = 304, // 自上一次请求后，该资源没有被修改，重定向到缓存查找
        k400BadRequest = 400, // 请求无效
        k401Unauthorized = 401, // 请求需要身份验证
        k403Forbidden = 403, // 请求的资源被禁止访问
        k404NotFound = 404, // 请求的资源不存在
        k409Conflict = 409,
        // k503 = 503, // 服务器不存在
        k500InternalServerError = 500 // 服务器内部错误
    };

    HttpResponse(bool close = true) :  statusCode_(kUnknow), closeConnection_(close) {}

    // 响应行
    void setVersion(std::string version) {httpVersion_ = version;}
    std::string version() const { return httpVersion_;} 

    void setStatusCode(HttpStatusCode code) {statusCode_ = code;} 
    HttpStatusCode getStatusCode() const { return statusCode_;}

    void setStatusMessage(const std::string Message) {statusMessage_ = Message;}

    // 关闭连接
    void setCloseConnection(bool on) { closeConnection_ = on; }
    bool closeConnection() const {return closeConnection_;}

    // 响应头
    void setContentLength(uint64_t length) { addHeader("Content-Length", std::to_string(length));}
    void setContentType(const std::string& contentType) { addHeader("Content-Type", contentType);}
    void addHeader(const std::string& key, const std::string& value) {headers_[key] = value;}

    // 响应体
    void setBody(const std::string& body) { body_ = body;}
    
    // 这个上面个拆解开来了
    void setStatusLine(HttpStatusCode statusCode, const std::string& statusMessage, const std::string& version);
    void setErrorHeader() {}
    
    // 添加到用户缓存区Buffer 然后send(),写到socketfd上
    void appendToBuffer(muduo::net::Buffer* outputBuf) const;

    // 获取响应的长度：响应行 + 响应头 + 响应体
    size_t getContentLength() const
    {
        // 响应行
        size_t length = httpVersion_.size() + 1 + statusMessage_.size() + 1 + 4 + 1;
        // 响应头
        for (const auto& header : headers_)
        {
            length += header.first.size() + 1 + header.second.size() + 1;
        }
        // 响应体
        length += body_.size();
        return length;
    }
private:
    // 响应行
    HttpStatusCode statusCode_;
    std::string statusMessage_;
    std::string httpVersion_;

    // 响应头
    std::map<std::string, std::string> headers_;
    // 响应体
    std::string body_;
    
    bool closeConnection_;

    bool isFile_;
};
}