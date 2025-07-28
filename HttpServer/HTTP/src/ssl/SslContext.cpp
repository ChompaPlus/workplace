#include "../../include/ssl/SslContext.h"
#include <muduo/base/Logging.h>
#include <openssl/err.h>

namespace ssl
{
SslContext::SslContext(const SslConfig& config) : ctx_(nullptr), config_(config) {}

SslContext::~SslContext()
{
    if (ctx_)
    {
        SSL_CTX_free(ctx_); // 安全的释放SSL上下文及其关联的资源
        // 依次为证书、私钥、证书链、会话缓存、密码套件
    }
}
bool SslContext::initialize()
{
    // 初始化 OpenSSL
    // OPENSSL_INIT_LOAD_SSL_STRINGS 加载SSL相关的错误字符串
    // 加载加密算法相关的错误字符串 OPENSSL_INIT_LOAD_CRYPTO_STRINGS
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | 
                    OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);

    // 创建SSL上下文
    const SSL_METHOD* method = TLS_server_method(); //返回一个支持TLS1.0+的服务端方法对象（自动协商最高版本）
    ctx_ = SSL_CTX_new(method); // 创建SSL上下文
    if (!ctx_)
    {
        handleSslError("Failed to create SSL context");
        return false;
    }

    // 设置SSL选项
    // 禁用已经废弃的SSL2.0 3.0（易受POODLE攻击） | 禁用TLS压缩（防止攻击泄露加密数据） | 服务器优先选择加密套件
    long options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION
                    | SSL_OP_CIPHER_SERVER_PREFERENCE;
    SSL_CTX_set_options(ctx_, options);

    // 加载证书和私钥
    if (!loadCertificates())
    {
        return false;
    }
    // 设置协议版本
    if (!setupProtocol())
    {
        return false;
    }
    // 设置会话缓存
    setupSessionCache();

    LOG_INFO << "SSL initialized successful";
    return true; 

}

bool SslContext::loadCertificates()
{
    // 加载证书
    // SSL_FILETYPE_PEM 用于指定证书或者密钥格式为PEM，告知OPENSSL如何解析文件内容
    if (SSL_CTX_use_certificate_file(ctx_, config_.getCertificateFile().c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        handleSslError("failed to load server certificate");
        return false;
    }
    // 加载私钥
    if (SSL_CTX_use_PrivateKey_file(ctx_, config_.getPrivateKeyFile().c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        handleSslError("failed to load server PrivateKey");
        return false;
    }
    // 验证私钥
    if (!SSL_CTX_check_private_key(ctx_))
    {
        handleSslError("Private key does not match the certificate");
        return false;
    }
    // 加载证书链
    if (!config_.getCertificateChainFile().empty())
    {
        if (SSL_CTX_use_certificate_chain_file(ctx_, config_.getCertificateChainFile().c_str()) <= 0)
        {
            handleSslError("failed to load cetificate chain");
            return false;
        }
    }
    return true;
}
// 加密算法和协议版本设置
bool SslContext::setupProtocol()
{
    // 设置SSL/TLS版本
    long options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
    switch (config_.getProtocolVersion())
    {
    case SSLVersion::TLS_1_0:
        options |= SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2 | SSL_OP_NO_TLSv1_3;
        break;
    case SSLVersion::TLS_1_1:
        options |= SSL_OP_NO_TLSv1_2 | SSL_OP_NO_TLSv1_3;
        break;
    case SSLVersion::TLS_1_2:
        options |= SSL_OP_NO_TLSv1_3;
        break;
    case SSLVersion::TLS_1_3:
        break;
    }
    SSL_CTX_set_options(ctx_, options);
    // 加密套件:根据要求来限制可用的协议版本和加密算法
    if (!config_.getCipherList().empty())
    {
        if (SSL_CTX_set_cipher_list(ctx_, config_.getCipherList().c_str()) <= 0)
        {
            handleSslError("failed to set cipher list");
            return false;
        }
    }
    return true;
}
void SslContext::setupSessionCache()
{
    SSL_CTX_set_session_cache_mode(ctx_, SSL_SESS_CACHE_SERVER); //设置服务端的会话缓存
    SSL_CTX_sess_set_cache_size(ctx_, config_.getSessionCacheSize());// 缓存长度
    SSL_CTX_set_timeout(ctx_, config_.getSessionTimeout());// 缓存失效时间
}

void SslContext::handleSslError(const char* msg)
{
    char buf[256];// 分配缓存空间存错误信息
    // 格式化错误信息，将openssl错误码转换成可读的字符串
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    LOG_ERROR << msg << ": " << buf;
}
} // namespace ssl
