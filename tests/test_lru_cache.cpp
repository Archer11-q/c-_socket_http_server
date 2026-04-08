#include<gtest/gtest.h>
#include"../src/utils/LRU_Cache.h"

// 测试用例：基本的插入何获取功能
TEST(LRUCacheTest,PutAndGet)
{
    //创建容量为2的LRU缓存
    LRU_Cache cache(2);
    //向缓存插入键值对
    cache.put("key1","val1");
    //获取key1对应的值，预期返回val1
    EXPECT_EQ(cache.get("key1"),"val1");
    //获取不存在的键，预期返回空字符串
    EXPECT_EQ(cache.get("noexistent"),"");
}

// 测试用例：缓存容量限制与LRU淘汰策略
TEST(LRUCacheTest,Eviction)
{
    //创建容量为2的LRU缓存
    LRU_Cache cache(2);

    cache.put("key1","val1");
    cache.put("key2","val2");
    //插入第三个数据，触发淘汰：最久未使用的key1被移除
    cache.put("key3","val3");
    //断言：key1被淘汰，返回空字符串；key2和key3仍然存在
    EXPECT_EQ(cache.get("key1"),"");
    EXPECT_EQ(cache.get("key2"),"val2");
    EXPECT_EQ(cache.get("key3"),"val3");
}

// 测试用例：更新已存在的键值
TEST(LRUCacheTest,Update)
{
    //创建容量为2的LRU缓存
    LRU_Cache cache(2);
    cache.put("key1","val1");

    //对key1重新赋值
    cache.put("key1","new_value1");
    EXPECT_EQ(cache.get("key1"),"new_value1"); //验证更新后的值
}

// 测试用例：访问顺序对LRU的影响
TEST(LRUCacheTest,AccessOrder)
{
    //创建容量为3的LRU缓存
    LRU_Cache cache(3);

    //出入3个数据，缓存已满
    cache.put("key1","val1");
    cache.put("key2","val2");
    cache.put("key3","val3");

    //访问key1->key1变成【最近最少使用队列】的最尾部
    cache.get("key1");
    //插入新数据key4，触发淘汰：最久未访问的key2被移除
    cache.put("key4","val4");

    //断言：key2被淘汰
    EXPECT_EQ(cache.get("key2"),"");
    //断言：key1因被访问国，保留在缓存中
    EXPECT_EQ(cache.get("key1"),"val1");
}