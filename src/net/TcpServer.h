#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <sys/socket.h>	//socket依赖
#include <netinet/in.h>	//协议族依赖
#include <unistd.h>	//线程、文件依赖
#include <cstdint>	//固定宽位整数
#include <cstring>
#include <iostream>
#include<sys/wait.h>	//wait依赖
#include<signal.h>	//信号处理依赖
#include "../utils/Logger.h"    //替换cerr,perror
#include<sys/select.h>	//select依赖
#include<vector>	//vector依赖
#include<algorithm>	

//前置声明: 避免头文件重复包含，仅声明类不引入头文件
class HttpHandler;

// 全局常量移至网络模块（TCP相关）
const uint16_t PORT = 8080;
const int BACKLOG = 5; // listen监听队列长度

class TcpServer {
public:
  //构造函数：初始化TCP服务（socket、bind、listen）
  TcpServer();
  //启动服务主循环，实现fork多进程并发
  void start();
  //析构函数：关闭socket，释放资源
  ~TcpServer();
  //IO模式枚举【FORK/SELECT】
  enum class Mode {FORK,SELECT};
  //设置运行模式的接口
  void setMode(Mode mode) {mode_=mode;}
private:
  int server_fd;
  //模式成员变量，默认FORK模式
  Mode mode_=Mode::FORK;
  //声明select模式的核心方法
  void startWithSelect();
  //重构：原有fork逻辑迁移到该方法
  void startWithFork();
};

#endif 	
