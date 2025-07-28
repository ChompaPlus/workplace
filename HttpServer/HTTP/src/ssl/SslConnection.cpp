#include "../../include/ssl/SslConnection.h"
#include <muduo/base/Logging.h>
#include <openssl/err.h>

namespace ssl
{
// SSL BIO
// 
static BIO_METHOD* createCustomBioMethod()
{
    // 自定义BIO方法，指定数据读写和控制BIO行为操作的回调函数
    // 属性为内存BIO，数据存在内存中
    BIO_METHOD* method = BIO_meth_new(BIO_TYPE_MEM, "custom");
    BIO_meth_set_write(method, SslConnection::bioWrite);
    BIO_meth_set_read(method, SslConnection::bioRead);
    BIO_meth_set_ctrl(method, SslConnection::bioCtrl); // 控制BIO的行为
    return method;
}
// 在tcp连接的基础上进行ssl握手，然后通过ssl上下文实现加密和通信
SslConnection::SslConnection(const TcpConnectionPtr& conn, SslContext* ctx)
        : ssl_(nullptr)
        , ctx_(ctx)
        , conn_(conn)
        , state_(SSLState::HANDSHAKE)
        , readBio_(nullptr)
        , writeBio_(nullptr)
        , messageCallback_(nullptr)
{
    // 创建SSL对象, 依托于ssl上下文创建的
    ssl_ = SSL_new(ctx_->getNativeHandle());
    if (!ssl_)
    {
        LOG_ERROR << "Failed to create SSL:" << ERR_error_string(ERR_get_error(), nullptr);
        return;
    }

    // 创建BIO
    readBio_ = BIO_new(BIO_s_mem());// 还是一样用内存存数据
    writeBio_ = BIO_new(BIO_s_mem());

    if (!readBio_||!writeBio_)
    {
        LOG_ERROR << "Failed to create BIO";
        // 释放上面创建的ssl
        SSL_free(ssl_);
        ssl_ = nullptr;
        return;
    }

    SSL_set_bio(ssl_, readBio_, writeBio_);// 绑定bio给ssl对象
    SSL_set_accept_state(ssl_); // 设置服务器模式

    // 设置SSL选项
    SSL_set_mode(ssl_, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);

    // 设置连接回调
    // 当数据来时，触发回调调用onread
    conn_->setMessageCallback(std::bind(&SslConnection::onRead, this, std::placeholders::_1,
                            std::placeholders::_2, std::placeholders::_3));
}

SslConnection::~SslConnection()
{
    if (ssl_)
    {
        SSL_free(ssl_);
    }
}

void SslConnection::startHandShake()
{
    SSL_set_accept_state(ssl_);
    handleHandShake();
}


// const void* data : 要发送的数据的指针
// size_t len : 数据的长度
void SslConnection::send(const void* data, size_t len)
{
    if (state_ != SSLState::ESTABLISHED)
    {
        LOG_ERROR << "cannot send data before SSL handshake";
        return;
    }
    int written = SSL_write(ssl_, data, len); // 写到writebio
    if (written <= 0) //
    {
        int err = SSL_get_error(ssl_, written);
        LOG_ERROR << "SSL_write failed:" << ERR_error_string(err, nullptr);
        return;
    }
    char buf[4096];
    int pending;
    // 这是检查writebio中的数据量
    while (pending = BIO_pending(writeBio_) > 0)
    {
        // 
        int bytes = BIO_read(writeBio_, buf, std::min(pending, static_cast<int>(sizeof(buf))));
        if (bytes > 0)
        {
            conn_->send(buf, bytes);// 发送到对端
        }
    } 
}

// 将数据从TCP上读到readbio
void SslConnection::onRead(const TcpConnectionPtr& conn, BufferPtr buf, muduo::Timestamp time)
{
    // 握手阶段
    if (state_ == SSLState::HANDSHAKE)
    {
        // 将数据写入BIO
        BIO_write(readBio_, buf->peek(), buf->readableBytes());
        // 指针移动
        buf->retrieve(buf->readableBytes());
        handleHandShake();
        return;
    }else if(state_ == SSLState::ESTABLISHED) // 加密通信阶段
    {
        // 第一步：先把 TCP 收到的密文数据写入 readBio_
        BIO_write(readBio_, buf->peek(), buf->readableBytes());
        buf->retrieve(buf->readableBytes());
        // 在缓存中预留解密后数据的空间
        char decryptedData[4096];
        // 将指定数据大小的数据读出来并解密
        int ret = SSL_read(ssl_, decryptedData, sizeof(decryptedData));
        if (ret > 0)
        {   // 存解密后的数据
            muduo::net::Buffer decryptedBuffer;
            decryptedBuffer.append(decryptedData, ret);
        }
        // 调用回调让应用程序去处理
        if (messageCallback_)
        {
            messageCallback_(conn, &decryptedBuffer_, time);
        }
    }
}

void SslConnection::handleHandShake()
{
    int ret = SSL_do_handshake(ssl_);
    if (ret == 1)
    {
        state_ = SSLState::ESTABLISHED;
        LOG_INFO << "SSL handshake completed";
        LOG_INFO << "using cipher:" << SSL_get_cipher(ssl_);
        LOG_INFO << "protocol:" << SSL_get_version(ssl_);

        // 确保设置了回调
        if (!messageCallback_)
        {
            LOG_WARN << "NO MESSAGE CALLBACK SET AFTER SSL handshake";
        }
        return;
    }
    
    int err = SSL_get_error(ssl_, ret);
    switch (err)
    {
        // 正常的握手过程
        // 线程会阻塞在这
        case SSL_ERROR_WANT_READ:// SSL需要更多的数据才能继续握手，继续等待数据到达
        case SSL_ERROR_WANT_WRITE:// ssl需要发送更多的数据才能继续握手
            break;
        
        default:{
            // 获取详细的错误信息
            char errBuf[256];
            unsigned long errCode = ERR_get_error();
            ERR_error_string_n(errCode, errBuf, sizeof(errBuf));
            LOG_ERROR << "SSL handeshake failed:" << errBuf;
            conn_->shutdown(); //关闭tcp连接
            break;
        }
    }
    
}

void SslConnection::onEcrypted(const char* data, size_t len)
{
    writeBuffer_.append(data, len); // 将加密数据放到发送缓冲区
    conn_->send(&writeBuffer_);
}

void SslConnection::onDcrypted(const char* data, size_t len)
{
    decryptedBuffer_.append(data, len);
}

SSLError SslConnection::getLastError(int ret)
{
    int err = SSL_get_error(ssl_, ret);
    switch (err)
    {
        case SSL_ERROR_NONE:
            return SSLError::NONE; // 操作成功完成
        case SSL_ERROR_WANT_READ:
            return SSLError::WANT_READ;
        case SSL_ERROR_WANT_WRITE:
            return SSLError::WANT_WRITE;
        case SSL_ERROR_SYSCALL:
            return SSLError::SYSCALL; // 底层系统调用发生错误
        case SSL_ERROR_SSL:
            return SSLError::SSL; // SSL协议层发生错误
        default:
            return SSLError::UNKNOWN;
    }
}

// 数据问题就等待
// 其他错误记录日志并关闭连接
void SslConnection::handleError(SSLError error)
{
    switch (error)
    {
    case SSLError::WANT_READ: // 需要等待更多数据
    case SSLError::WANT_WRITE: // 需要写入更多数据
        // 等待
        break;
    case SSLError::SSL:
    case SSLError::SYSCALL:
    case SSLError::UNKNOWN:
        LOG_ERROR << "ssl ERROR occurred:" << ERR_error_string(ERR_get_error(), nullptr);
        state_ = SSLState::ERROR;
        conn_->shutdown(); // 关闭连接
        break;
    default:
        break;
    }
}

// 
int SslConnection::bioWrite(BIO* bio, const char* data, int len)
{
    SslConnection* conn = static_cast<SslConnection*>(BIO_get_data(bio));
    if (!conn) return -1;
    conn->conn_->send(data, len);
    return len;
}
int SslConnection::bioRead(BIO* bio, char* data, int len)
{
    SslConnection* conn = static_cast<SslConnection*>(BIO_get_data(bio));
    if (!conn) return -1;
    size_t readable = conn->readBuffer_.readableBytes();
    if (readable == 0)
    {
        return -1;
    }
    // 将数据从接收
    size_t toRead = std::min(static_cast<size_t>(len), readable);
    memcpy(data, conn->readBuffer_.peek(), toRead);
    conn->readBuffer_.retrieve(toRead);
    return toRead;
}
long SslConnection::bioCtrl(BIO* bio, int cmd, long num, void* ptr)
{
    switch (cmd)
    {
        case BIO_CTRL_FLUSH: // 刷新缓冲区
            return 1;
        default:
            return 0;
    }
}
} // namespace ssl
