#include "TcpServer.h"
#include "http/HttpHandler.h"	//仅在.cpp文件引入头文件，避免重复包含

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
    struct sockaddr_in server_addr;  //简化地址结构初始化
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
    LOG_INFO("【Tcp Server】socket资源已释放");
  }
}

//启动服务主循环，实现fork多进程并发
void TcpServer::start() {
  //根据模式选择不同的启动逻辑
  if(mode_==Mode::SELECT) 
    startWithSelect();
  else if(mode_==Mode::EPOLL)
    startWithEpoll(); 
  else
    startWithFork();
}

void TcpServer::startWithFork() {
  while(true) {
    //接受客户端连接：忽略客户端地址信息
    int client_fd=accept(server_fd,nullptr,nullptr);
    if(client_fd<0) {
      LOG_SYS_ERROR("accept failed");
      continue;
    }

    //创建子进程: 复制父进程的地址空间，实现并发处理
    pid_t pid=fork();
    if(pid<0) { //fork失败：释放客户端fd，继续接收下一个连接
      LOG_SYS_ERROR("fork failed");
      close(client_fd);
      continue;
    }

    if(pid==0) {        //子进程：专门处理当前客户端的HTTP请求
      close(server_fd); //子进程不需要监听socket, 避免文件描述符泄漏
      HttpHandler handler;
      handler.handleRequest(client_fd); //处理客户端请求
      close(client_fd); //处理完毕，关闭客户端fd
      exit(0);  //避免子进程进入accept循环
    } else {    //父进程：继续接收新的客户端连接，不处理具体请求
        close(client_fd);    //父进程已将客户端fd复制给子进程，自身关闭避免泄漏
        //非阻塞回收僵尸进程：WNOHANG表示无结束的子进程则立即返回，不阻塞主循环
        while(waitpid(-1,nullptr,WNOHANG)>0);
    }
  }
}

//select模式核心实现
void TcpServer::startWithSelect() {
  std::vector<int> client_fds;	//存储所有已连接的客户端fd
  fd_set read_fds;		//select监听的读事件集合
  int max_fd=server_fd;		//初始化监听上限

  std::string select_start_msg = "【Select模式】服务器启动，最大连接数限制：" + std::to_string(FD_SETSIZE);
  LOG_INFO(select_start_msg.c_str());

  while(true) {
    //1.每次循环重置fd集合
    FD_ZERO(&read_fds);	//初始化时清空读事件集合，避免随机值干扰
    FD_SET(server_fd,&read_fds);//将监听fd加入集合
    max_fd=server_fd;	//每次循环重置max_Fd，避免无效值占用资源

    //将所有已连接的客户端fd加入监听集合
    for(int fd:client_fds) {
	FD_SET(fd,&read_fds);
	if(fd>max_fd)
	  max_fd=fd;	//更新监听上限
    }

    //2.阻塞等待事件，仅监听读事件,无超时
    int ready=select(max_fd+1,&read_fds,NULL,NULL,NULL);
    if(ready<0) {
	LOG_SYS_ERROR("select failed");
	continue;
    }

    //3.处理新连接，监听fd就绪
    if(FD_ISSET(server_fd,&read_fds)) {
	int client_fd=accept(server_fd,nullptr,nullptr);//简化客户端地址结构初始化
	if(client_fd<0) {
	  LOG_SYS_ERROR("accept failed in select mode");
	  continue;
	}
	//新连接加入客户端列表
	client_fds.push_back(client_fd);
	std::string new_conn_msg = "新连接：fd=" + std::to_string(client_fd) + 
                           "(当前总连接数: " + std::to_string(client_fds.size()) + ")";
	LOG_INFO(new_conn_msg.c_str());
    }

    //4.处理客户端连接，客户端fd就绪
    for(size_t i=0;i<client_fds.size();) {    
	int fd=client_fds[i];
	//判断该客户端fd是否有可读事件
	if(FD_ISSET(fd,&read_fds)) {
	  char buffer[2048]={0};
	  ssize_t n=recv(fd,buffer,sizeof(buffer)-1,0);

	  if(n>0) {
	    //调用适配后的HttpHandler接口，接收已读取的buffer
	    HttpHandler handler;
	    handler.handleRequest(fd,buffer,n);
	  } else {	//客户端端口/读取失败：关闭fd并从列表中删除
		close(fd);
		client_fds.erase(client_fds.begin()+i);
		std::string close_conn_msg = "连接关闭：fd=" + std::to_string(fd) +"(剩余连接数: " + std::to_string(client_fds.size()) + ")";
		LOG_INFO(close_conn_msg.c_str());
		continue;
	  }
	}
	  //仅当未删除元素时，使用递增让指针指向下一个就绪fd
	  i++;
    }
  }
}

