#include<gtest/gtest.h>
#include<thread>
#include<vector>
#include<chrono>    //用于模拟业务耗时、线程等待
#include"MySQLPool.h"


// 测试用例：单次获取与释放数据库连接
TEST(MySQLPoolTest,AcquireReleaseConnection)
{
    //1.获取连接池单例实例
    MySQLPool& pool=MySQLPool::getInstance();

    //2.初始化连接池
    pool.init("127.0.0.1","root","123456","http_server_db",5);

    //3.从连接池获取一个可用的MySQL连接
    MYSQL* conn=pool.getConnection();
    //4.断言：获取的连接不能为空，验证获取成功
    EXPECT_NE(conn,nullptr);

    //5.将使用完的连接归还到连接池
    pool.releaseConnection(conn);

    //6.关闭连接池，释放所有资源
    pool.closePool();
}

// 测试用例：连接复用功能
TEST(MySQLPoolTest,ReuseConnection)
{
    //1.获取连接池单例并初始化
    MySQLPool& pool=MySQLPool::getInstance();
    pool.init("127.0.0.1","root","123456","http_server_db",5);

    //2.第一次获取连接
    MYSQL* conn1=pool.getConnection();
    //断言：第一次获取连接成功
    EXPECT_NE(conn1,nullptr);

    //3.释放第一次获取的连接到连接池
    pool.releaseConnection(conn1);

    //4.第二次获取连接
    MYSQL* conn2=pool.getConnection();
    //断言：第二次获取连接成功，验证复用生效
    EXPECT_NE(conn2,nullptr);

    //5.释放第二次获取的连接并关闭连接池
    pool.releaseConnection(conn2);
    pool.closePool();
}

// 测试用例：多线程并发获取与释放连接
TEST(MySQLPoolTest,ConcurrentAcquireRelease)
{
    //1.获取连接池单例并初始化
    MySQLPool& pool=MySQLPool::getInstance();
    pool.init("127.0.0.1","root","123456","http_server_db",5);

    //2.创建线程容器，存储10个并发工程线程
    std::vector<std::thread> threads;

    //3.循环创建10个线程，模拟高并发获取/释放线程
    for (int i=0;i<10;++i)
    {
        //lambda表达式创建线程，捕获连接池引用
        threads.emplace_back([&pool]()
        {
            //线程内：获取MySQL连接
            MYSQL* conn=pool.getConnection();
            //断言：并发场景下连接获取成功
            EXPECT_NE(conn,nullptr);

            //模拟业务逻辑处理：耗时10ms
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            //线程内：释放连接回连接池
            pool.releaseConnection(conn);
        });
    }

    //4.等待所有并发线程执行完毕
    for (auto& t:threads)
        t.join();

    //5.关闭连接池
    pool.closePool();
}

// 测试用例：连接池满时的阻塞等待行为
TEST(MySQLPoolTest,FullPoolBlockWait)
{
    //1.获取连接池单例并初始化
    MySQLPool& pool=MySQLPool::getInstance();
    pool.init("127.0.0.1","root","123456","http_server_db",5);
    //存储占用的连接
    std::vector<MYSQL*> connections;

    //2.循环获取5个连接，占满整个连接池
    for (int i=0;i<5;++i)
    {
        MYSQL* conn=pool.getConnection();
        EXPECT_NE(conn,nullptr);
        connections.push_back(conn);
    }

    //3.标记阻塞线程是否执行完成
    bool thread_executed=false;
    //4.创建第6个线程：此时连接池已满，线程会阻塞等待空闲连接
    std::thread wait_thread([&]()
    {
        //阻塞等待，直到有连接释放
        MYSQL* conn=pool.getConnection();
        EXPECT_NE(conn,nullptr);
        //释放获取到的连接
        pool.releaseConnection(conn);
        //标记线程执行完成
        thread_executed=true;
    });

    //5.等待50ms，确保线程进入阻塞状态
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    //6.释放一个占用的连接，唤醒阻塞的线程
    pool.releaseConnection(connections.back());
    connections.pop_back();

    //7.等待50ms，确保阻塞线程被唤醒并执行完毕
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    //断言：阻塞线程成功被唤醒并执行
    EXPECT_EQ(thread_executed,true);

    //8.释放所有剩余的连接
    for (auto& conn:connections)
        pool.releaseConnection(conn);
    //9.等待阻塞线程结束
    wait_thread.join();
    //10.关闭连接池
    pool.closePool();
}

