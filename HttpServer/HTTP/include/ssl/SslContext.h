#pragma once
#include "SslConfig.h"
#include <openssl/ssl.h>
#include <memory>
#include <muduo/base/noncopyable.h> // 继承该类的基类不允许拷贝构造和复制

namespace ssl
{
class SslContext : muduo::noncopyable
// 不允许拷贝构造和拷贝赋值
{
private:
    SSL_CTX* ctx_;
    SslConfig config_;
public:
    explicit SslContext(const SslConfig& config);
    ~SslContext();

    bool initialize();
    SSL_CTX* getNativeHandle() { return ctx_; }

private:
    bool loadCertificates(); // 加载证书和密钥
    bool setupProtocol();// 设置协议版本
    void setupSessionCache(); // 设置会话缓存
    static void handleSslError(const char* msg); // 处理加密错误
};


} // namespace ssl

