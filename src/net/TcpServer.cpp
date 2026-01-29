#include "TcpServer.h"
#include "http/HttpHandler.h"	//仅在.cpp文件引入头文件，避免重复包含
#include "../utils/Logger.h"	//替换cerr,perror

//处理僵尸进程
void sigchld_handler(int sig) {
  (void)sig;	//屏蔽未使用参数警告
  while(waitpid(-1,nullptr,WNOHANG)>0);	//循环回收僵尸进程
}

//构造函数：实现原tcp_server_init的逻辑，初始化并赋值server_fd
TcpServer::TcpServer() : server_fd(-1) {
    // 1. 创建Socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOG_ERROR("Socket创建失败");
        exit(EXIT_FAILURE);  //退出函数调用
    }

     // 2. 端口复用（优化，避免端口占用问题）
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));  //地址+端口复用

    // 3. 配置服务端地址
    struct sockaddr_in server_addr;  //简化地质结构初始化
    memset(&server_addr, 0, sizeof(server_addr));       //清空数据，避免随机值干扰
    server_addr.sin_family = AF_INET;   //IPv4
    server_addr.sin_port = htons(PORT); //转化为网络字节序
    server_addr.sin_addr.s_addr = INADDR_ANY; //监听所有端口

    // 4. 绑定端口
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("Socket绑定端口失败");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 5. 开始监听
    if (listen(server_fd, BACKLOG) < 0) {
        LOG_ERROR("Socket监听端口失败");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 注册信号
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_handler=sigchld_handler;
    sa.sa_flags=SA_RESTART;
    sigaction(SIGCHLD,&sa,nullptr);

    LOG_INFO("【TCP Server】初始化成功，监听端口");
}

//析构函数: 释放socket资源
TcpServer::~TcpServer() {
  if(server_fd>=0) {
    close(server_fd);
    std::cout<<"【Tcp Server】socket资源已释放"<<std::endl;
  }
}

//启动服务主循环，实现fork多进程并发
void TcpServer::start() {
  while(true) {
    //接受客户端连接：忽略客户端地址信息
    int client_fd=accept(server_fd,nullptr,nullptr);
    if(client_fd<0) {
      LOG_SYS_ERROR("accept failed");
      continue;
    }

    //创建子进程: 复制父进程的地址空间，实现并发处理
    pid_t pid=fork();
    if(pid<0) {	//fork失败：释放客户端fd，继续接收下一个连接
      LOG_SYS_ERROR("fork failed");
      close(client_fd);
      continue;
    }

    if(pid==0) {	//子进程：专门处理当前客户端的HTTP请求
      close(server_fd);	//子进程不需要监听socket, 避免文件描述符泄漏
      HttpHandler handler;
      handler.handleRequest(client_fd);	//处理客户端请求
      close(client_fd);	//处理完毕，关闭客户端fd
      exit(0);	//避免子进程进入accept循环
    } else {	//父进程：继续接收新的客户端连接，不处理具体请求
        close(client_fd);    //父进程已将客户端fd复制给子进程，自身关闭避免泄漏
	//非阻塞回收僵尸进程：WNOHANG表示无结束的子进程则立即返回，不阻塞主循环
	while(waitpid(-1,nullptr,WNOHANG)>0);
    }
  }
}
