#include "HttpHandler.h"

//类外定义静态变量
std::atomic<long long> HttpHandler::total_requests{0}; //初始化累计请求数为0
std::atomic<int> HttpHandler::active_connections{0};   //初始化当前活跃连接数为0
time_t HttpHandler::start_time;
std::atomic<long> HttpHandler::cached_memory_kb_{0};
std::atomic<time_t> HttpHandler::last_update_time_{0};

//初始化服务记录的启动时间
void HttpHandler::initServerStartTime()
{
  start_time=time(nullptr); //记录服务器启动时间
}

//更新进程内存使用情况的缓存
void HttpHandler::updateStatusCache()
{
  //定义系统资源使用结构体，用于接收进程内存、CPU等资源占有信息
  struct rusage usage;

  //调用系统调用获取当前进程的资源使用数据
  if (getrusage(RUSAGE_SELF,&usage)==0)
  {
    //读取成功：将进程最大常驻内存值 原子写入缓存变量
    cached_memory_kb_.store(usage.ru_maxrss);
  }

  //无论是否读取成功，都更新【最后缓存更新时间戳】，避免频繁重试
  last_update_time_.store(time(nullptr));

  //调试日志：标记内存缓存已更新
  LOG_DEBUG("Memory cache update.");
}


//【Kepp-Alive模式】处理客户端请求
void HttpHandler::handleRequest(int client_fd,const char* buffer,size_t length,bool& keep_alive) {
  //增加连接数和总请求数
  active_connections++;
  total_requests++;

  //1.初始化连接状态
  keep_alive=false;

  //2.检查请求完整性
  if(!isRequestComplete(buffer,length)) {
    LOG_WARN("请求不完整，等待后续数据，fd="+std::to_string(client_fd));
    keep_alive=true;  //请求不完整但保持连接，等待后续数据补全
    return; //请求不完整，直接返回，保持连接状态为false，调用者根据该状态决定是否关闭连接
  }

  //3.解析请求基础信息
  std::string request(buffer,length); //获取请求，将buffer转化为字符串
  std::string path=parse_http_path(request);  //提取路径
  std::string method=parse_http_method(request);  //解析HTTP请求的方法和请求体，获取GET/POST
  keep_alive=shouldKeepAlive(request);  //判断是否保持连接


  //处理静态文件/动态响应
  //4.判断是否为静态文件，若是则进行静态文件处理
  if(path.find('.')!=std::string::npos) {
    if(serverStaticFile(client_fd,path,keep_alive)) {
      LOG_DEBUG("Static file served: " + path);
      return; //静态文件处理成功，直接返回
    }
  }


  //5.解析POST请求体
  std::string response_body;  //存储响应体内容
  std::string body;           //存储POST请求体
  if(method=="POST") body=parse_http_body(request);

  //6.处理API路由：匹配到路由发送响应直接return
  //路由1：用户注册接口
  if(method=="POST" && path=="/api/register"){
    //从POST请求体中获取用户名和密码参数
    std::string username=getPostParam(body,"username");
    std::string password=getPostParam(body,"password");
    //调用User类注册方法，执行用户注册逻辑：密码加密、数据库写入
    response_body=User::registerUser(username,password);
    //发送HTTP响应，状态码为200表示OK，响应体为JSON格式的注册结果
    sendJsonResponse(client_fd,200,response_body,keep_alive);
    return; //注册接口处理完成，直接返回
  }

  //路由2：用户登录接口
  if(method=="POST" && path=="/api/login"){
    //从POST请求体中或全部用户名和密码参数
    std::string username=getPostParam(body,"username");
    std::string password=getPostParam(body,"password");
    //调用User类的登录方法，验证用户名和密码
    response_body=User::loginUser(username,password);
    //发送HTTP响应，状态码200表示OK
    sendJsonResponse(client_fd,200,response_body,keep_alive);
    return; //登录接口处理完成，直接返回
  }

  //路由3：服务器状态监控接口
  if (method=="GET" && path=="/status")
  {
    std::string response_body=buildStatusJson(); //构建服务器状态的JSON响应
    sendJsonResponse(client_fd,200,response_body,keep_alive); //发送响应

    //减少活跃连接数
    active_connections--;
    return; //状态接口处理完成，直接返回
  }

  //7.处理默认动态响应/404
  response_body=build_http_response(path);
  //解析原有响应的状态码，判断是否包含404
  int status_code=(response_body.find("404 Not Found")!=std::string::npos) ? 404 : 200; 
  std::string response=build_http_response(status_code,"text/plain; charset=utf-8",
    response_body.substr(response_body.find("\r\n\r\n")+4),keep_alive);


  //8.循环发送响应，避免截断
  size_t sent=0;                 //记录已经成功发送的字节数
  size_t total=response.length(); //记录总共需要发送的字节数
  while(sent<total) { //未发送完继续发送
    //尝试发送尚未发送完的数据
    ssize_t ret=send(client_fd,response.c_str()+sent,total-sent,0);
    if(ret<0) { //发送完毕
      LOG_ERROR("发送响应失败，fd= "+std::to_string(client_fd));
      keep_alive=false;
      return;
    }
    sent+=ret;  //更新已经成功发送的字节数
  }
  LOG_DEBUG("动态响应发送成功，fd= "+std::to_string(client_fd)+", keep_alive= "+std::to_string(keep_alive));

  //减少活跃连接数
  active_connections--;
}


