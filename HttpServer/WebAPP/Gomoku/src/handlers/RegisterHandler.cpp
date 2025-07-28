#include "../../include/handlers/RegisterHandler.h"


void RegisterHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    json parsed = json::parse(req.getBody());
    // 获取用户名和密码
    std::string username = parsed["username"];
    std::string password = parsed["password"];

    // 分配id
    int userId = insertUser(username, password);
    if (userId == -1) //用户名已存在，注册失败
    {
        json errResp;
        errResp["status"] = "error";
        errResp["message"] = "username already exists";
        std::string errorBody = errResp.dump(4); // 将json对象转换为字符串
        // 构建响应返回错误信息
        server_->packageResp(req.getVersion(), http::HttpResponse::k409Conflict
                   , "Confilct", false, "application/json", errorBody, errorBody.length(), resp);
        return;
    }
    json successResp;
    successResp["status"] = "success";
    successResp["message"] = "register success";
    successResp["userId"] = userId;
    std::string successBody = successResp.dump(4); // 将json对象转换为字符串

    // 构建响应返回成功信息
    server_->packageResp(req.getVersion(), http::HttpResponse::k200Ok
                  , "OK", false, "application/json", successBody, successBody.length(), resp);
    return;
}

bool RegisterHandler::isUserExit(const std::string& username)
{
    std::string sql = "SELECT * FROM users WHERE username = '" + username + "'";
    sql::ResultSet* res = mysql_.executeQuery(sql);
    if (res->next())
    {
        return true;
    }
    return false;
}

int RegisterHandler::insertUser(const std::string& username, const std::string& password)
{
    if (!isUserExit(username))
    {
        // 插入用户
        std::string sql = "INSERT INTO users (username, password) VALUES ('" + username + "', '" + password + "')";
        mysql_.executeUpdate(sql);
        // 获取用户id
        sql = "SELECT id FROM users WHERE username = '" + username + "'";
        sql::ResultSet* res = mysql_.executeQuery(sql);
        if (res->next())
        {
            return res->getInt("id");
        }
    }
    return -1;
}