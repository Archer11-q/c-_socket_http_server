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

###版本4：http-modular-4 多模块拆分重构
-完成项目多模块拆分重构，拆分为main入口、net网络模块、http业务处理模块、移除旧单文件socket_server.cpp
-修改CMakeLists.txt, 适配多文件编译，完成本地构建与功能验证
-在feature/http_server分支上完成代码提交，推送至远程github仓库

###版本5:http-complete-5
-核心重构：网络模块新增fork()多进程模型，父进程监听端口，子进程独立处理单个客户端请求，实现并发请求支持
-日志系统：新增utils模块及单例Logger日志系统，支持时间戳+进程PID+日志级别分级输出（DEBUG、INFO、WARN、ERROR），修复宏定语语法错误，配置项目根目录logs/为日志存储历经，实现多进程日志统一写入与进程区分
-静态文件支持：拓展http模块，实现二进制文件读取传输，支持png/jpg等图片以及css/js等纯文本静态资源，新增文件后缀与MIME类型映射，返回标准HTTPA响应头



##关键收获
1.HTTP是TCP之上的应用层协议: TCP负责底层字节流传输，HTTP定义传输格式
2.真实开发: 核心文件迭代修改, 通过Git标签追溯历史版本, 避免改错无法修复
3.编译: CMake一键编译socket_server.cpp 产物隔离在build/bin
4.并发模型：新增fork的多进程是Linux下轻量并发实现方案，子进程独立运行可避免请求阻塞，通过进程PID可有效区分不同请求处理进程
5.工程规范：日志系统三服务器端开发必备，分级日志+全链路埋点可大幅提升问题排查效率；静态资源统一管理，二进制读取+正确MIME类型是图片等非文本资源正常传输关键

