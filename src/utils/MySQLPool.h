#ifndef CPP_SOCKET_HTTP_SERVER_MYSQLPOOL_H
#define CPP_SOCKET_HTTP_SERVER_MYSQLPOOL_H
#include<queue>
#include<mutex>
#include<condition_variable>
#include<string>
#include<mysql/mysql.h>
#include<../utils/Logger.h>
#include<chrono>

class MySQLPool
{
public:
    //单例模式获取实例
    static MySQLPool& getInstance();
    //初始化连接
    void init(const std::string& host,const std::string& user,
              const std::string& pwd,const std::string& db,int max_conn=8);
    //获取一个可用连接(阻塞)
    MYSQL* getConnection();
    //归还连接到连接池
    void releaseConnection(MYSQL* conn);
    //关闭连接池，释放所有资源
    void closePool();

    //禁止拷贝和赋值
    MySQLPool(const MySQLPool&)=delete;
    MySQLPool& operator=(const MySQLPool&)=delete;

private:
    MySQLPool()=default;
    ~MySQLPool();

    std::queue<MYSQL*> _connQueue;  //连接队列
    std::mutex _mtx;                //队列互斥锁
    std::condition_variable _cond;  //阻塞等待条件

    std::string _host;
    std::string _user;
    std::string _pwd;
    std::string _db;

    int _maxConn;           //最大连接数
    bool _isClosed{false};  //池是否关闭

};


#endif //CPP_SOCKET_HTTP_SERVER_MYSQLPOOL_H