//处理客户端HTTP请求
void HttpHandler::handleRequest(int client_fd) {
  LOG_INFO("Client connected, start handle HTTP request");
  char buffer[BUF_SIZE]={0};
  ssize_t recv_len=recv(client_fd,buffer,BUF_SIZE-1,0);
  if(recv_len<=0) return;

  //复用通用逻辑，避免重复
  bool keep_alive=false;
  handleRequest(client_fd,buffer,recv_len,keep_alive);

  //fork模式专属：处理完请求关闭fd
  close(client_fd);
  std::string log_msg = "Client fd=" + std::to_string(client_fd) + " request handled, connection close(fork mode)";
  LOG_INFO(log_msg.c_str());
}


//【Select模式】处理客户端请求
void HttpHandler::handleRequest(int client_fd,const char* buffer,size_t length) {
  LOG_INFO("Client connected, start handle HTTP request (select mode)");
  bool keep_alive=false;
  //复用通用逻辑
  handleRequest(client_fd,buffer,length,keep_alive);
  if (!keep_alive) close(client_fd);
}


//解析HTTP请求路径
std::string HttpHandler::parse_http_path(const std::string& request) {
  size_t line_end=request.find("\r\n");  
  if(line_end==std::string::npos) return "/"; //未找到字符串，返回根路径
  std::string req_line=request.substr(0,line_end);  //截取路径
  size_t start=req_line.find(" ")+1; //路径起始位置
  size_t end=req_line.find(" ",start); //路径结束位置
  return (start==std::string::npos || end==std::string::npos)?"/":req_line.substr(start, end - start);  //未找到返根路径回/，找到返回具体路径
}


