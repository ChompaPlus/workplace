#include "../../include/http/HttpRequest.h"
#include <muduo/base/Logging.h>
namespace http
{
void HttpRequest::setReceiveTime(muduo::Timestamp receiveTime)
{
    receiveTime_ = receiveTime;
}

bool HttpRequest::setMethod(const char* start, const char* end)
{
    assert(method_ == kInvalid);
    std::string m(start, end);
    if (m == "GET")
    {
        method_ = kGet;
    }
    else if(m == "POST")
    {
        method_ = kPost;
    }
    else if(m == "HEAD")
    {
        method_ = kHead;
    }
    else if(m == "PUT")
    {
        method_ = kPut;
    }
    else if(m == "DELETE")
    {
        method_ = kDelete;
    }
    else
    {
        method_ = kInvalid;
    }
    return method_ != kInvalid;
}

void HttpRequest::setPath(const char* start, const char* end)
{
    path_.assign(start, end);
}

void HttpRequest::setPathParameters(const std::string& key, const std::string& value)
{
    pathParameters_[key] = value;
}

std::string HttpRequest::getPathParameters(const std::string& key) const
{
    auto it = pathParameters_.find(key);
    if (it != pathParameters_.end())
    {
        return it->second;
    }
    return "";
}

std::string HttpRequest::getQueryParameters(const std::string& key) const
{
    auto it = queryParameters_.find(key);
    if (it!= queryParameters_.end())    
    {
        return it->second;
    }
    return "";
}

// 从？后面开始，到#结束
void HttpRequest::setQueryPathParameters(const char* start, const char* end)
{
    if (!start || !end || start >= end)
    {
        LOG_ERROR << "Invalid parameter range in setQueryPathParameters (start=" << (void*)start << ", end=" << (void*)end << ")";
        return;
    }  
    if (start >= end)
    {
        LOG_ERROR << "Invalid parameter range in setQueryPathParameters (start >= end)";
        return;
    }
    std::string argStr(start, end);
    std::string::size_type pos = 0;// 记录当前位置
    std::string::size_type prev = 0;// 记录上一个位置

    // 按&分割参数
    // npos: 表示没有找到匹配的位置
    // find('&', prev): 从prev位置开始查找&的位置
    while ((pos = argStr.find('&', prev)) != std::string::npos)
    {
        std::string pair = argStr.substr(prev, pos-prev); // （起点， 长度）
        std::string::size_type eqPos = pair.find('=');
        // key1 = value1 & key2 = value2 # 
        if (eqPos!= std::string::npos)
        {
            std::string key = pair.substr(0, eqPos);
            std::string value = pair.substr(eqPos+1);// 没有标识长度，表示到结尾
            queryParameters_[key] = value;
        }
        prev = pos+1; // 更新prev位置
    }
    // 最后一个参数结尾没有&，处理最后一个参数
    std::string pair = argStr.substr(prev);
    std::string::size_type eqPos = pair.find('=');
    if(eqPos!= std::string::npos)
    {
        std::string key = pair.substr(0, eqPos);
        std::string value = pair.substr(eqPos+1);
        queryParameters_[key] = value;
    }
}

// 每次只处理一个请求头中的字段
void HttpRequest::addHeader(const char* start, const char* colon, const char* end)
{
    std::string field(start, colon);
    colon++;
    while (colon<end && isspace(*colon)) // isspace: 判断是否为空格字符
    {
        colon++;
    }
    std::string value(colon, end);
    // 消除空格
    while (!value.empty() && isspace(value[value.size()-1]))
    {
        value.resize(value.size()-1);
    }
    headers_[field] = value;
}

std::string HttpRequest::getHeader(const std::string& field) const
{
    auto it = headers_.find(field);
    if (it!= headers_.end())
    {
        return it->second;
    }
    return "";
}

// 交换两个对象
void HttpRequest::swap(HttpRequest& that)
{
    std::swap(method_, that.method_);
    std::swap(version_, that.version_);
    std::swap(path_, that.path_);
    std::swap(receiveTime_, that.receiveTime_);
    std::swap(pathParameters_, that.pathParameters_);
    std::swap(queryParameters_, that.queryParameters_);
    std::swap(headers_, that.headers_);
}
} // namespace http
