#pragma once
#include <string>
#include "../utils/Logger.h"
#include "../utils/LRU_Cache.h"

class Test {
public:
    static void run(int argc, char* argv[]);
private:
    static void testLRU();
    // 可以添加更多测试函数，如 testDB(), testUser() 等
};