//Epoll模式单函数实现
void TcpServer::startWithEpoll() {
    //1.创建epoll实例，内核创建事件表，返回epoll文件描述符
    int epoll_fd = epoll_create1(0);	
    if (epoll_fd < 0) { 
        LOG_SYS_ERROR("epoll_create1 failed"); 
        exit(EXIT_FAILURE);	//创建失败退出
    }

    //2.初始化监听fd的事件结构，注册EPOLLIN读事件
    epoll_event listen_ev;	//初始化监听fd的事件结构
    memset(&listen_ev, 0, sizeof(listen_ev));//初始化事件结构，避免脏数据
    listen_ev.data.fd = server_fd;	//绑定监听fd
    listen_ev.events = EPOLLIN;		//监听读事件

    //将监听fd添加到epoll内和事件列表
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &listen_ev);

    //3.定义就绪事件数组，存放epoll_wait返回的就绪fd事件
    epoll_event ready_events[1024];
    LOG_INFO("【Epoll+ET模式】服务器启动，单次最大监听事件数：1024");

    //4.epoll主事件循环：持续检测就绪fd，不退出直到进程终止
    while (true) {
	//阻塞等待就绪事件：永久阻塞，内核主动通知，无需轮询所有fd
        int ready_num = epoll_wait(epoll_fd, ready_events, 1024, -1);
        if (ready_num < 0) {	//被信号中断则继续循环 
	    if (errno == EINTR) 
	        continue;
	 }
	
	//5.遍历所有就绪事件，处理每个就绪fd
        for (int i = 0; i < ready_num; ++i) {
            int fd = ready_events[i].data.fd;	//获取当前就绪fd
            uint32_t events = ready_events[i].events;//获取当前fd的就绪事件类型
	    
	    //6.处理监听fd就绪，有新的客户端TCP连接建立
            if (fd == server_fd) {
		//接收连接+设置fd为非阻塞
                int client_fd = accept4(server_fd, nullptr, nullptr, SOCK_NONBLOCK);
                if (client_fd < 0) continue;
		
		//初始化客户端fd的事件结构，注册EPOLLIN|EPOLLET
                epoll_event client_ev;
                memset(&client_ev, 0, sizeof(client_ev));
                client_ev.data.fd = client_fd;
                client_ev.events = EPOLLIN | EPOLLET;	
		//将客户端fd添加到epoll内核事件列表，监听读事件
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev);
                LOG_INFO(("【Epoll模式】新连接：fd=" + std::to_string(client_fd)).c_str());
            }
	    //7.处理客户端fd就绪：有HTTP请求数据到达
            else if (events & EPOLLIN) {	//读取数据
                char buffer[4096] = {0};
                ssize_t n = recv(fd, buffer, 4095, 0);
		 
		//8.读取到有效数据，调用HttpHandler处理请求
                if (n > 0) {
                    LOG_INFO(("【Epoll】fd=" + std::to_string(fd) + " 开始处理HTTP请求").c_str());
                    HttpHandler handler;
                    handler.handleRequest(fd, buffer, n);
                    LOG_INFO(("【Epoll】fd=" + std::to_string(fd) + " HTTP请求处理完成").c_str());
                }

		//9.HTTP短连接：处理完一次请求，立即删除事件，关闭fd
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                close(fd);
                LOG_INFO(("【Epoll模式】连接关闭：fd=" + std::to_string(fd)).c_str());
            }
        }
    }
    close(epoll_fd);
}
