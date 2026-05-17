#ifndef LOGGER_H
#define LOGGER_H	//头文件保护

#include<string>
#include<fstream>
#include<mutex>		//互斥锁依赖
#include<ctime>		//日志时间格式化依赖
#include<sstream>	//字符串拼接依赖
#include<cstring>	//strerror依赖
#include<unistd.h>	//getpid依赖
#include<iostream>	//控制台输出依赖
#include<queue>
#include<thread>
#include<condition_variable>  //条件变量依赖
#include<atomic>

// 同步日志
class Logger {
public:
  enum Level{DEBUG,INFO,WARN,ERROR};	//日志分级

  static Logger& getInstance();	//单例模式，全局唯一日志实例
  void log(Level level,const std::string& message);	//日志过滤、格式化，输出
  void setLogFile(const std::string& filepath);	//设置日志输出文件，默认控制台输出
private:
  Logger();	//私有构造函数，禁止外部实例化（单例）
  static Logger* instance;	//类内声明实例
  std::ofstream log_file;	//日志文件输出流
  std::mutex log_mutex;		//日志互斥锁，保证线程/进程安全
};


// 异步日志：生产与消费分离，前端线程写入队列，后台线程负责落盘
class AsyncLogger
{
public:
  //获取单例实例：全局唯一日志入口
  static AsyncLogger& getInstance();
  //日志写入接口
  void log(Logger::Level level,const std::string& message);
  //设置日志输出文件路径
  void setLogFile(const std::string& filepath);

  //唤醒后台线程，刷新剩余日志，安全退出线程
  void stop();

private:
  //构造函数私有化：单例模式
  AsyncLogger();
  //析构函数
  ~AsyncLogger();

  //后台工作线程函数
  void worker();

  //写入一条日志（供worker和stop兜底共用）
  void writeOne(const std::string& log_str);

  //日志缓冲队列：存储待写入文件的日志字符串
  std::queue<std::string> log_queue_;
  //互斥锁：保护日志队列线程安全访问
  std::mutex queue_mutex_;
  //条件变量：通知后台线程有新日志可处理
  std::condition_variable cv_;

  //后台工作线程：负责异步写文件
  std::thread worker_thread_;
  //日志文件输出流
  std::ofstream log_file_;
  //线程停止标志：原子变量，保证多线程可见性
  std::atomic<bool> stop_flag_;
};


// 同步日志宏定义：4个分级日志
//#define LOG_DEBUG(msg) Logger::getInstance().log(Logger::DEBUG,msg) //调试信息
//#define LOG_INFO(msg) Logger::getInstance().log(Logger::INFO,msg)  //启动成功
//#define LOG_WARN(msg) Logger::getInstance().log(Logger::WARN,msg)  //警告
//#define LOG_ERROR(msg) Logger::getInstance().log(Logger::ERROR,msg)//错误

//perror专用宏,将系统错误信息转化为ERROR级日志
//#define LOG_SYS_ERROR(msg) Logger::getInstance().log(Logger::ERROR,std::string(msg)+": "+std::string(strerror(errno)))


// 异步日志宏定义
#define LOG_DEBUG(msg) AsyncLogger::getInstance().log(Logger::DEBUG,msg)
#define LOG_INFO(msg) AsyncLogger::getInstance().log(Logger::INFO,msg)
#define  LOG_WARN(msg) AsyncLogger::getInstance().log(Logger::WARN,msg)
#define  LOG_ERROR(msg) AsyncLogger::getInstance().log(Logger::ERROR,msg)

//系统错误日志宏定：自动拼接系统错误描述
#define LOG_SYS_ERROR(msg) AsyncLogger::getInstance().log(Logger::ERROR,std::string(msg)+": "+std::string(strerror(errno)))


#endif