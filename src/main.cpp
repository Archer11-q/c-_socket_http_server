#include "net/TcpServer.h"
#include "http/HttpHandler.h"
#include <sys/socket.h>	//socket依赖
#include <netinet/in.h>	//协议族依赖
#include <unistd.h>	//线程、文件依赖
#include <cstring>	//字符串操作依赖
#include <iostream>
#include"utils/Logger.h"//cout、cerr替换
#include<string>	//字符串拼接
#include"db/DB.hpp"
#include<ctime>   //设置启动时间

int main(int argc,char *argv[]) {
  Logger::getInstance().setLogFile("/home/archer/projects/cpp_socket_http_server/logs/http_server.log");

  HttpHandler::initServerStartTime(); //记录服务器启动时间

  //数据库初始化
  LOG_INFO("系统启动，开始初始化数据库连接");
  bool db_ok=DB::instance().connect(
    "127.0.0.1",
    "root",
    "123456",
    "http_server_db"
  ); 
  if(!db_ok){
    LOG_ERROR("数据库连接失败，程序退出");
    return 1;
  }
  LOG_INFO("数据库初始化完成，可以处理用户注册/登录请求");


  try {
    ThreadPool pool(8); //创建线程池实例，指定线程数量
    TcpServer tcp_server(&pool);	//创建TCP服务实例，自动调用构造函数初始化
     
    //用命令行判断是否传入两个参数，若传入的参数中包含select，则打开select模式
    if(argc >= 2 && std::string(argv[1])=="select") {
      tcp_server.setMode(TcpServer::Mode::SELECT);
      std::string log_msg = "HTTP Select模式服务器启动成功，监听8080端口";
      LOG_INFO(log_msg.c_str());
    } else if(argc >= 2 && std::string(argv[1])=="epoll") {
        tcp_server.setMode(TcpServer::Mode::EPOLL);
        std::string log_msg = "HTTP Epoll+ET模式服务器启动成功，监听8080端口";
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
