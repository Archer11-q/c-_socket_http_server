#include "HttpHandler.h"

//解析HTTP请求路径
std::string HttpHandler::parse_http_path(const std::string& request) {
  size_t line_end=request.find("\r\n");  
  if(line_end==std::string::npos) return "/"; //未找到字符串，返回根路径
  std::string rep_line=request.substr(0,line_end);  //截取路径
  size_t start=rep_line.find(" ")+1; //路径起始位置
  size_t end=rep_line.find(" ",start); //路径结束位置
  return (start==std::string::npos || end==std::string::npos)?"/":rep_line.substr(start, end - start);  //未找到返根路径回/，找到返回具体路径
}

//向客户端发送指定路径对应的静态文件，判断是否发送成功
bool HttpHandler::serverStaticFile(int client_fd,const std::string& path) {
  //安全限制：只允许访问public目录下的文件，拼接安全路径
  std::string project_root = "/home/archer/projects/cpp_socket_http_server";
  std::string safe_path = project_root + "/public" + path;  

  //防止路径遍历攻击，避免访问上级目录文件
  if(safe_path.find("..")!=std::string::npos)
    return false;

  //二进制模式打开文件，定位到文件末尾，获取文件大小
  std::ifstream file(safe_path,std::ios::binary | std::ios::ate);
  if(!file.is_open())
    return false;       //文件不存在/不可读，走404逻辑

  //获取文件大小
  std::streamsize size=file.tellg();  //tellg返回当前文件指针位置（文件总大小）
  file.seekg(0,std::ios::beg);  //将文件指针移回开头，准备读取

  //构建标准HTTP响应头
  std::string response="HTTP/1.1 200 OK\r\n";
  response += "Content-Type: "+getMimeType(safe_path)+"\r\n"; //动态获取MIEM类型
  response += "Content-Length: "+std::to_string(size)+"\r\n"; //文件总大小
  response += "Connection: close\r\n\r\n";      //空行分隔响应头和响应体

  //发送响应头, 优先让浏览器解析文件类型和大小
  send(client_fd,response.c_str(),response.length(),0);

  //分块发送文件内容，缓冲区4096,适配大文件，避免内存溢出
  char buffer[BUF_SIZE];
  while(file.read(buffer,sizeof(buffer))) {
    send(client_fd,buffer,file.gcount(),0);     //gcout返回实际读取的字节数
  }

  //兜底处理：发送最后一次未读满缓冲区的剩余数据
  if(file.gcount()>0)
    send(client_fd,buffer,file.gcount(),0);

  file.close();  //关闭文件流，避免资源泄漏
  return true;
}


//构造HTTP响应
std::string HttpHandler::build_http_response(const std::string& path) {
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
void HttpHandler::handleRequest(int client_fd) {
  LOG_INFO("Client connected, start handle HTTP request");
  char buffer[BUF_SIZE]={0};
  ssize_t recv_len=recv(client_fd,buffer,BUF_SIZE-1,0);
  if(recv_len<=0) return;

  //复用select模式接口，避免重复
  handleRequest(client_fd,buffer,recv_len);

  //fork模式专属：处理完请求关闭fd
  close(client_fd);
  std::string log_msg = "Client fd=" + std::to_string(client_fd) + " request handled, connetion close(fork mode)";
  LOG_INFO(log_msg.c_str());
}

void HttpHandler::handleRequest(int client_fd,const char* buffer,size_t length) {
  LOG_INFO("Client connected, start handle HTTP request (select mode)");
  std::string request(buffer,length);  //将buffer转化为字符串类型
  std::string path=this->parse_http_path(request);  //截取路径

  //静态优先，动态兜底
  if(path.find('.')!=std::string::npos) {//判断是否为文件请求
    if(this->serverStaticFile(client_fd,path)) {
      LOG_DEBUG("Static file served: " + path);
      return;   //静态文件服务成功，避免重复发送动态响应
    }
  std::string log_msg = "Static file not found: " + path + ",failback to dynamic response";
  LOG_INFO(log_msg.c_str());  
  }

  //启动动态响应
  std::string response = this->build_http_response(path); //根据路径实现差异化处理
  ssize_t send_len=send(client_fd,response.c_str(),response.size(),0);
  if(send_len<0) {
    std::string log_msg = "Send dynamic response failed for fd=" + std::to_string(client_fd);
    LOG_DEBUG(log_msg.c_str());
  } else {
      std::string log_msg = "Dynamic response sent " + path + ", length=" + std::to_string(send_len);
      LOG_DEBUG(log_msg.c_str());
  }
}

//获取文件路径后缀名
std::string HttpHandler::getMimeType(const std::string& file_path) {
  //提取文件后缀名
  size_t dot_pos=file_path.find_last_of('.');	//从后往前查找.的位置
  if(dot_pos==std::string::npos)
    return "text/plain; charset=utf-8";	//纯文本形式

  std::string ext=file_path.substr(dot_pos+1);
  //基础类型映射
  if(ext=="html" || ext=="htm")
    return "text/html; charset=utf-8";	//html类型
  else if(ext=="txt")
    return "text/plain; charset=utf-8";	//文本类型
  else 
    return "application/octet-stream";	//未知类型，返回二进制流
}

