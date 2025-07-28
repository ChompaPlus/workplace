#include "../../include/http/HttpResponse.h"

namespace http
{
void HttpResponse::setStatusLine(HttpStatusCode statusCode
                                , const std::string& statusMessage
                                , const std::string& version)
{
    httpVersion_ = version;
    statusCode_ = statusCode;
    statusMessage_ = statusMessage;
}

void HttpResponse::appendToBuffer(muduo::net::Buffer* output) const
{
    char buf[32];
    // 把协议版本和状态码格式化
    // 之所以不格式化状态信息，是因为状态信息的大下不固定，可能会导致缓冲区溢出
    snprintf(buf, sizeof(buf), "%s %d", httpVersion_.c_str(), statusCode_);

    output->append(buf); // 协议版本和状态码
    output->append(statusMessage_);// 状态信息
    output->append("\r\n"); // 回车换行

    if (closeConnection_) // 这里能移入headers_吗？// 可以，因为closeConnection_是一个标志位，不需要加入到headers_中
    {
        output->append("Connection: close\r\n");
    }
    else
    {
        output->append("Connection: Keep-Alive\r\n");
    }

    for (auto& header : headers_)
    {
        output->append(header.first);
        output->append(": ");
        output->append(header.second);
        output->append("\r\n");
    }
    output->append("\r\n");

    output->append(body_); // 请求体可能为空


}
}