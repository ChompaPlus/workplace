#include <string>
#include <iostream>
#include <muduo/net/TcpServer.h>
#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include "../include/GomokuServer.h"

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  
  std::string serverName = "HttpServer";
  int port = 8080;
  
  // 参数解析
  // p:port
  // 例如:./HttpServer -p 8080
  
  int opt;
  const char* str = "p:"; // p:表示p后面需要跟一个参数
  while ((opt = getopt(argc, argv, str)) != -1) // 解析命令行参数
  {
    switch (opt)
    {
      case 'p':
      {
        port = atoi(optarg);
        break;
      }
      default:
        break;
    }
  }

  // 初始化数据库连接池
  http::MySqlUtil::init("tcp://127.0.0.1:3306", "root", "root", "Gomoku", 10);
  
  muduo::Logger::setLogLevel(muduo::Logger::INFO);
  GomokuServer server(port, serverName);
  server.setThreadNum(4);
  server.start();
}
