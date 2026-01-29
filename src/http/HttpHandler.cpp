#include "HttpHandler.h"


//解析HTTP请求路径
std::string parse_http_path(const std::string& request) {
  size_t line_end=request.find("\r\n");  
  if(line_end==std::string::npos) return "/"; //未找到字符串，返回根路径
  std::string rep_line=request.substr(0,line_end);  //截取路径
  size_t start=rep_line.find(" ")+1; //路径起始位置
  size_t end=rep_line.find(" ",start); //路径结束位置
  return (start==std::string::npos || end==std::string::npos)?"/":rep_line.substr(start, end - start);  //未找到返根路径回/，找到返回具体路径
}

//构造HTTP响应
std::string build_http_response(const std::string& path) {
  std::string response_body;
  int status_code = 200;
  
  if (path == "/" || path == "/index") {
        response_body = "Hello HTTP Server | 多模块重构完成 | 路径：" + path;
  } else if (path == "/about") {
        response_body = "Server Info: C++多模块开发 | 网络/HTTP模块解耦 | 路径：" + path;
  } else {
        response_body = "404 Not Found! Path: " + path;
        status_code = 404;
  }

  // 标准HTTP响应构建
    std::string response = (status_code == 200) ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 404 Not Found\r\n";
    response += "Content-Type: text/plain; charset=utf-8\r\n";
    response += "Content-Length: " + std::to_string(response_body.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += response_body;
    return response;
}

//处理客户端HTTP请求
void handle_client(int client_fd) {
  char buffer[BUF_SIZE]={0};
  ssize_t recv_len=recv(client_fd,buffer,BUF_SIZE-1,0);
  if(recv_len<=0) return;

  std::string request(buffer,recv_len);  //将buffer转化为字符串类型
  std::string path=parse_http_path(request);  //截取路径
  std::string response = build_http_response(path); //根据路径实现差异化处理
  send(client_fd,response.c_str(),response.size(),0);
}
