#pragma once
#include <string>


//用户业务模块
//专门处理：注册、登录、密码加密、MySQL验证
//HttpHandler 只调用这里的接口

#include "../utils/LRU_Cache.h"
#include"../utils/MySQLPool.h"      //获取原生MySQL连接

class User {
public:
    //用户注册
    static std::string registerUser(const std::string& username, const std::string& password);

    //用户登录
    static std::string loginUser(const std::string& username, const std::string& password);

private:
    //密码加密（内部使用，不对外暴露）
    static std::string encryptPassword(const std::string& password, const std::string& salt);

    //生成随机盐（内部使用）
    static std::string generateSalt(int len = 16);

    //静态LRU缓存：缓存用户名到密码哈希和盐的组合
    static LRU_Cache user_cache;
};