//提取静态文件处理的公共逻辑
bool HttpHandler::handleStaticFileCore(int client_fd,const std::string& safe_path,bool keep_alive) {
  //防止路径遍历攻击，避免访问上级目录文件
  if(safe_path.find("..")!=std::string::npos) {
    LOG_WARN("路径包含非法字符'..'，拒绝访问: "+safe_path);
    return false;
  }

  //二进制模式打开文件，定位到文件末尾，获取文件大小
  std::ifstream file(safe_path,std::ios::binary | std::ios::ate);
  if(!file.is_open()) {
    LOG_ERROR("静态文件打开失败: "+safe_path);
    return false;
  }
  std::streamsize size=file.tellg();  //tellg返回当前文件指针位置（文件总大小）
  file.seekg(0,std::ios::beg);  //将文件指针移回开头，准备读取
  
  //构造响应头：动态适配keep_alive
  std::string response="HTTP/1.1 200 OK\r\n";
  response += "Content-Type: "+getMimeType(safe_path)+"\r\n";
  response += "Content-Length: "+std::to_string(size)+"\r\n";
  if(keep_alive) {
    response += "Connection: keep-alive\r\n";
    response += "Keep-Alive: timeout=5,max=100\r\n";
  } else 
      response += "Connection: close\r\n";
  response += "\r\n";

  //发送响应头，优先让浏览器解析文件类型和大小
  size_t sent=0; //初始化已经成功读取的字节数
  size_t total_header=response.length();  //计算响应头的总长度
  while(sent<total_header) {
    ssize_t ret=send(client_fd,response.c_str()+sent,total_header-sent,0);
    if(ret<0) {
      LOG_ERROR("发送响应失败，fd="+std::to_string(client_fd));
      file.close();
      return false;
    }
    sent+=ret;
  }

  //分块发送文件内容
  char buffer[BUF_SIZE];
  while(file.read(buffer,sizeof(buffer))) {
    send(client_fd,buffer,file.gcount(),0);//gcount返回实际读取的字节数
  }

  //兜底处理：发送最后一次未读满缓冲区的剩余数据
  if(file.gcount()>0)
    send(client_fd,buffer,file.gcount(),0);

  file.close();
  LOG_DEBUG("静态文件发送完成(keep_alive="+std::to_string(keep_alive)+"): "+safe_path);
  return true;
}


//向客户端发送指定路径对应的静态文件，判断是否发送成功
bool HttpHandler::serverStaticFile(int client_fd,const std::string& path) {
  //安全限制：只允许访问public目录下的文件，拼接安全路径
  std::string project_root = "/home/archer/projects/cpp_socket_http_server";
  std::string safe_path = project_root + "/public" + path;  
  
  //调用核心函数，默认长连接为关闭
  return handleStaticFileCore(client_fd,safe_path,false);
}


//重构静态文件响应
bool HttpHandler::serverStaticFile(int client_fd,const std::string& path,bool keep_alive) {
  //拼接静态文件路径
  std::string project_root = "/home/archer/projects/cpp_socket_http_server";
  std::string safe_path=project_root+"/public"+path;

  return handleStaticFileCore(client_fd,safe_path,keep_alive);
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
  
  //调用重构函数实现响应构造
  return build_http_response(status_code,"text/plain; charset=utf-8",response_body,false);
}


