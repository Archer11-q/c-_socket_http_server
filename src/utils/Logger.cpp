#include "Logger.h"

//日志级别转字符串，用于格式化输出
static std::string levelToString(Logger::Level level) {
  switch(level) {
    case Logger::DEBUG: return "DEBUG";
    case Logger::INFO:  return "INFO";
    case Logger::WARN:  return "WARN";
    case Logger::ERROR: return "ERROR";
    default:            return "UNKNOWN";
  }
}

//获取当前系统时间
static std::string getCurrentTime() {
  time_t now=time(nullptr);	//仅获取当前时间戳
  tm* local_tm=localtime(&now);	//将UNIX时间戳转化为本地时区可读时间结构体
  char buffer[64]={0};	
  strftime(buffer,sizeof(buffer),"[%Y-%m-%d %H:%M:%S]",local_tm);	//按照自定义格式化规则，将时间转换为可读字符串
  return std::string(buffer);
}

//私有构造函数
Logger::Logger() = default;	//显示默认构造函数，成员变量默认初始化

//类外初始化静态成员：完成类型+类名
Logger *Logger::instance =nullptr;

//单例模式getInstance，全局唯一实例，线程安全
Logger& Logger::getInstance() {
  if(instance==nullptr)	//判断实例是否已被创建
    instance=new Logger();	//仅首次调用初始化，全局唯一
  return *instance;
}

//设置日志输出文件，实现文件写入功能
void Logger::setLogFile(const std::string& filepath) {
  //避免重复打开文件
  if(log_file.is_open())
    log_file.close();

  //以追加模式打开，避免覆盖原有日志
  log_file.open(filepath,std::ios::out | std::ios::app);

  //若文件打开失败，输出ERROR级日志到控制台
  if(!log_file.is_open())
    std::cerr<<"Logger: open log file failned - "<<filepath<<std::endl;
}

//核心log方法：线程/进程安全+双输出+格式化
void Logger::log(Level level,const std::string& message) {
  //加锁保证线程/进程安全，多进程下避免日志乱序
  std::lock_guard<std::mutex> lock(log_mutex);	//自动加锁/解锁

  //日志格式化：【时间】【进程ID】【级别】日志内容
  std::ostringstream oss;	//字符串输出流
  oss<<getCurrentTime()<<"【PID: "<<getpid()<<"】"//标志日志所属进程
  <<"【"<<levelToString(level)<<"】"<<message<<std::endl;

  std::string log_str=oss.str();	//返回字符串类型oss所有拼接内容

  //1.控制台输出，实时查看
  std::cout<<log_str;

  //2.文件输出：持久化存储，若文件流打开则写入
  if(log_file.is_open()) {
    log_file<<log_str;	//将完成拼接的字符串写入日志文件
    log_file.flush();	//强制刷新缓冲区，避免日志丢失
  }
}


// 异步日志实现

//获取单例实例
AsyncLogger& AsyncLogger::getInstance()
{
  static AsyncLogger instance;
  return instance;
}

//构造函数：初始化停止标志位，创建并启动后台工作线程
AsyncLogger::AsyncLogger():stop_flag_(false)
{
  worker_thread_=std::thread(&AsyncLogger::worker,this);  //绑定成员函数，传递this指针
}

//析构函数：自动调用stop函数，安全回收线程资源，保证程序退出前日志全部写入
AsyncLogger::~AsyncLogger()
{
  stop();
}

//单条日志落盘（供worker和stop兜底共用）
void AsyncLogger::writeOne(const std::string& log_str)
{
  std::cout<<log_str;
  if (log_file_.is_open())
  {
    log_file_<<log_str;
    log_file_.flush();
  }
}

//日志写入接口（生产者）：完成日志格式化-->加锁入队-->通知消费线程
void AsyncLogger::log(Logger::Level level,const std::string& message)
{
  // 停止后拒绝新日志（缩小竞态窗口，兜底在stop()中）
  if (stop_flag_.load(std::memory_order_acquire)) return;

  //格式化日志
  std::ostringstream oss;
  oss<<getCurrentTime()<<"【PID："<<getpid()<<"】"
     <<"【"<<levelToString(level)<<"】"<<message<<std::endl;
  std::string log_str=oss.str();

  //加锁保护队列，将格式化后的日志字符串加入队列
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    log_queue_.push(std::move(log_str));
  }
  cv_.notify_one();	//通知后台线程有新日志可处理
}

//设置日志输出文件路径：加锁保证文件操作线程安全，关闭已有文件并以追加模式打开新文件
void AsyncLogger::setLogFile(const std::string& filepath)
{
  std::lock_guard<std::mutex> lock(queue_mutex_);
  if (log_file_.is_open()) log_file_.close();

  log_file_.open(filepath,std::ios::out | std::ios::app);

  //文件打开失败时向标准错误输出提示信息
  if (!log_file_.is_open())
    std::cerr<<"AsyncLogger: open log file failed - "<<filepath<<std::endl;
}

//后台工作线程函数（消费者）：循环等待日志任务，从队列取出并输出到控制台+文件
void AsyncLogger::worker()
{
  //未收到停止信号时持续循环
  while (!stop_flag_ || !log_queue_.empty())
  {
    std::string log_str;
    {
      //加锁等待条件变量通知
      std::unique_lock<std::mutex> lock(queue_mutex_);

      //等待条件：队列非空 或 收到停止信号
      cv_.wait(lock,[this]()
      {
        return !log_queue_.empty() || stop_flag_;
      });

      //停止信号+队列为空，退出线程循环
      if (stop_flag_ && log_queue_.empty()) break;
      //从队列头部取出一条日志并弹出
      if (!log_queue_.empty())
      {
        log_str=std::move(log_queue_.front());
        log_queue_.pop();
      }
    }
    writeOne(log_str);
  }
}

//停止异步日志模块：设置停止标志，唤醒线程，等待工作线程执行完毕后回收线程资源
//随后兜底排空队列，防止工作线程break后生产者新入队的消息丢失
void AsyncLogger::stop()
{
  stop_flag_.store(true, std::memory_order_release);
  cv_.notify_one();
  if (worker_thread_.joinable()) worker_thread_.join();

  std::unique_lock<std::mutex> lock(queue_mutex_);
  while (!log_queue_.empty())
  {
    std::string msg = std::move(log_queue_.front());
    log_queue_.pop();
    lock.unlock();
    writeOne(msg);
    lock.lock();
  }
}