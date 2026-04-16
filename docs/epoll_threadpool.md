#Epoll ET + 线程池协作机制
##一、项目实现方案

###架构流程
1.**主线程（Epoll事件循环）**
- 采用**边缘触发**模式，通过epoll_wait统一监听所有文件描述符的EPOLLIN可读事件
- 客户端可读事件触发后，循环调用recv读取全部数据，直到返回EAGAIN/EWOULDBLOCK，满足ET模式使用要求
- 对读取的数据进行拷贝封装，将任务提交到线程池，不阻塞主线程的IO监听
2.**工作线程（线程池）**
- 从任务队列中获取请求任务，执行HTTP协议解析、响应构造等业务逻辑
- 通过循环send发送完整响应数据，避免报文截断问题
- 根据长连接配置判断是否关闭客户端连接

###核心代码实现
1.**主线程Epoll事件处理**
//客户端fd注册
epoll_event client_ev;
menset(&client_ev,0,sizeof(client_ev));
client_ev.data.fd=client_fd;
client_ev.events=EAGAIN | EPOLLET;
epoll_ctl(epoll_fd_,EPOLL_CTL_ADD,client_fd,&client_ev);

//可读事件处理
else if(events & EPOLLIN){
    //ET模式循环读取数据
    while((recv_len=recv(fd,buffer.data(),buffer.size()-1,0))>0){
        req_buffer.append(buffer.data(),recv_len);
    }
    //数据拷贝后提交给线程池
    thread_pool_->enqueue([this,fd,data=std::string(req_buffer.c_str()+offset,remain_len)]()
    {
        HttpHandler handler;
        bool keep_alive=false;
        //执行业务处理+数据发送
        handler.handleRequest(fd,data.c_str(),data.size(),keep_alive);
        if(!keep_alive)
            closeConnection(fd);
    });
}

2.**工作线程循环发送响应**
//工作线程内执行：循环发送响应，避免数据截断
size_t sent=0;
size_t total=response.size();
//未发送完成则持续发送
while(snet<total){
    ssize_t ret=send(client_fd,response.c_str()+sent,total-sent,0);
    if(ret<0){
        LOG_ERROR("发送响应失败，fd= " + std::to_string(client_fd));
        keep_alive = false;
        return;
    }   
    sent+=ret;
}

###二、模型核心特点
1.采用单Reactor单进程多线程模型，主线程专注IO事件监听与数据读取，工作线程专注业务处理与数据发送
2.客户端套接字使用 EPOLLIN | EPOLLET 边沿触发模型
3.数据通过拷贝传递线程池，保证任务传递的线程安全
4.工作线程直接操作客户端fd完成响应发送，实现IO操作与业务逻辑的异步分离

###三、边缘触发使用注意事项
1.可读事件触发时，必须循环读取数据直至recv返回 EAGAIN/EWOULDBLOCK，保证数据读取完整
2.客户端套接字设置为非阻塞模式，配合ET模式避免阻塞主线程
3.响应发送采用循环send机制，解决大报文发送截断问题

###四、存在问题&改进方向
1.**多线程并发操作同一fd风险**
- 主线程负责recv，工作线程负责send/close，同一个client_fd被多线程同时操作，存在竞争与崩溃风险

2.**无EPOLLONESHOT导致重复触发
- ET模式下，客户端大量数据可能使同一个fd多次触发事件，被多次提交到线程池，造成请求错乱

3.**改进方案**
- 引入EPOLLONESHOT，保证同一时刻只有一个线程处理同一个fd
- 工作线程处理完后注册Epoll事件
- 改为主线程同一负责所有IO（recv+send），工作线程只做纯业务计算，从根源避免fd竞争
- 增加fd生命周期安全管理，防止跨线程非法关闭