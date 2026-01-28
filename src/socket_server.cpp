#include<sys/socket.h>  //socket依赖
#include<netinet/in.h>  //地址结构，协议族依赖
#include<unistd.h>      //文件，进程依赖
#include<cstring>       //strerror依赖
#include<iostream>

const int PORT = 8080;
const int BUF_SIZE =2048;

int main() {
  //1.创建Socket(IPv4+TCP)
  int server_fd=socket(AF_INET,SOCK_STREAM,0);
  if(server_fd<0) { 
    std::cerr<<"Socket创建失败"<<std::endl;
    return 1;  //异常退出
  }

  //2.配置地址结构体
  struct sockaddr_in server_addr;  //简化IPv4地址配置
  menset(&server_addr,0,sizeof(server_addr));	//清空数据，避免随机数值干扰
  server_addr.sin_family=AF_INET; 		//IPv4
  server_addr.sin_port=htons(PORT);		//绑定端口，转化为网络字节序
  server_addr.sin_addr.s_addr=INADDR_ANY;  	//监听所有端口

  //3.绑定端口
  if(bind(server_fd,(struct sockaddr*)&server_addr,sizeof(server)) {
    std::cerr<<"端口绑定失败"<<std::endl;
    return 1;
  }

  //4.监听端口:等待客户连接，设置最大同时监听上限
  if(listen(server_fd,5)<0) {
    std::cerr<<"端口监听失败"<<std::endl;
    close(server_fd);
    return 1;
  }

  std::cout<<"[阶段2：最小HTTP Server] 启动，监听8080端(支持GET/单路径)"<<std::endl;

  //5.循环接受连接+字节流回显
  while(1) {
    int client_fd=accept(server_fd,nullptr,nullptr);
    if(client_fd<0) continue;
    
    char buffer[BUF_SIZE] ={0};
    ssize_t recv_len=recv(client_fd,buffer,BUF_SIZE-1,0);
    if(recv_len<=0) {
      closer(client_fd);
      continue;
    }

    //5.1从纯字节流到HTTP协议处理
    std::string request(buffer);  //字节流转字符串，适配HTTP解析
    std::cout<<"收到HTTP请求: \n"<<request<<std::endl;

    //5.2构造标准HTTP/1.1响应（仅支持/路径，符合协议规范：响应行+响应头+空行+响应体
    std::string response_body="Hello Minimal HTTP Server!(Path: /)";
    std::string http_response="HTTP/1.1 200 OK\r\n"; 		     //响应行
    http_response += "Content-Type: text/plain; clarset=utf-8\r\n";  //文本格式
    http_response += "Content-Length: "+std::to_string(response_body.size())+"\r\n";  //自动计算文本长度
    http_response += "Connection: close\r\n";  //短连接格式
    http_response += "\r\n";  //强制换行，留出空白行
    http_response += response_body;  //响应体内容

    send(client_fd,http_response.c_str(),http_response.size(),0);  //发送HTTP响应，替代纯字节流
    close(client_fd);
  }

  close(server_fd);
  return 0;
}
