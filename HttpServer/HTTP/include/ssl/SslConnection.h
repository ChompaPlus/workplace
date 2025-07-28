#pragma once
#include "SslContext.h"
#include <functional>
#include <muduo/net/TcpConnection.h>
#include <muduo/base/noncopyable.h>
#include <memory>
#include <muduo/net/Buffer.h>
#include <openssl/ssl.h>


namespace ssl
{
// tcp连接对象、缓冲区、时间戳（连接时间么）
using MessageCallback = std::function<void(const std::shared_ptr<muduo::net::TcpConnection>&, muduo::net::Buffer*
                                    , muduo::Timestamp)>;

class SslConnection : muduo::noncopyable
{
public:
    using TcpConnectionPtr = std::shared_ptr<muduo::net::TcpConnection>;
    using BufferPtr = muduo::net::Buffer*;
private:
    SSL*            ssl_;
    SslContext*     ctx_;
    TcpConnectionPtr   conn_; // tcp连接
    SSLState        state_; // SSL状态
    BIO*            readBio_; // 网络->SSL
    BIO*            writeBio_;// SSL->网络数据
    muduo::net::Buffer readBuffer_;
    muduo::net::Buffer writeBuffer_;
    muduo::net::Buffer decryptedBuffer_; // 解密后的数据
    MessageCallback    messageCallback_;// 消息回调
public:
    SslConnection(const TcpConnectionPtr& conn, SslContext* ctx);
    ~SslConnection();

    // SSL握手
    void startHandShake();
    bool isHandShakeCompleted() const { return state_ == SSLState::ESTABLISHED;}
    // 发数据
    void send(const void* data, size_t len);

    // 读数据: 从连接上读数据，要知道缓冲区的位置，以及数据的接收时间
    // 将数据读入readBio_
    // 然后调用onEcrypted解密数据，将数据存入decrypted
    // 然后触发回调messqgecallback告诉应用层处理数据
    void onRead(const TcpConnectionPtr& conn, BufferPtr buf, muduo::Timestamp time);
    // 获得解码数据
    muduo::net::Buffer* getDecryptedBuffer() { return &decryptedBuffer_;}

    // SSL BIO 回调操作（把ssl和底层协议进行解耦），单独处理ssl加密数据和I/O的交互
    // 方便拓展和优化
    static int bioWrite(BIO* bio, const char* data, int len);
    static int bioRead(BIO* bio, char* data, int len);
    static long bioCtrl(BIO* bio, int cmd, long num, void* ptr); // 控制BIO的行为，比如刷新缓冲区，检查数据是否可读
    // 设置消息回调
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb;}
private:
    void handleHandShake();
    void onEcrypted(const char* data, size_t len); // 发送加密数据
    void onDcrypted(const char* data, size_t len); // 将ssl加密后的数据放入指定位置缓冲区
    SSLError getLastError(int ret); 
    void handleError(SSLError error);
};
} // namespace ssl
