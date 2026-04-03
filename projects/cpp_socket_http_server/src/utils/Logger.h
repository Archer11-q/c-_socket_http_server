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

//宏定义：4个分级日志
#define LOG_DEBUG(msg) Logger::getInstance().log(Logger::DEBUG,msg) //调试信息
#define LOG_INFO(msg) Logger::getInstance().log(Logger::INFO,msg)  //启动成功
#define LOG_WARN(msg) Logger::getInstance().log(Logger::WARN,msg)  //警告
#define LOG_ERROR(msg) Logger::getInstance().log(Logger::ERROR,msg)//错误

//perror专用宏,将系统错误信息转化为ERROR级日志
#define LOG_SYS_ERROR(msg) Logger::getInstance().log(Logger::ERROR,std::string(msg)+": "+std::string(strerror(errno)))

#endif
