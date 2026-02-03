#include "net/TcpServer.h"
#include "http/HttpHandler.h"
#include <sys/socket.h>	//socket依赖
#include <netinet/in.h>	//协议族依赖
#include <unistd.h>	//线程、文件依赖
#include <cstring>	//字符串操作依赖
#include <iostream>
#include"utils/Logger.h"//cout、cerr替换
#include<string>	//字符串拼接

int main(int argc,char *argv[]) {
  Logger::getInstance().setLogFile("/home/archer/projects/cpp_socket_http_server/logs/http_server.log");
  try {
    TcpServer tcp_server;	//创建TCP服务实例，自动调用构造函数初始化
     
    //用命令行判断是否传入两个参数，若传入的参数中包含select，则打开select模式
    if(argc >= 2 && std::string(argv[1])=="select") {
      tcp_server.setMode(TcpServer::Mode::SELECT);
      std::string log_msg = "HTTP Select模式服务器启动成功，监听8080端口";
      LOG_INFO(log_msg.c_str());
    } else {
        std::string log_msg = "HTTP多进程并发服务器启动成功，监听8080端口";
        LOG_INFO(log_msg.c_str());
    }
    tcp_server.start();	//启动并发服务主循环
  } catch(const std::exception& e) {
    std::string err_msg = "服务器启动失败: " + std::string(e.what());
    LOG_ERROR(err_msg.c_str()); // 转为const char*，匹配宏参数要求    
    return 1;
  }
  return 0;
}
