#include "net/TcpServer.h"
#include "http/HttpHandler.h"
#include <sys/socket.h>	//socket依赖
#include <netinet/in.h>	//协议族依赖
#include <unistd.h>	//线程、文件依赖
#include <cstring>	//字符串操作依赖
#include <iostream>
#include"utils/Logger.h"//cout、cerr替换

int main() {
  Logger::getInstance().setLogFile("/home/archer/projects/cpp_socket_http_server/logs/http_server.log");	//指定日志持久化存储路径与文件名
  try {
    TcpServer tcp_server;	//创建TCP服务实例，自动调用构造函数初始化
    LOG_INFO("HTTP多进程并发服务器启动成功，监听8080端口");
    tcp_server.start();	//启动并发服务主循环
  } catch(const std::exception& e) {
    LOG_ERROR("服务器启动失败: " + std::string(e.what()));  
    return 1;
  }
  return 0;
}
