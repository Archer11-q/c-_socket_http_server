#include "MySQLPool.h"

//单例实例
MySQLPool& MySQLPool::getInstance()
{
    static MySQLPool pool;
    return pool;
}

MySQLPool::~MySQLPool()
{
    closePool();
}

//创建一批连接加入池
void MySQLPool::init(const std::string& host, const std::string& user, const std::string& pwd, const std::string& db, int max_conn)
{
    std::lock_guard<std::mutex> lock(_mtx);

    _host=host;
    _user=user;
    _pwd=pwd;
    _db=db;
    _maxConn=max_conn;
    _isClosed=false;

    for (int i=0;i<max_conn;++i)
    {
        MYSQL* conn=mysql_init(nullptr);
        if (!conn) throw std::runtime_error("mysql_init failed");

        //连接MySQL服务器
        if (!mysql_real_connect(conn,host.c_str(),user.c_str(),pwd.c_str(),
                                db.c_str(),3306,nullptr,0)){
            mysql_close(conn);
            throw std::runtime_error("mysql connect failed");
        }

        //设置UTF8编码
        mysql_set_character_set(conn,"utf8mb4");
        _connQueue.push(conn);
    }
    LOG_INFO("[MySQLPool] 连接池初始化完成");
}

//从池获取连接
MYSQL* MySQLPool::getConnection()
{
    std::unique_lock<std::mutex> lock(_mtx);

    //等待可用连接：连接池关闭 或 队列非空
    bool ok=_cond.wait_for(lock,std::chrono::seconds(5),[this]()
    {
        return _isClosed || !_connQueue.empty();
    });

    if (!ok) {
        LOG_WARN("[MySQLPool] 获取连接超时");
        return nullptr;
    }

    //如果连接池关闭，返回空
    if (_isClosed) return nullptr;

    //否则取出首部连接
    MYSQL* conn=_connQueue.front();
    _connQueue.pop();

    //检查连接是否存货，失败则自动重连
    if (mysql_ping(conn)!=0)
    {
        mysql_close(conn);
        conn=mysql_init(nullptr);
        mysql_real_connect(conn,_host.c_str(),_user.c_str(),_pwd.c_str(),
                           _db.c_str(),3306,nullptr,0);
    }

    return conn;
}

//归还连接到池
void MySQLPool::releaseConnection(MYSQL* conn)
{
    //连接失效 或 连接池关闭
    if (!conn || _isClosed) return;

    std::lock_guard<std::mutex> lock(_mtx);
    _connQueue.push(conn);
    _cond.notify_one();         //唤醒一个等待线程，通知连接池已有空闲连接
}

//关闭连接池，释放所有资源
void MySQLPool::closePool()
{
    std::lock_guard<std::mutex> lock(_mtx);
    _isClosed=true;

    //关闭所有连接
    while (!_connQueue.empty())
    {
        MYSQL* c=_connQueue.front();
        _connQueue.pop();
        mysql_close(c);
    }

    _cond.notify_all();     //唤醒所有等待线程
    LOG_INFO("[MySQLPool] 连接池已关闭");
}

