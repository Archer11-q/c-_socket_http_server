#ifndef HTTPHANDLER_H
#define HTTPHANDLER_H

#include <string>
#include <cstdint>
#include <sys/socket.h>  
#include <unistd.h>
#include <iostream>
#include<fstream>  //文件操作依赖
#include<sstream>  //MIME类型解析依赖
#include"../utils/Logger.h"
#include<algorithm> //transform依赖
#include"../user/User.hpp"
#include<gtest/gtest.h>
#include<atomic>  //用于线程安全的原子操作
#include<ctime>   //用于时间统计
#include<sys/resource.h>


//HTTP相关常量
const uint32_t BUF_SIZE = 4096; // 接收缓冲区大小

//HttpHandler类框架
class HttpHandler {
public:
  //处理单个客户端HTTP请求（TCP模块传client_fd进来）
  void handleRequest(int client_fd);
  //重载Select模式接口
  void handleRequest(int client_fd,const char* buffer,size_t length);
  //重载接口：支持Keep-Alive，通过引用传递连接状态
  void handleRequest(int client_fd,const char* buffer,size_t length,bool& keep_alive);

  //解析HTTP请求路径
  std::string parse_http_path(const std::string& request);
  //根据路径，构建HTTP响应
  std::string build_http_response(const std::string& path);
  //重构构建HTTP响应，含keep-alive参数
  std::string build_http_response(int status_code,const std::string& content_type,const std::string& response_body,bool keep_alive);
  //向客户端发送指定路径对应静态文件，判断是否发送成功
  bool serverStaticFile(int client_fd,const std::string& path);
  //静态文件服务重载版，支持传递keep-alive
  bool serverStaticFile(int client_fd,const std::string& path,bool keep_alive);
  //获取文件路径后缀名
  std::string getMimeType(const std::string& file_path);
  //初始化服务器启动时间
  static void initServerStartTime();

private:
  //服务器状态统计变量
  static std::atomic<long long> total_requests; //累计请求数
  static std::atomic<int> active_connections;   //当前活跃连接数
  static time_t start_time;                     //服务器启动时间
  static std::atomic<long> cached_memory_kb_;   //缓存的内存值
  static std::atomic<time_t> last_update_time_; //上次更新时间戳

  //构建 /status 接口的JSON响应
  static std::string buildStatusJson();
  //更新内存缓存
  static void updateStatusCache();

  //静态文件处理公共逻辑
  bool handleStaticFileCore(int client_fd,const std::string& safe_path,bool keep_alive);
  //解析请求中的Connection头部，返回是否需要保持连接
  bool shouldKeepAlive(const std::string& request);
  //检查请求是否完整，用于长连接多次读取，避免粘包导致的解析错误
  bool isRequestComplete(const char* buffer,size_t length);
  //解析请求方法：GET/POST
  std::string parse_http_method(const std::string& request);
  //解析POST请求体
  std::string parse_http_body(const std::string& request);
  //从POST表单中获取参数
  std::string getPostParam(const std::string& body,const std::string& key);
  //发送JSON格式响应，兼容Keep-Alive
  void sendJsonResponse(int client_fd,int code,const std::string& josn,bool keep_alive);

  FRIEND_TEST(HttpHandlerTest,ParseHttpPath);
  FRIEND_TEST(HttpHandlerTest,ParseHttpMethod);
  FRIEND_TEST(HttpHandlerTest,IsRequestComplete);
  FRIEND_TEST(HttpHandlerTest,ShouldKeepAlive);
};

#endif // HTTPHANDLER_H
