
#include "../include/GomokuServer.h"
#include "../include/handlers/LoginHandler.h"
#include "../include/handlers/EntryHandler.h"
#include "../include/handlers/MenuHandler.h"
#include "../include/handlers/AiGameStartHandler.h"
#include "../include/handlers/LogoutHandler.h"
#include "../include/handlers/AiMoveHandler.h"
#include "../include/handlers/GameBackendHandler.h"
#include "../include/handlers/RegisterHandler.h"


using namespace http;

GomokuServer::GomokuServer(int port, const std::string& name, muduo::net::TcpServer::Option option) :
    server_(port, name, option), maxOnline_(0)
{
    initialize();
}

void GomokuServer::setThreadNum(int numThreads)
{
    server_.setThreadNum(numThreads);
}

void GomokuServer::start()
{
    server_.start();
}

void GomokuServer::initialize()
{

    // 初始化会话管理器
    initializeSession();
    // 初始化路由
    initializeRouter();
    // 初始化中间件
    initializeMiddleware();
}

void GomokuServer::initializeSession()
{
    // 创建会话存储
    auto sessionStorage = std::make_unique<http::session::MemorySessionStorage>();
    if (!sessionStorage)
    {
        LOG_INFO << "Failed to create session storage";
        return;
    }
    // 创建会话管理器
    auto sessionManager = std::make_unique<http::session::SessionManager>(std::move(sessionStorage));
    if (!sessionManager)
    {
        LOG_INFO << "Failed to create session manager";
        return;
    }
    // 设置会话管理器
    server_.setSessionManager(std::move(sessionManager));
}

void GomokuServer::initializeRouter()
{
    // 注册路由处理器
    // 入口页面
    server_.Get("/", std::make_shared<EntryHandler>(this));
    server_.Get("/entry", std::make_shared<EntryHandler>(this));
    // 登录
    server_.Post("/login", std::make_shared<LoginHandler>(this));
    // 注册
    server_.Post("/register", std::make_shared<RegisterHandler>(this));
    // 登出
    server_.Post("/user/logout", std::make_shared<LogoutHandler>(this));
    // 菜单
    server_.Get("/menu", std::make_shared<MenuHandler>(this));
    // 对战ai
    server_.Get("/aiBot/start", std::make_shared<AiGameStartHandler>(this));
    // ai移动
    server_.Post("/aiBot/move", std::make_shared<AiMoveHandler>(this));
    // 重新开始对战ai
    server_.Get("/aiBot/restart", [this](const HttpRequest& req, HttpResponse* resp)
                {
                    restartChessGameVsAi(req, resp);
                });
    // 游戏后端
    server_.Get("/backend", std::make_shared<GameBackendHandler>(this));
    server_.Get("/backend_data", [this](const HttpRequest& req, HttpResponse* resp)
                {
                    getBackendData(req, resp);
                });
    // this 是什么？
    // this 是一个指向当前对象的指针，它指向当前对象的内存地址。在这个例子中，this 指向 GomokuServer 对象。
}

void GomokuServer::initializeMiddleware()
{
    // 注册中间件
    auto middleware = std::make_shared<http::middleware::CorsMiddleware>();
    if (!middleware)
    {
        LOG_INFO << "Failed to create middleware";
        return;
    }
    
    server_.addMiddleware(middleware);
}

void GomokuServer::restartChessGameVsAi(const HttpRequest& req, HttpResponse* resp)
{
    // 解析请求体
    // 获得sessionId并构建到响应中
    // 如果是第一次建立连接，那么就创建一个新的会话
    auto session = getSessionManager()->getSession(req,resp);

    if (session->getValue("isLoggedIn") != "true")
    {
        json errResp;
        errResp["status"] = "error";
        errResp["message"] = "Unauthorized";
        std::string errorBody = errResp.dump(4); // 将json对象转换为字符串

        // 构建响应返回错误信息，未登录
        packageResp(req.getVersion(), HttpResponse::k401Unauthorized
                    , "Unauthorized", true, "application/json", errorBody, errorBody.length(), resp);
        return;
    }

    int userId = std::stoi(session->getValue("userId"));
    {
        // 重新开始ai对战
        std::lock_guard<std::mutex> lock(mutexForAiGames_);
        if (game_map_.find(userId)!= game_map_.end())
        {
            game_map_.erase(userId);
        }
        game_map_[userId] = std::make_shared<AiGame>(userId);
    }   

    json scuccessResp;
    scuccessResp["status"] = "ok";
    scuccessResp["message"] = "Restarted chess game vs AI";
    std::string successBody = scuccessResp.dump(4); // 将json对象转换为字符串

    // 构建响应返回成功信息
    packageResp(req.getVersion(), HttpResponse::k200Ok
               , "OK", true, "application/json", successBody, successBody.length(), resp);
}

// 获取后台数据
void GomokuServer::getBackendData(const HttpRequest& req, HttpResponse* resp)
{
    try
    {
        int curOnline = getCurrentOnline();
        LOG_INFO << "当前在线人数:" << curOnline;

        int maxOnline = getMaxOnline();
        LOG_INFO << "最高在线人数:" << maxOnline;

        int totalUsers = getUserTotal();
        LOG_INFO << "总用户数:" << totalUsers;

        // 构造json响应
        // json响应是用来返回给客户端的数据，客户端可以根据这个数据来更新页面
        nlohmann::json respBody;
        respBody = {
            {"currentOnline", curOnline},
            {"maxOnline", maxOnline},
            {"totalUsers", totalUsers}
        };

        // 转换成字符串
        std::string respBodyStr = respBody.dump(4); // 4表示缩进4个空格
        resp->setStatusLine(HttpResponse::k200Ok, "OK", req.getVersion());
        resp->setContentType("application/json");
        resp->setBody(respBodyStr);
        resp->setContentLength(respBodyStr.length());
        resp->setCloseConnection(false);

        LOG_INFO << "返回的json响应:" << respBodyStr;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR << "获取后台数据失败:" << e.what();
        // 构建错误响应
        nlohmann::json errorBody =
        {
            {"error", "Internal Server Error"},
            {"message", e.what()}
        };
        std::string errorBodyStr = errorBody.dump(4);
        resp->setStatusLine(HttpResponse::k500InternalServerError, "Internal Server Error", req.getVersion());
        resp->setContentType("application/json");
        resp->setBody(errorBodyStr);
        resp->setContentLength(errorBodyStr.length());
        resp->setCloseConnection(true);
    }
    
}


void GomokuServer::packageResp(const std::string &version,
                 http::HttpResponse::HttpStatusCode statusCode,
                 const std::string &statusMessage,
                 bool close,
                 const std::string &body,
                 const std::string &contentType,
                 int contentlength,
                 http::HttpResponse *resp)
{
    if (resp == nullptr)
    {
        LOG_ERROR << "resp is nullptr";
        return;
    }
    try
    {
        resp->setStatusLine(statusCode, statusMessage, version);
        resp->setContentType(contentType);
        resp->setStatusCode(statusCode);
        resp->setBody(body);
        resp->setContentLength(contentlength);
        resp->setStatusMessage(statusMessage);
        resp->setCloseConnection(close);

        LOG_INFO << "resp panckage successfully";
    }
    catch(const std::exception& e)
    {
        LOG_ERROR << "resp panckage failed:" << e.what();
        // 构建错误响应
        resp->setStatusLine(HttpResponse::k500InternalServerError, "Internal Server Error", version);
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Server Error");
        resp->setCloseConnection(true);
    }
    
}