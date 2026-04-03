#ifndef CPP_SOCKET_HTTP_SERVER_THREADPOOL_H
#define CPP_SOCKET_HTTP_SERVER_THREADPOOL_H

#include<iostream>
#include <thread>
#include <vector>
#include<queue>
#include<mutex>
#include<condition_variable>
#include<functional>

#include"../src/utils/Logger.h"


class ThreadPool
{
public:
    //构造函数：创建threadNum个工作线程
    ThreadPool(size_t thread_num);
    //析构函数：停止所有线程
    ~ThreadPool();
    //添加任务到队列
    template<class F>
    void enqueue(F&& f);

    //停止线程池
    void stop();

private:
    void worker();  //工作线程执行函数

    std::vector<std::thread> workers;           //工作线程
    std::queue<std::function<void()>> tasks;    //任务队列
    std::mutex mtx;                             //互斥锁
    std::condition_variable cv;                 //条件变量
    bool stopflag;                              //停止标识
};


template<class F>
void ThreadPool::enqueue(F&& f)
{
    {
        //添加互斥锁和条件变量
        std::lock_guard<std::mutex> lock(mtx);
        tasks.emplace(std::forward<F>(f));
    }

    //唤醒一个线程
    cv.notify_one();
}


#endif //CPP_SOCKET_HTTP_SERVER_THREADPOOL_H