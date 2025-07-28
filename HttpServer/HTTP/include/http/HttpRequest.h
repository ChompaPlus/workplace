#pragma once

#include <map>
#include <string>
#include <unordered_map>
#include <muduo/base/Timestamp.h>

namespace http
{
class HttpRequest
{
public:
    // 请求方法，获取请求的目的
    enum Method
    {
        kInvalid,
        kGet,
        kPost,
        kDelete,
        kPut,
        kHead,
        kOptions

    };

    HttpRequest() : method_(kInvalid), version_("Unknow") {}

    // 核心方法
    bool setMethod(const char* start, const char* end); // 传入的应该是用户缓存的某一段地址的起点和终点
    Method method() const { return method_; }

    void addHeader(const char* start, const char* colon, const char* end); // colon指向请求头中冒号所在位置的指针
    std::string getHeader(const std::string& field) const;
    const std::map<std::string, std::string>& headers() const { return headers_; }

    // 这里是两种方法进行设置i请求体
    void setBody(const std::string& body) { content_= body; }
    void setBody(const char* start, const char* end) 
    {
        if (end > start)
        {
            content_.assign(start, end-start); // 第一个参数为其实位置，第二个参数为长度
        }
    }
    std::string getBody() const { return content_; }
    void setContentLength(uint64_t length) { contentLength_ = length; }
    uint64_t ContentLength() const { return contentLength_; }

    void setPath(const char* start, const char* end);
    std::string path() const { return path_; }

    // 其他方法
    void setReceiveTime(muduo::Timestamp t);
    muduo::Timestamp receiveTime() const { return receiveTime_; }

    void setPathParameters(const std::string& key, const std::string& value);
    std::string getPathParameters(const std::string& key) const;

    void setQueryPathParameters(const char* start, const char* value);
    std::string getQueryParameters(const std::string &key) const;

    void setVersion(std::string version) { version_ = version;}
    std::string getVersion() const { return version_; }


    void swap(HttpRequest& that);



private:
    // 请求行：请求方法，http协议版本，URL（及参数）
    Method method_; // 方法
    std::string version_; // 请求行：：http协议版本
    
    std::string path_; // URL
    std::unordered_map<std::string, std::string> pathParameters_;// 路径参数，用来支持动态路由
    // 利用这些参数来确定执行某一个特定的回调函数
    std::unordered_map<std::string, std::string> queryParameters_; // URL查询参数

    // 请求头:（字段：内容）,key是按字母顺序排列的，并且是不可重复的
    std::map<std::string, std::string> headers_;

    // 请求体，如果使用了post或者put方法，就会有请求体
    uint64_t contentLength_ {0}; // 这个字段是放在请求头中的，标识了请求主体的长度
    std::string content_;// 请求体的内容

    muduo::Timestamp receiveTime_;// 接收时间，用了muduo的时间戳模块，可以计算时间差，比较时间点

};

}
