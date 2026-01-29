#ifndef TCPSERVER_H
#define TCPSERVER_

#include <sys/socket.h>	//socket依赖
#include <netinet/in.h>	//协议族依赖
#include <unistd.h>	//线程、文件依赖
#include <cstdint>	//固定宽位整数

// 全局常量移至网络模块（TCP相关）
const uint16_t PORT = 8080;
const int BACKLOG = 5; // listen监听队列长度

// 初始化TCP服务，返回服务端文件描述符
int tcp_server_init();

#endif 	
