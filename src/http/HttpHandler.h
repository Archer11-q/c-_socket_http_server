#ifndef HTTPHANDLER_H
#define HTTPHANDLER_

#include <string>
#include <cstdint>
#include <sys/socket.h>  
#include <unistd.h>
#include <iostream>

//HTTP相关常量
const uint32_t BUF_SIZE = 2048; // 接收缓冲区大小

//处理单个客户端HTTP请求（TCP模块传client_fd进来）
void handle_client(int client_fd);
//解析HTTP请求路径
std::string parse_http_path(const std::string& request);
//根据路径，构建HTTP响应
std::string build_http_response(const std::string& path);

#endif // HTTPHANDLER_H
