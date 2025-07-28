#include "../../include/http/HttpContext.h"
#include <muduo/base/Logging.h>
using namespace muduo;
using namespace muduo::net;

namespace http
{
bool HttpContext::parseRequest(Buffer *buf, Timestamp receiveTime)
{
    bool ok = true; // 解析每行请求格式是否正确
    bool hasMore = true;
    while (hasMore)
    {
        if (state_ == kExpectRequestLine)
        {
            const char *crlf = buf->findCRLF(); // 注意这个返回值边界可能有错
            if (crlf)
            {
                ok = processRequestLine(buf->peek(), crlf);
                if (ok)
                {
                    request_.setReceiveTime(receiveTime);
                    buf->retrieveUntil(crlf + 2);
                    state_ = kExpectHeaders;
                }
                else
                {
                    hasMore = false;
                }
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == kExpectHeaders)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                const char *colon = std::find(buf->peek(), crlf, ':');
                if (colon < crlf)
                {
                    request_.addHeader(buf->peek(), colon, crlf);
                }
                else if (buf->peek() == crlf)
                { 
                    // 空行，结束Header
                    // 根据请求方法和Content-Length判断是否需要继续读取body
                    if (request_.method() == HttpRequest::kPost || 
                        request_.method() == HttpRequest::kPut)
                    {
                        std::string contentLength = request_.getHeader("Content-Length");
                        if (!contentLength.empty())
                        {
                            request_.setContentLength(std::stoi(contentLength));
                            if (request_.ContentLength() > 0)
                            {
                                state_ = kExpectBody;
                            }
                            else
                            {
                                state_ = kGotAll;
                                hasMore = false;
                            }
                        }
                        else
                        {
                            // POST/PUT 请求没有 Content-Length，是HTTP语法错误
                            ok = false;
                            hasMore = false;
                        }
                    }
                    else
                    {
                        // GET/HEAD/DELETE 等方法直接完成（没有请求体）
                        state_ = kGotAll; 
                        hasMore = false;
                    }
                }
                else
                {
                    ok = false; // Header行格式错误
                    hasMore = false;
                }
                buf->retrieveUntil(crlf + 2); // 开始读指针指向下一行数据
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == kExpectBody)
        {
            // 检查缓冲区中是否有足够的数据
            if (buf->readableBytes() < request_.ContentLength())
            {
                hasMore = false; // 数据不完整，等待更多数据
                return true;
            }

            // 只读取 Content-Length 指定的长度
            std::string body(buf->peek(), buf->peek() + request_.ContentLength());
            request_.setBody(body);

            // 准确移动读指针
            buf->retrieve(request_.ContentLength());

            state_ = kGotAll;
            hasMore = false;
        }
    }
    return ok; // ok为false代表报文语法解析错误
}
// bool HttpContext::parseRequest(Buffer* buf, Timestamp receiveTime)
// {
//     // 空指针检查
//     if (!buf || buf->readableBytes() == 0 || buf->peek() == nullptr) {
//         LOG_WARN << "Invalid buffer or empty data";
//         return false;
//     }
//     bool ok = true; // 解析每行请求格式是否正确
//     bool hasMore = true; 
//     // 按步骤封装request_对象
//     while (hasMore)
//     {
//         if (state_ == HttpContext::kExpectRequestLine)
//         {
//             const char *crlf = buf->findCRLF();// 获取刻度数据的边界(end，终止位置)，知道要读到什么位置
//             // 这个边界可能是NULL值，所以要判断一下
//             if (crlf)
//             {
//                 // peek是buffer对象的核心方法，用来获取可读数据的起始位置
//                 // 封装请求行
//                 ok = processRequestLine(buf->peek(), crlf);
//                 if (ok)
//                 {
//                     request_.setReceiveTime(receiveTime);

//                     // 现在已经读取了请求行，移动指针到下一行
//                     buf->retrieveUntil(crlf+2); // +2是为了跳过\r\n

//                     state_ = HttpContext::kExpectHeaders; // 请求行已经处理完毕状态切换，再次循环时开始处理解析请求头        
//                 }else
//                 {
//                     hasMore = false; // 请求行的数据获取失败
//                 }
//             }else
//             {
//                 hasMore = false;// 返回数据的边界值错误，无法提取数据
//             }
//         }
//         else if (state_ == kExpectHeaders) // 这里会执行多次，因为会处理多个头部字段
//         {
//             // 还是先获取数据边界
//             const char* crlf = buf->findCRLF();
//             if (crlf) 
//             {
//                 // 获取冒号的位置/地址
//                 const char* colon = std::find(buf->peek(), crlf, ':');
//                 if (colon < crlf)
//                 {
//                     request_.addHeader(buf->peek(), colon, crlf);// 构建请求头
//                 }
//                 //
//                 else if (buf->peek() == crlf)
//                 {
//                     // 已经处理完请求头所有字段，然后空行
//                     // 根据请求方法考虑怎么处理请求主体，不是每个方法都有请求体的
//                     // 前面已经封装好请求行了所以这里可以通过对象获取方法
//                     if (request_.method() == HttpRequest::kPost || request_.method() == HttpRequest::kPut)
//                     {
//                         // 会在headers_通过key找到value然后返回
//                         std::string contentLength = request_.getHeader("Content-Length");
//                         if (!contentLength.empty())
//                         {
//                             // 找到了长度也知道长度确实存在，下一步就是封装request_对象
//                             request_.setContentLength(std::stoi(contentLength));
//                             if (request_.ContentLength() > 0)
//                             {
//                                 state_ = kExpectBody; //确定有请求体了下次循环封装
                                
//                             }
//                             else 
//                             {
//                                 state_ = kGotAll;
//                                 hasMore = false;
//                             }
//                         }
//                         else
//                         {
//                             ok = false; // 这是当前解析步骤失败的标志，用于返回结果
//                             hasMore = false;
//                         }
//                     }
//                     else
//                     {
//                         state_ = kGotAll;// 不是put或者post，解析完成
//                         hasMore = false;
//                     }
//                 }
//                 else
//                 {
//                     ok = false;
//                     hasMore = false;
//                 }
//                 buf->retrieveUntil(crlf + 2);// 指针继续移动
//             }
//             else 
//             {
//                 hasMore = false;
//             }
//         }
//         else if (state_ == kExpectBody)
//         {
//             if (buf->readableBytes() < request_.ContentLength()) //可读数据的长度小于请求体长度
//             {
//                 hasMore = false; // 数据不完整等待更多数据
//                 return true;
//             }
//             std::string body(buf->peek(), buf->peek() + request_.ContentLength());// 构造了一个字符串不是调用函数
//             request_.setBody(body);
//             // 两种设置请求体的方法，不使用下面这个是因为之前已经确定数据是存在的了
//             // const char* crlf = buf->findCRLF();
//             // if (crlf)
//             // {
//             //     request_.setBody(buf->peek(), crlf);
//             // }
//             buf->retrieve(request_.ContentLength()); // 指针移动到写缓冲区的起始位置
//             state_ = kGotAll;
//             hasMore = false;
//         }
//     }
//     return ok;
// }

bool  HttpContext::processRequestLine(const char* start, const char* end) 
{
    bool ok = false;
    if (!start || !end || end < start) {
        LOG_WARN << "Invalid request line pointer range";
        return false;
    }
    // 请求行每个类型的内容是用空格来分隔的，占1个字符的长度

    const char* space = std::find(start, end, ' ');
    if (space != end && request_.setMethod(start, space))
    {
        start = space + 1;
        space = std::find(start, end, ' '); // 指针移动到下一个内容
        if (space != end)
        {
            const char* argumentStart = std::find(start, space, '?'); // 处理 《路径?查询参数》
            if (argumentStart != end && argumentStart < space)
            {
                request_.setPath(start, argumentStart);
                request_.setQueryPathParameters(argumentStart + 1, space);
            }
            else
            {
                // 没有查询参数的路径
                request_.setPath(start, space);
            }
            // 起始指针移动开始处理协议版本
            start = space + 1;
            // 确定版本格式对不对，以及必须是HTTP/1版本
            // 显然这里边界条件是左闭右开的
            ok = ((end - start == 8)) && std::equal(start, end-1, "HTTP/1.");
            if (ok)
            {
                if (*(end-1) == '1')
                {
                    request_.setVersion("HTTP/1.1");
                }
                else if (*(end-1) == '0')
                {
                    request_.setVersion("HTTP/1.0");
                }
                else{
                    ok = false;
                }
            }
        }
    }
    return ok;
}

}