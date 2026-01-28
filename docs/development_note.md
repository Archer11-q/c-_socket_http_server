#C++ Socket->HTTP单文件迭代开发笔记
##核心规则：src目录仅保留socket_server.cpp, 后续开发在此基础上进行修改, GIT记录所有版本
###版本1: socket-v1: 纯Socket Server
-功能: 纯TCP字节流回显，掌握socket/bind/listen/accept/recv/send核心API

###版本2: http_min-v2: 最小HTTP Server
-增量修改: 仅改recv--send之间的逻辑，增大缓冲区为2048
-功能: 支持GET / 单路径，返回标准HTTP/1.1响应(响应行+响应头+空行+响应体)
-复用: Socket底层代码完全不变

###版本3: http-multi-3: 多路径HTTP Server
-增量扩展: 多路径判断逻辑
-功能: 支持/、/index、/about三路径访问,兼容浏览器/nc访问
-复用: Sockct底层和HTTP基础协议代码不变

##关键收获
1.HTTP是TCP之上的应用层协议: TCP负责底层字节流传输，HTTP定义传输格式
2.真实开发: 核心文件迭代修改, 通过Git标签追溯历史版本, 避免改错无法修复
3.编译: CMake一键编译socket_server.cpp 产物隔离在build/bin
