#include"User.hpp"    
#include"../db/DB.hpp"      
#include"../utils/Logger.h" //日志输出依赖
#include<openssl/sha.h>     //C库 API依赖
#include<cstdlib>           //rand依赖
#include<ctime>             //随机时间依赖
#include<string>
#include<sstream>
#include<mysql/mysql.h>     //mysql依赖
#include<cstdio>

// 初始化静态LRU缓存，容量100
LRU_Cache User::user_cache(100);


//生成随机盐：用于密码加密，防止相同密码生成相同哈希
std::string User::generateSalt(int len) {
    //随机盐支持数字和字母形式
    const char chars[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string salt;
    salt.reserve(len);

    //只初始化一次随机种子
    static bool first=true;
    if (first){
        srand(time(nullptr));
        first=false;
        LOG_DEBUG("[User] 随机数种子初始化完成");
    }

    //随机选取字符组成盐值
    for (int i=0;i<len;++i){
        salt+=chars[rand()%62];
    }
    return salt;
}


// 密码加密：明文密码 + 盐 → 哈希密码（数据库不存明文）
std::string User::encryptPassword(const std::string& pwd, const std::string& salt) {
 //定义存储哈希结果的32字节数组
 unsigned char hash[SHA256_DIGEST_LENGTH];

 //声明SHA256上下文结构体，用于保存哈希计算过程中的中间状态
 SHA256_CTX sha256;

 //初始化SHA256上下文，准备开始计算
 SHA256_Init(&sha256);
 //将密码和盐值拼接成字符串，将内容更新到哈希计算中
 SHA256_Update(&sha256,(pwd+salt).c_str(),pwd.size()+salt.size());
 //完成哈希计算，将最终结果写入hash数组
 SHA256_Final(hash,&sha256);

 //准备用于存放结果字符串的变量
 std::string res;
 //临时缓冲区：用于格式化单个字节为两位十六进制数
 char buf[3];
 //遍历哈希数组的每个字节，将其转化为两位十六进制小写字符串并追加到结果中
  for(int i=0;i<SHA256_DIGEST_LENGTH;++i){
    sprintf(buf,"%02x",hash[i]);
    res+=buf;                //格式化的字符串追加到末尾
  }

  //返回最终的十六进制哈希字符串
  return res;
}


//用户注册：加密+MySQL
std::string User::registerUser(const std::string& username,const std::string& password){
  if(username.empty() || password.empty()){
    LOG_WARN("[User] 注册失败，参数不能为空");

    //返回JSON格式响应字符串：含结果码和消息
    return "{\"code\":-1,\"msg\":\"参数不能为空\"}";
  }

  //构建SQL查询语句，检查用户名是否已存在
  char sql[256];
  sprintf(sql,
    "SELECT * FROM users WHERE username='%s'",
    username.c_str()
  );
  MYSQL_RES* res=DB::instance().query(sql);

  //如果查询结果有记录，说明用户名已被占用
  if(mysql_num_rows(res)>0){
    mysql_free_result(res);   //释放查询结果集，避免内存泄漏
    std::string msg="[User] 注册失败：用户名已存在 -> "+username;
    LOG_WARN(msg);
    return "{\"code\":-1,\"msg\":\"用户名已存在\"}";
  }
  mysql_free_result(res); //释放结果集

  //生成16字节随机盐值，增强密码安全性，防止彩虹表攻击
  std::string salt=generateSalt(16);
  //使用SHA256算法对密码加盐后加密，得到安全的密码哈希值
  std::string safe_pwd=encryptPassword(password,salt);

  //构建INSERT语句，将用户名、加密后的密码和盐值存入数据库
  sprintf(sql,
    "INSERT INTO users(username,password,salt) VALUES('%s','%s','%s')",
     username.c_str(),
     safe_pwd.c_str(),
     salt.c_str()
  );
  DB::instance().execute(sql);    //执行插入操作

  //记录成功日志并返回响应
  std::string msg="[User] 注册成功 -> "+username;
  LOG_INFO(msg);
  return "{\"code\":0,\"msg\":\"注册成功\"}";
}


//用户登录：处理用户登录请求，验证用户名和密码
//密码验证流程：从数据库获取盐值—>对输入密码加盐加密->与数据库存储的哈希值比对
std::string User::loginUser(const std::string& username,const std::string& password) {
  if(username.empty() || password.empty()){
    LOG_WARN("[User] 登录失败：参数不能为空");
    return "{\"code\":-1,\"msg\":\"参数不能为空\"}";
  }

  // 先检查LRU缓存
  std::string cached_data = user_cache.get(username);
  std::string db_pwd, salt;
  if (!cached_data.empty()) {
    // 缓存命中，解析password:salt
    size_t pos = cached_data.find(':');
    if (pos != std::string::npos) {
      db_pwd = cached_data.substr(0, pos);
      salt = cached_data.substr(pos + 1);
    } else {
      // 缓存数据格式错误，从DB查询
      cached_data = "";
    }
  }

  if (cached_data.empty()) {
    // 缓存未命中，从数据库查询
    char sql[256];
    sprintf(sql,
      "SELECT password,salt FROM users WHERE username='%s'",
       username.c_str()
    );
    MYSQL_RES* res = DB::instance().query(sql);

    // 检查用户是否存在：查询结果为空说明不存在
    if (mysql_num_rows(res) == 0) {
      mysql_free_result(res);   // 释放结果集
      std::string msg = "[User] 登录失败：用户不存在 -> " + username;
      LOG_WARN(msg);
      return "{\"code\":-1,\"msg\":\"用户不存在\"}";
    }

    // 获取查询结果的第一行数据
    MYSQL_ROW row = mysql_fetch_row(res);
    db_pwd = row[0];       // 数据库中存储的加密密码
    salt = row[1];         // 该用户对应盐值
    mysql_free_result(res);          // 释放结果集

    // 缓存数据
    std::string cache_value = db_pwd + ":" + salt;
    user_cache.put(username, cache_value);
  }

  // 使用相同的盐值对用户输入的密码进行加密，得到哈希值
  std::string input_pwd = encryptPassword(password, salt);

  // 比对加密后的密码与数据库中存储的密码是否一致
  if (input_pwd == db_pwd) {
    // 密码验证通过，登录成功
    std::string msg = "[User] 登录成功 -> " + username;
    LOG_INFO(msg);
    return "{\"code\":0,\"msg\":\"登录成功\"}";
  } else {
      // 密码验证失败，登录失败
       std::string msg = "[User] 登录失败：密码错误 -> " + username;
       LOG_WARN(msg);
       return "{\"code\":-1,\"msg\":\"密码错误\"}";
  }
}
