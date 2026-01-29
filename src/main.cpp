#include "net/TcpServer.h"
#include "http/HttpHandler.h"
#include <sys/socket.h>	//socket依赖
#include <netinet/in.h>	//协议族依赖
#include <unistd.h>	//线程、文件依赖
#include <cstring>	//字符串操作依赖
#include <iostream>

int main() {
    //1. 调用网络模块，初始化TCP服务
    int server_fd = tcp_server_init();
    if (server_fd < 0) {
        return 1;
    }
    std::cout << "【HTTP Server】多模块版启动成功，监听8080端口" << std::endl;
 
    //2.主循环：接收连接，调用HTTP模块处理
    struct sockaddr_in client_addr;  //创建客户端地址结构
    socklen_t addr_len=sizeof(client_addr); //获取地址结构长度
    while(true) {
      int client_fd=accept(server_fd,(struct sockaddr*)&client_addr,&addr_len);
      if (client_fd < 0) {
            perror("accept failed");
            continue;
      }
      // 调用HTTP模块的处理函数，TCP/HTTP彻底解耦
      handle_client(client_fd);
      close(client_fd);
    }

    close(server_fd);
    return 0;
}
