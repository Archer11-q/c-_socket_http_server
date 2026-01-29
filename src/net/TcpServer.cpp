#include "TcpServer.h"
#include <cstring>
#include <iostream>

int tcp_server_init() {
    // 1. 创建Socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Socket创建失败: " << strerror(errno) << std::endl;
        return -1;
    }

     // 2. 端口复用（优化，避免端口占用问题）
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));  //地址+端口复用

    // 3. 配置服务端地址
    struct sockaddr_in server_addr;  //简化地质结构初始化
    memset(&server_addr, 0, sizeof(server_addr));	//清空数据，避免随机值干扰
    server_addr.sin_family = AF_INET;	//IPv4
    server_addr.sin_port = htons(PORT); //转化为网络字节序
    server_addr.sin_addr.s_addr = INADDR_ANY; //监听所有端口

    // 4. 绑定端口
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Socket绑定失败: " << strerror(errno) << std::endl;
        close(server_fd);
        return -1;
    }

    // 5. 开始监听
    if (listen(server_fd, BACKLOG) < 0) {
        std::cerr << "Socket监听失败: " << strerror(errno) << std::endl;
        close(server_fd);
        return -1;
    }

    std::cout << "【TCP Server】初始化成功，监听端口: " << PORT << std::endl;
    return server_fd;
}
