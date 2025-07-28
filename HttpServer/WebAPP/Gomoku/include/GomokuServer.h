#pragma once

#include <atomic>
#include <memory>
#include <tuple>
#include <mutex>
#include <unordered_map>

#include "AiGame.h"
#include "../../HTTP/include/http/HttpServer.h"
#include "../../HTTP/include/utils/MySqlUtil.h"
#include "../../HTTP/include/utils/JsonUtils.h"
#include "../../HTTP/include/utils/FileUtils.h"


// 路由处理器的类型定义
class LoginHandler; // 登录处理器
class EntryHandler; // 进入
class MenuHandler; // 菜单
class RegisterHandler; // 注册
class AiGameStartHandler; // 人机对战
class LogoutHandler; // 退出
class AiMoveHandler; // 人机移动
class GameBackendHandler; // 悔棋

#define DURING_GAME 1 // 游戏中
#define GAME_OVER 2 // 游戏结束

#define MAX_AIBOT_NUM 4096 // 最大的机器人数量

class GomokuServer
{
private:
    // 提供给Handler的接口
    friend class EntryHandler;
    friend class LoginHandler;
    friend class MenuHandler;
    friend class AiGameStartHandler;
    friend class LogoutHandler;
    friend class AiMoveHandler;
    friend class GameBackendHandler;
    friend class RegisterHandler;

private:
    enum GameType
    {
        NO_GAME = 0,
        MAN_VS_AI = 1,
        MAN_VS_MAN = 2
    };

    // 实际业务由GmokuServer来处理
    // 使用了HttpServer提供的接口
    http::HttpServer server_; // 服务器
    http::MySqlUtil mysql_; // 数据库操作接口

    std::unordered_map<int, std::shared_ptr<AiGame>> game_map_; // 用户id和游戏的映射
    std::mutex mutexForAiGames_; //

    // useID是否在游戏中
    std::unordered_map<int, bool> online_users;
    std::mutex mutexForOnlineUsers_;

    // 最高在线人数
    std::atomic<int> maxOnline_;

public:
    GomokuServer(int port,
                const std::string& name,
                muduo::net::TcpServer::Option option = muduo::net::TcpServer::kNoReusePort); // 不允许重用本地端口
                // 为什么不允许重用本地端口？
                // 因为如果允许重用本地端口，那么当服务器重启时，新的服务器实例可能会绑定到相同的端口，这可能导致之前的连接无法正常关闭。

    void start(); // 内部初始化服务器
    void setThreadNum(int numThreads);


private:
    // 初始化服务器
    void initialize();
    // 初始化会话模块
    void initializeSession();
    // 初始化路由
    void initializeRouter();
    // 初始化中间件
    void initializeMiddleware();

    // 设置会话管理器
    void setSessionManager(std::unique_ptr<http::session::SessionManager> sessionManager)
    {
        server_.setSessionManager(std::move(sessionManager));
    }
    
    http::session::SessionManager* getSessionManager() const
    {
        return server_.getSessionManager();
    }
    
    void restartChessGameVsAi(const http::HttpRequest& req, http::HttpResponse* resp); // 重新开始人机对战
    void getBackendData(const http::HttpRequest& req, http::HttpResponse* resp);// 悔棋

    // 打包响应: 版本，状态码，状态消息，关闭连接，响应体，响应体类型，响应体长度，响应
    void packageResp(const std::string& version, http::HttpResponse::HttpStatusCode
        statusCode, const std::string& statusMessage, bool close, 
        const std::string& body, const std::string& contentType, int contentlength, http::HttpResponse* resp);

    // 获取历史最高在线人数
    int getMaxOnline() const { return maxOnline_.load(); }

    // 获取当前在线人数
    int getCurrentOnline() const
    {
        return online_users.size();
    }

    void updateMaxOnline(int online)
    {
        maxOnline_ = std::max(maxOnline_.load(), online);
    }

    // 获取用户总人数
    int getUserTotal()
    {
        std::string sql = "SELECT COUNT(*) as count FROM users"; // 查询语句

        sql::ResultSet* res = mysql_.executeQuery(sql); // 执行查询
        if (res->next())
        {
            return res->getInt("count");
        }
        return 0;
    }
};

