#include "Test.h"

void Test::run(int argc, char* argv[]) {
    LOG_INFO("进入测试模式");
    if (argc < 3) {
        LOG_INFO("用法: ./http_server test <测试名>，例如 test lru");
        return;
    }
    std::string testName = argv[2];
    if (testName == "lru") {
        testLRU();
    } else {
        LOG_WARN("未知测试: " + testName);
    }
}

void Test::testLRU() {
    LOG_INFO("开始测试 LRU_Cache");
    LRU_Cache cache(3);  // 容量 3

    // 测试 put 和 get
    cache.put("key1", "val1");
    cache.put("key2", "val2");
    cache.put("key3", "val3");
    LOG_INFO("插入 key1, key2, key3");

    std::string val = cache.get("key1");
    if (!val.empty()) {
        LOG_INFO("get key1: " + val);
    }

    // 插入第四个，淘汰 key2
    cache.put("key4", "val4");
    LOG_INFO("插入 key4，容量满，应淘汰最久未用");

    val = cache.get("key2");
    if (val.empty()) {
        LOG_INFO("key2 被淘汰，get 返回空");
    }

    LOG_INFO("LRU 测试完成");
}
