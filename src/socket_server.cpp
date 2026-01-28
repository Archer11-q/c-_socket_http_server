#include<sys/socket.h>  //socket依赖
#include<netinet/in.h>  //地址结构，协议族依赖
#include<unistd.h>      //文件，进程依赖
#include<cstring>       //strerror依赖
#include<iostream>

const int PORT = 8080;
const int BUF_SIZE =1024;

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

    std::cout<<"收到TCP数据："<<buffer <<std::endl;
    send(client_fd,buffer,reve_len,0);  //纯字节流回显
    close(client_fd);
  }

  close(server_fd);
  return 0;
}