//重构多路径HTTP响应
std::string HttpHandler::build_http_response(int status_code,const std::string& content_type,const std::string& response_body,bool keep_alive){
  //1.构造响应行
  std::string response;
  if(status_code==200)
    response="HTTP/1.1 200 OK\r\n";
  else if(status_code==404)
    response="HTTP/1.1 404 Not Found\r\n";
  else
    response="HTTP/1.1 500 Internal Server Error\r\n";

  //2.构造响应头，整合keep-alive逻辑
  response += "Content-Type: " + content_type+"\r\n";
  response += "Content-Length: " + std::to_string(response_body.size())+"\r\n";

  //3.动态控制Connetion头部
  if(keep_alive) {
    response += "Connection: keep-alive\r\n";
    response += "Keep-Alive: timeout=5,max=100\r\n";
  } else
      response += "Connection: close\r\n";

  //4.空行分隔头和体+响应体
  response += "\r\n";
  response += response_body;

  return response;
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


//解析客户端请求中的Connetion头部，返回是否需要保持连接
bool HttpHandler::shouldKeepAlive(const std::string& request) {
  //查找Connetion头部，未找到则按协议默认处理
  size_t conn_pos=request.find("Connection:");
  if(conn_pos==std::string::npos)  //HTTP/1.1默认Keep-Alive
    return true;

  //提取Connetion头部的值，处理空白字符
  size_t value_start=request.find(":",conn_pos)+1;
  size_t value_end=request.find("\r\n",value_start);
  std::string conn_value=request.substr(value_start,value_end-value_start);

  //去除首尾空白字符，转小写后比较
  conn_value.erase(0,conn_value.find_first_not_of(" \t"));//从头开始找
  conn_value.erase(conn_value.find_last_not_of(" \t")+1);//尾部开始找
  std::transform(conn_value.begin(),conn_value.end(),conn_value.begin(),::tolower);

  return conn_value !="close";
}

//基于HTTP协议判断请求是否完整
bool HttpHandler::isRequestComplete(const char* buffer,size_t length) {
  //HTTP请求头以\r\n\r\n结尾，找到该标识则请求完整
  if(length<4) return false;

  for(size_t i=0;i<=length-4;++i) 
    if(buffer[i]=='\r' && buffer[i+1]=='\n' && buffer[i+2]=='\r' && buffer[i+3]=='\n')
      return true;
  return false;
}


//解析请求方法GET/POST
std::string HttpHandler::parse_http_method(const std::string& request){
  //空格分隔请求方法与请求路径
  size_t end=request.find(" ");
  if(end==std::string::npos) return "GET"; //格式错误默认返回GET
  return request.substr(0,end);
}

//解析POST请求体
std::string HttpHandler::parse_http_body(const std::string& request){
  //查找空行分隔结束符
  size_t pos=request.find("\r\n\r\n");
  if(pos==std::string::npos) return ""; //没有找到空行即没有请求体

  //跳过空行返回请求体
  return request.substr(pos+4);
}

//从POST请求的表单中提取指定参数的值
std::string HttpHandler::getPostParam(const std::string& body,const std::string& key){
  //查找"key="在body中的位置
  size_t start=body.find(key+"=");
  if(start==std::string::npos) return ""; //参数不存在

  //跳过“key="部分，定位到值开始的位置
  start+=key.length()+1;

  //查找下一个参数分隔符&
  size_t end=body.find("&",start);
  if(end==std::string::npos) end=body.length(); //没有&即是最后一个字符
  
  //返回参数的值
  return body.substr(start,end-start);
}


//向客户端发送HTTP响应的JSON数据，支持Keep-Alive模式
void HttpHandler::sendJsonResponse(int client_fd,int code,const std::string& json,bool keep_alive){
  //构建完整的HTTP响应，包含响应头和响应体
  std::string response=build_http_response(
    code,
    "application/json; charset=utf-8",  //设置Content-Type为JSON
    json,
    keep_alive
  );

  //循环发送，确保所有数据都发送完成
  size_t sent=0;                //已发送的字节数
  size_t total=response.size(); //需要发送的总字节数

  while(sent<total){
    //发送剩余未发送的数据
    ssize_t ret=send(client_fd,response.c_str()+sent,total-sent,0);
    if(ret<0){
      LOG_ERROR("sendJsonResponse 发送失败, fd="+std::to_string(client_fd));
      keep_alive=false;
      return ;
    }
    sent+=ret;    //累加已发送的字节数
  }
}

//构建服务器状态监控的JSON响应数据
std::string HttpHandler::buildStatusJson()
{
  //获取当前系统时间
  time_t now=time(nullptr);

  //若距离上次更新超过10s，则更新缓存
  if (now-last_update_time_.load()>=10)
    updateStatusCache();

  //直接读取实时指标
  //计算服务器运行时间：当前时间-服务器启动时间
  double uptime=difftime(now,start_time); //单位秒
  //原子操作读取累计总请求数：线程安全，避免多线程统计冲突
  long long total_req=total_requests.load();
  //原子操作读取当前活跃连接数
  int active_conn=active_connections.load();
  //计算服务器QPS：总请求数/运行时长；运行时长为0时，QPS置零
  double qps=uptime>0? total_req/uptime : 0.0;

  //缓存指标
  long memory_kb=cached_memory_kb_.load();

  //手动拼接标准JSON格式字符串，封装所有监控指标
  std::string json="{\n";
  json += "  \"uptime\":"+std::to_string(uptime)+",\n";                 //服务器运行时间
  json += "  \"total_requests\":"+std::to_string(total_req)+",\n";      //累计处理请求总数
  json += "  \"active_connections\":"+std::to_string(active_conn)+",\n";//当前活跃连接数
  json += "  \"memory_kb\":"+std::to_string(memory_kb)+",\n";           //进程内存占用，单位KB
  json += "  \"qps\":"+std::to_string(qps)+",\n";   //平均每秒请求数
  json += "  \"cached\":true\n";
  json += "}\n";

  return json;
}
  
  
