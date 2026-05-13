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
#include<sys/epoll.h>	//epoll依赖
#include<fcntl.h>	//fcntl依赖
#include<errno.h>	//errno依赖
#include<ctime>   //超时管理的时间依赖
#include<unordered_map> //哈希表依赖
#include"../utils/ThreadPool.h" //线程池依赖
#include<gtest/gtest.h>


//前置声明: 避免头文件重复包含，仅声明类不引入头文件
class HttpHandler;

// 全局常量移至网络模块（TCP相关）
const uint16_t PORT = 8080;
const int BACKLOG = 5; // listen监听队列长度

class TcpServer {
public:
  //构造函数：初始化TCP服务（socket、bind、listen）
  TcpServer(ThreadPool* thread_pool);
  //启动服务主循环，实现fork多进程并发
  void start();
  //析构函数：关闭socket，释放资源
  ~TcpServer();
  //IO模式枚举【FORK/SELECT/EPOLL】
  enum class Mode {FORK,SELECT,EPOLL};
  //设置运行模式的接口
  void setMode(Mode mode) {mode_=mode;}

private:
  friend class BufferPoolTest_AcquireRelease_Test; //测试BufferPool的AcquireRelease功能
  friend class BufferPoolTest_ReuseBuffer_Test; //测试BufferPool的ReuseBuffer功能
  friend class BufferPoolTest_MultipleAcquireRelease_Test; //测试BufferPool的MultipleAcquire

  //连接信息结构体：存储fd最后活跃时间
  struct ConnectionInfo {
    time_t last_active; //跟踪每个连接空闲时长，判断是否超时
  };
  //新增简单内存池类：减少Epoll模式下频繁创建/销毁缓冲区带来的内存分配开销
  class BufferPool {
  public:
    std::vector<char> acquire();              //获取缓冲区
    void release(std::vector<char>&& buffer); //归还缓冲区

  private:
    std::vector<std::vector<char>> pool_; //缓冲区池：存储可复用的char缓冲区
    static constexpr size_t buffer_size_=8192;  //固定缓冲区大小为8KB
  };
  
  //类内成员变量
  int server_fd;
  int epoll_fd_;            
  Mode mode_=Mode::FORK;    //模式成员变量，默认FORK模式
  epoll_event* ready_events_;  
  std::unordered_map<int,ConnectionInfo> connections_;  //连接管理容器：存储所有客户端fd及活跃时间
  static const int MAX_IDLE_TIME =30; //30s空闲超时
  static const int EPOLL_CHECK_INTERVAL=5000; //5s检查间隔
  BufferPool buffer_pool_;  //内存池实例
  ThreadPool* thread_pool_; //线程池指针
  std::unordered_map<int,std::string> client_buffers_; //客户端数据缓存：解决粘包问题，存储每个fd未处理完的数据

  //声明select模式的核心方法
  void startWithSelect();
  //重构：原有fork逻辑迁移到该方法
  void startWithFork();
  //声明Epoll模式的核心方法
  void startWithEpoll();
  //超时检查
  void checkIdleConnections();
  //连接关闭
  void closeConnection(int fd);
};


#endif 	
