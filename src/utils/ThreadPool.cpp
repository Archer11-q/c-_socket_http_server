#include "ThreadPool.h"

#include "Logger.h"

//构造函数：创建threadNum个工作线程
ThreadPool::ThreadPool(size_t threadNum):stopflag(false)
{
    std::string msg="线程池初始化，线程数量："+std::to_string(threadNum);
    LOG_INFO(msg);
    for (size_t i=0;i<threadNum;++i){
        //在工作线程末尾直接构造线程对象，并将成员函数worker作为线程函数
        workers.emplace_back(&ThreadPool::worker,this);
    }
}

//析构函数：调用stop函数等待所有线程完成并退出
ThreadPool::~ThreadPool()
{
    stop();
}

//停止线程池并等待所有工作线程退出
//阻塞线程直到所有线程执行完毕
void ThreadPool::stop()
{
    {
        //自动管理锁直到退出作用域自动解锁
        std::lock_guard<std::mutex> lock(mtx);
        stopflag=true;  //设置停止标志
    }
    //锁在此处释放，允许工作线程在收到通知前读取stopflag
    //唤醒所有线程
    cv.notify_all();

    //遍历所有工作线程，如果线程可加入，则加入线程
    for (std::thread &worker:workers)
    {
        if(worker.joinable()){
            worker.join();
        }
    }
    LOG_INFO("线程池已停止，所有线程已退出");
}

//工作线程主循环：从任务队列中取出任务执行，直到收到信号停止或队列为空
void ThreadPool::worker()
{
    while (true)
    {
        std::function<void()> task;

        {
            //使用unique_lock管理互斥锁，允许条件变量等待时释放锁
            std::unique_lock<std::mutex> lock(mtx);

            //条件变量等待：如果满足线程池停止或者队列非空，就解除等待获取锁
            cv.wait(lock,[this]()
            {
                return stopflag || !tasks.empty();
            });

            //如果任务停止并且队列为空，就退出循环
            if (stopflag && tasks.empty()) return;

            //否则从任务队列中取出任务
            task=std::move(tasks.front());
            tasks.pop();
        }

        //提前释放锁，允许其他工作线程通知取任务，提高并发现
        if (task) //检查是否有效
            task(); //执行任务函数
    }
}


