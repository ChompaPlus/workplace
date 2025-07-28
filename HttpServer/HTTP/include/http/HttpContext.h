#pragma once

#include <iostream>
#include <muduo/net/TcpServer.h>

#include "HttpRequest.h"

namespace http
{
class HttpContext
{
public:
    enum HttpRequestParseState
    {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kGotAll
    };

    bool parseRequest(muduo::net::Buffer* buf, muduo::Timestamp receiveTime);
    bool gotAll() const { return state_ == kGotAll; }

    void reset() 
    {
        state_ = kExpectRequestLine;
        HttpRequest dummyData;
        // 清空旧请求的内容
        request_.swap(dummyData); // 高效的交换两个请求的内容，避免拷贝和复制，swap应该是内部调用了move
    }

    // 获取完整的请求对象
    const HttpRequest& request() const {return request_;}
    HttpRequest& request() { return request_; }


    HttpContext() : state_(kExpectRequestLine) {}

private:
    // 处理请求行这个方法是在解析请求中调用的
    bool processRequestLine(const char* start, const char* end);

private:
    HttpRequestParseState state_;
    HttpRequest request_;
};

}