#include<gtest/gtest.h>
#include"net/TcpServer.h"


// 测试用例：单次获取与释放缓冲区
TEST(BufferPoolTest,AcquireRelease)
{
    //创建缓冲区实例
    TcpServer::BufferPool pool;

    //从缓冲池获取一个缓冲区
    auto buf=pool.acquire();
    EXPECT_EQ(buf.size(),8192); //默认缓冲区大小为8192字节

    //从缓冲区释放回缓冲池
    pool.release(std::move(buf));
}


// 测试用例：缓冲区复用功能
TEST(BufferPoolTest,ReuseBuffer)
{
    TcpServer::BufferPool pool;

    //第一次从缓冲池获取缓冲区
    auto buf1=pool.acquire();
    EXPECT_EQ(buf1.size(),8192);

    //释放缓冲区到缓冲池
    pool.release(std::move(buf1));

    //第二次获取缓冲区，应复用之前释放的缓冲区
    auto buf2=pool.acquire();
    EXPECT_EQ(buf2.size(),8192); //缓冲区大小仍为8192
}


// 测试用例：多缓冲区批量获取与释放
TEST(BufferPoolTest,MultipleAcquireRelease)
{
    TcpServer::BufferPool pool;

    //存储多个获取的缓冲区
    std::vector<std::vector<char>> buffers;

    //循环获取5个缓冲区，并校检每个缓冲区大小
    for (int i=0;i<5;++i)
    {
        buffers.push_back(pool.acquire());
        EXPECT_EQ(buffers.back().size(),8192);
    }

    //批量释放所有缓冲区回缓冲池
    for (auto& buf:buffers)
        pool.release(std::move(buf));

    //重新获取一个缓冲区，应从复用池中获取
    auto buf=pool.acquire();
    EXPECT_EQ(buf.size(),8192); //缓冲区大小仍为8192
}