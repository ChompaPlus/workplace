#include "../../include/handlers/LoginHandler.h"

void LoginHandler::handle(const http::HttpRequest &req, http::HttpResponse *resp)
{
    // 处理登录逻辑
    // 验证 contentType
    auto contentType = req.getHeader("Content-Type");
    if (contentType.empty() || contentType != "application/json" || req.getBody().empty())
    {
        LOG_INFO << "content" << req.getBody();
        resp->setStatusLine(http::HttpResponse::k400BadRequest, "Bad Request",req.getVersion());
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(0);
        resp->setBody("");
        return;
    }

    // JSON 解析使用 try catch 捕获异常
    try
    {
        json parsed = json::parse(req.getBody());
        std::string username = parsed["username"];
        std::string password = parsed["password"];
        // 验证用户是否存在
        int userId = queryUser(username, password);
        if (userId != -1)
        {
            // 获取会话
            auto session = server_->getSessionManager()->getSession(req, resp);
            // 会话都不是同一个会话，因为会话判断是不是同一个会话是通过请求报文中的cookie来判断的
            // 所以不同页面的访问是不可能是相同的会话的，只有该页面前面访问过服务端，才会有会话记录
            // 那么判断用户是否在其他地方登录中不能通过会话来判断
            
            // 在会话中存储用户信息
            session->setValue("userId", std::to_string(userId));
            session->setValue("username", username);
            session->setValue("isLoggedIn", "true");
            if (server_->online_users.find(userId) == server_->online_users.end() || server_->online_users[userId] == false)
            {
                {
                    std::lock_guard<std::mutex> lock(server_->mutexForOnlineUsers_);
                    server_->online_users[userId] = true;
                }
                
                // 更新历史最高在线人数
                server_->updateMaxOnline(server_->online_users.size());
                // 用户存在登录成功
                // 封装json 数据。
                json successResp;
                successResp["success"] = true;
                successResp["userId"] = userId;
                std::string successBody = successResp.dump(4);

                resp->setStatusLine(http::HttpResponse::k200Ok, "OK",req.getVersion());
                resp->setCloseConnection(false);
                resp->setContentType("application/json");
                resp->setContentLength(successBody.size());
                resp->setBody(successBody);
                return;
            }
            else
            {
                // FIXME: 当前该用户正在其他地方登录中，将原有登录用户强制下线更好
                // 不允许重复登录，
                json failureResp;
                failureResp["success"] = false;
                failureResp["error"] = "账号已在其他地方登录";
                std::string failureBody = failureResp.dump(4);

                resp->setStatusLine(http::HttpResponse::k403Forbidden, "Forbidden",req.getVersion());
                resp->setCloseConnection(true);
                resp->setContentType("application/json");
                resp->setContentLength(failureBody.size());
                resp->setBody(failureBody);
                return;
            }
        }
        else // 账号密码错误，请重新登录
        {
            // 封装json数据
            json failureResp;
            failureResp["status"] = "error";
            failureResp["message"] = "Invalid username or password";
            std::string failureBody = failureResp.dump(4);

            resp->setStatusLine(http::HttpResponse::k401Unauthorized, "Unauthorized",req.getVersion());
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(failureBody.size());
            resp->setBody(failureBody);
            return;
        }
    }
    catch (const std::exception &e)
    {
        // 捕获异常，返回错误信息
        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = e.what();
        std::string failureBody = failureResp.dump(4);

        resp->setStatusLine(http::HttpResponse::k400BadRequest, "Bad Request", req.getVersion());
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
        return;
    }
}

// void LoginHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
// {
//     // 验证请求类型
//     auto contentType = req.getHeader("Content-Type");
//     // 该handler处理的是post请求的业务逻辑，所以一定是有请求体的
//     if (contentType != "application/json" || contentType.empty() || req.getBody().empty())
//     {
//         LOG_WARN << "请求体为空";
//         json errorResp;
//         errorResp["status"] = "error";
//         errorResp["message"] = "Invalid request body";
//         // 转换成字符串
//         std::string errorRespStr = errorResp.dump(4); // 4：缩进4个空格
//         server_->packageResp(req.getVersion(), http::HttpResponse::HttpStatusCode::k400BadRequest
//                        , "Invalid request body", true, errorRespStr, "application/json", errorRespStr.size(), resp);
//         return;
        
//     }
//     try
//     {
//         // 解析请求体
//         json reqBody = json::parse(req.getBody());
//         std::string username = reqBody["username"];
//         std::string password = reqBody["password"];
//         int userId = queryUser(username, password);
//         if (userId!=-1)
//         {
//             // 创建会话
//             auto session = server_->getSessionManager()->getSession(req,resp);
//             session->setValue("userId", std::to_string(userId));
//             session->setValue("username", username);
//             session->setValue("isLoggedIn", "true");

//             // 记录在线用户
//             if (server_->online_users.find(userId)==server_->online_users.end() || server_->online_users[userId]==false)
//             {
//                 {
//                     std::lock_guard<std::mutex> lock(server_->mutexForOnlineUsers_);
//                     server_->online_users[userId] = true;
//                 }
//                 // 更新历史最高在线人数
//                 server_->updateMaxOnline(server_->online_users.size());

//                 // 构建响应返回成功信息
//                 json successResp;
//                 successResp["success"] = true;
//                 successResp["userId"] = userId;
//                 std::string successRespStr = successResp.dump(4); // 4：缩进4个空格
//                 server_->packageResp(req.getVersion(), http::HttpResponse::k200Ok
//                             , "OK", false, "application/json", successRespStr, successRespStr.size(), resp);
//                 return;
                
//             }
//             else
//             {
//                 json errorResp;
//                 errorResp["success"] = "false";
//                 errorResp["error"] = "账号已在其他地方登录";
//                 std::string errorRespStr = errorResp.dump(4); // 4：缩进4个空格
//                 server_->packageResp(req.getVersion(), http::HttpResponse::k403Forbidden
//                             , "Forbidden", true, errorRespStr, "application/json", errorRespStr.size(), resp);
//                 return;
//             }
//         }
//         else
//         {
//             // 账号密码错误
//             json errorResp;
//             errorResp["status"] = "error";
//             errorResp["message"] = "Invalid username or password";
//             std::string errorRespStr = errorResp.dump(4); // 4：缩进4个空格
//             server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized
//                           , "Unauthorized", true, errorRespStr, "application/json", errorRespStr.size(), resp);
//             return;
//         }
//     }
//     catch(const std::exception& e)
//     {
//         json errorResp;
//         errorResp["status"] = "error";
//         errorResp["message"] = e.what();
//         std::string errorRespStr = errorResp.dump(4); // 4：缩进4个空格
//         server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest
//                       , "Bad Request", true, errorRespStr, "application/json", errorRespStr.size(), resp);
//         return;
//     }
    

// }


int LoginHandler::queryUser(const std::string& username, const std::string& password)
{
    // 连接数据库
    std::string sql = "SELECT * FROM users WHERE username = '" + username + "' AND password = '" + password + "'";
    sql::ResultSet* res = mysql_.executeQuery(sql);
    if (res->next())
    {
        return res->getInt("id");
    }
    return -1;
}