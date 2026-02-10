#include "TcpServer.h"
#include "http/HttpHandler.h"	//仅在.cpp文件引入头文件，避免重复包含

//处理僵尸进程
void sigchld_handler(int sig) {
  (void)sig;	//屏蔽未使用参数警告
  while(waitpid(-1,nullptr,WNOHANG)>0);	//循环回收僵尸进程
}

//构造函数：实现原tcp_server_init的逻辑，初始化并赋值server_fd
TcpServer::TcpServer() : server_fd(-1), epoll_fd_(-1), ready_events_(nullptr) {
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
  if(epoll_fd_>=0) {
    close(epoll_fd_);
    LOG_INFO(("【Tcp Server】epoLl实例已释放，fd="+std::to_string(epoll_fd_)).c_str());
  }

  if(!connections_.empty()) {
    for(const auto& pair: connections_) {
      close(pair.first);
      LOG_INFO(("【Tcp Server】清理残留连接：fd="+std::to_string(pair.first)).c_str());
    }
    connections_.clear();
  }

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
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        LOG_SYS_ERROR("epoll_create1 failed");
        exit(EXIT_FAILURE);	//创建失败退出
    }

    //2.初始化监听fd的事件结构，注册EPOLLIN读事件
    epoll_event listen_ev;	//初始化监听fd的事件结构
    memset(&listen_ev, 0, sizeof(listen_ev));//初始化事件结构，避免脏数据
    listen_ev.data.fd = server_fd;	//绑定监听fd
    listen_ev.events = EPOLLIN;		//监听读事件

    //将监听fd添加到epoll内和事件列表
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd, &listen_ev);

    //3.定义就绪事件数组，存放epoll_wait返回的就绪fd事件
    epoll_event ready_events[1024];
    LOG_INFO("【Epoll模式】服务器启动，单次最大监听事件数：1024");

    //4.epoll主事件循环：持续检测就绪fd，不退出直到进程终止
    while (true) {
	    //阻塞等待就绪事件：设置5s超时，内核主动通知，无需轮询所有fd
      int ready_num = epoll_wait(epoll_fd_, ready_events, 1024, EPOLL_CHECK_INTERVAL);
      if (ready_num < 0) {	//被信号中断则继续循环
	      if (errno == EINTR)
	        continue;
        //异常：epoll_wait失败，退出循环
        LOG_SYS_ERROR("epoll_wait failed");
        break;
	    } else if(ready_num==0) { //超时
        checkIdleConnections(); //空闲连接检查
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
          connections_[client_fd]={time(nullptr)};  //初始化连接活跃时间

		      //初始化客户端fd的事件结构，注册EPOLLIN|EPOLLET
          epoll_event client_ev;
          memset(&client_ev, 0, sizeof(client_ev));
          client_ev.data.fd = client_fd;
          client_ev.events = EPOLLIN | EPOLLET;
		      //将客户端fd添加到epoll内核事件列表，监听读事件
          epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &client_ev);
          LOG_INFO(("【Epoll模式】新连接：fd=" + std::to_string(client_fd)).c_str());
        }
	      //7.处理客户端fd就绪：有HTTP请求数据到达
        else if (events & EPOLLIN) {	//读取数据
          if(connections_.count(fd))  //判断fd是否存在服务器连接池
            connections_[fd].last_active=time(nullptr); //更新最后活跃时间

          std::vector<char> buffer=buffer_pool_.acquire();//从内存中获取缓冲区
          std::string req_buffer; //缓存所有读取的数据，解决粘包
          ssize_t recv_len = 0;   //存储每次调用recv读取的字节数
          bool is_conn_close=false; //标记连接是否需要关闭

          //7.1循环读取数据：一次性读完所有可用数据
          while((recv_len=recv(fd,buffer.data(),buffer.size()-1,0))>0)          {
            req_buffer.append(buffer.data(),recv_len);//将读取的数据追加到缓存
            memset(buffer.data(),0,buffer.size());  //清空临时缓冲区
          }

          //7.2处理recv返回-1的情形
          if(recv_len==0) { //客户端主动关闭连接
            LOG_INFO(("【Epoll】fd="+std::to_string(fd)+" 客户端主动关闭连接").c_str());
            is_conn_close=true;
          } else if(recv_len<0) {
              if(errno!=EAGAIN && errno!=EWOULDBLOCK) {
                LOG_SYS_ERROR(("【Epoll】fd="+std::to_string(fd)+" recv failed").c_str());
                is_conn_close=true;
              }
              //EAGAIN/EWOULDBLOCK数据已读完，无需处理
          }

          //7.3解析并处理缓存中的HTTP请求：循环解析+解决粘包
          if(!is_conn_close && !req_buffer.empty()) {
            size_t offset=0;  //移动指针，跳过已读数据包，指向下一个数据包起始位置
            HttpHandler handler;

            while(offset<req_buffer.size()) { //循环检查是否读取完
              //调用HttpHandler接口，判断当前偏移后的数据是否是完整请求
              size_t remain_len=req_buffer.size()-offset;

              //处理单个完整HTTP请求，获取长连接状态
              bool keep_alive=false;
              handler.handleRequest(fd,req_buffer.c_str()+offset,remain_len,keep_alive);

              //找到当前请求的结束位置，更新偏移量
              size_t req_end=req_buffer.find("\r\n\r\n",offset)+4;
              offset=req_end;

              //短连接：处理完当前请求后，标记关闭，不再处理后续请求
              if(!keep_alive) {
                is_conn_close=true;
                break;
              }
            } //循环检查关闭

            //长连接/半包
            if(!is_conn_close)
              LOG_INFO(("【Epoll】fd="+std::to_string(fd)+" 长连接复用/等待半包数据").c_str());
          }   //退出处理HTTP请求判断
          
          //统一归还缓冲区
          buffer_pool_.release(std::move(buffer));

          //最终判断：是否关闭连接
          if(is_conn_close)  {
            closeConnection(fd); //关闭fd，自动移除connctions_
          }
        }     //退出读取事件判断
      }       //退出遍历所有就绪事件循环
    }         //退出epoll主循环
    //8.关闭epoll实例
    close(epoll_fd_);
    LOG_INFO("【Epoll模式】服务器停止，epoll实例已关闭");
}


