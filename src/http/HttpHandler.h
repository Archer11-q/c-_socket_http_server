#ifndef HTTPHANDLER_H
#define HTTPHANDLER_

#include <string>
#include <cstdint>
#include <sys/socket.h>  
#include <unistd.h>
#include <iostream>
#include<fstream>  //文件操作依赖
#include<sstream>  //MIME类型解析依赖

//HTTP相关常量
const uint32_t BUF_SIZE = 4096; // 接收缓冲区大小

//HttpHandler类框架
class HttpHandler {
public:
  //处理单个客户端HTTP请求（TCP模块传client_fd进来）
  void handleRequest(int client_fd);
  //解析HTTP请求路径
  std::string parse_http_path(const std::string& request);
  //根据路径，构建HTTP响应
  std::string build_http_response(const std::string& path);
  //向客户端发送指定路径对应静态文件，判断是否发送成功
  bool serverStaticFile(int client_fd,const std::string& path);
  //获取文件路径后缀名
  std::string getMimeType(const std::string& file_path);
private:

};

#endif // HTTPHANDLER_H