//超时检查
void TcpServer::checkIdleConnections() {
  time_t now=time(nullptr);   //获取当前系统时间
  std::vector<int> to_close;  //暂存需要关闭的文件描述符

  //遍历连接容器，筛选超时连接
  for(const auto& pair:connections_) {
    if(now-pair.second.last_active > MAX_IDLE_TIME) {//当前空闲时间>预设空闲时间
      to_close.push_back(pair.first); //将该连接的fd添加到关闭列表中
      LOG_INFO(("连接超时，fd="+std::to_string(pair.first)).c_str());
    }
  }

  //批量关闭超时连接
  for(int fd:to_close)
    closeConnection(fd);
}


//连接关闭
void TcpServer::closeConnection(int fd) {
  if(fd<0) return;
  //检查fd是否在连接容器中
  if(connections_.find(fd)==connections_.end()) return ;

  //从监听列表移除超时的fd
  if(epoll_ctl(epoll_fd_,EPOLL_CTL_DEL,fd,nullptr)<0)
    LOG_ERROR(("epoll_ctl删除fd失败："+std::string(strerror(errno))).c_str());
  if(close(fd)<0)  //关闭超时fd
    LOG_ERROR(("关闭fd失败："+std::string(strerror(errno))).c_str());

  connections_.erase(fd);
  LOG_INFO(("【Epoll模式】连接关闭：fd="+std::to_string(fd)).c_str());
}

//获取缓冲区：池非空则取空闲缓冲区，否则新建
std::vector<char> TcpServer::BufferPool::acquire() {
  if(!pool_.empty()) {
    auto buffer=std::move(pool_.back());  //移动语义，避免拷贝
    pool_.pop_back(); //从池中移除move移动后的空缓冲区
    return buffer;
  }
  return std::vector<char>(buffer_size_);  //池空则新建8KB缓冲区
}


//归还缓冲区：校验大小合法后入池复用
void TcpServer::BufferPool::release(std::vector<char>&& buffer) {
  if(buffer.size()==buffer_size_) //仅回收8KB的缓冲区，避免非法数据混入
    pool_.push_back(std::move(buffer)); //移动语义，降低开销
}


