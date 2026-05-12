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

  //1.从项目数据库获取原生MySQL连接
  MYSQL* conn=MySQLPool::getInstance().getConnection();
  if (!conn)
  {
    LOG_ERROR("数据库连接失败");
    return "{\"code\":-1,\"msg\":\"服务器异常\"}";
  }

  //2.创建预处理语句
  MYSQL_STMT* stmt=mysql_stmt_init(conn);
  if (!stmt)
  {
    LOG_ERROR("预处理语句创建失败");
    MySQLPool::getInstance().releaseConnection(conn);
    return "{\"code\":-1,\"msg\":\"服务器异常\"}";
  }

  //3.预处理SQL：检查用户名是否存在
  if (mysql_stmt_prepare(stmt,"SELECT * FROM users WHERE username=?",-1)!=0)
  {
    LOG_ERROR("预处理SQL失败");
    mysql_stmt_close(stmt);
    MySQLPool::getInstance().releaseConnection(conn);
    return "{\"code\":-1,\"msg\":\"服务器异常\"}";
  }

  //4.绑定参数：关联用户输入和SQL占位符
  MYSQL_BIND bind[1]={0};
  bind[0].buffer_type=MYSQL_TYPE_STRING;   //参数类型：字符串
  bind[0].buffer=(void*)username.c_str();  //用户输入的用户名
  bind[0].buffer_length=username.size();   //用户名长度
  mysql_stmt_bind_param(stmt,bind);        //将参数绑定到预处理语句

  //5.执行查询
  mysql_stmt_execute(stmt);                     //执行预处理语句，查询数据库中是否已有该用户名
  mysql_stmt_store_result(stmt);
  MYSQL_RES *res=mysql_stmt_result_metadata(stmt); //获取查询结构集

  //6.如果查询结果有记录，说明用户名已被占用
  if(mysql_num_rows(res)>0){
    mysql_free_result(res);   //释放查询结果集，避免内存泄漏
    mysql_stmt_close(stmt);
    MySQLPool::getInstance().releaseConnection(conn);
    std::string msg="[User] 注册失败：用户名已存在 -> "+username;
    LOG_WARN(msg);
    return "{\"code\":-1,\"msg\":\"用户名已存在\"}";
  }
  mysql_free_result(res);                           //释放结果集
  mysql_stmt_close(stmt);                           //关闭预处理语句，释放内存

  //7.生成密码和盐
  //生成16字节随机盐值，增强密码安全性，防止彩虹表攻击
  std::string salt=generateSalt(16);
  //使用SHA256算法对密码加盐后加密，得到安全的密码哈希值
  std::string safe_pwd=encryptPassword(password,salt);

  //8.重新初始化预处理语句，用于插入数据
  stmt=mysql_stmt_init(conn);
  if (!stmt) {
    LOG_ERROR("预处理语句创建失败");
    MySQLPool::getInstance().releaseConnection(conn);
    return "{\"code\":-1,\"msg\":\"服务器异常\"}";
  }
  //预处理插入SQL：用户名、密码、盐
  mysql_stmt_prepare(stmt,"INSERT INTO users(username,password,salt) VALUES(?,?,?)",-1);

  //绑定3个输入参数
  MYSQL_BIND insert_bind[3]={0};
  //绑定用户名
  insert_bind[0].buffer_type=MYSQL_TYPE_STRING;
  insert_bind[0].buffer=(void*)username.c_str();
  insert_bind[0].buffer_length=username.size();
  //绑定加密后的密码
  insert_bind[1].buffer_type=MYSQL_TYPE_STRING;
  insert_bind[1].buffer=(void*)safe_pwd.c_str();
  insert_bind[1].buffer_length=safe_pwd.size();
  //绑定盐值
  insert_bind[2].buffer_type=MYSQL_TYPE_STRING;
  insert_bind[2].buffer=(void*)salt.c_str();
  insert_bind[2].buffer_length=salt.size();

  mysql_stmt_bind_param(stmt,insert_bind);  //绑定所有参数
  mysql_stmt_execute(stmt);                 //安全执行插入
  mysql_stmt_close(stmt);                   //释放预处理语句资源
  MySQLPool::getInstance().releaseConnection(conn); //归还连接到连接池

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
    //1.获取原生数据库连接
    MYSQL* conn=MySQLPool::getInstance().getConnection();
    if(!conn){
      LOG_ERROR("[User] 登录失败：数据库连接失败");
      return "{\"code\":-1,\"msg\":\"服务器异常\"}";
    }

    //2.创建预处理语句
    MYSQL_STMT* stmt=mysql_stmt_init(conn);
    if(!stmt){
      LOG_ERROR("[User] 预处理语句创建失败");
      MySQLPool::getInstance().releaseConnection(conn);
      return "{\"code\":-1,\"msg\":\"服务器异常\"}";
    }

    //3.预处理SQL
    if (mysql_stmt_prepare(stmt,"SELECT password,salt FROM users WHERE username=?",-1)!=0)
    {
      LOG_ERROR("[User] 预处理SQL失败");
      mysql_stmt_close(stmt);
      MySQLPool::getInstance().releaseConnection(conn);
      return "{\"code\":-1,\"msg\":\"服务器异常\"}";
    }

    //4.绑定用户输入的用户名
    MYSQL_BIND bind[1]={0};
    bind[0].buffer_type=MYSQL_TYPE_STRING;
    bind[0].buffer=(void*)username.c_str();
    bind[0].buffer_length=username.size();
    mysql_stmt_bind_param(stmt,bind);

    //5.执行查询
    mysql_stmt_execute(stmt);                 //执行安全查询

    //6.定义缓冲区：存查询出来的password和salt
    char pwd_buf[256]={0};
    char salt_buf[256]={0};

    //7.定义结果绑定结构体，用来接收SELECT返回的两个字段
    MYSQL_BIND result_bind[2]={0};
    //绑定password
    result_bind[0].buffer_type=MYSQL_TYPE_STRING;  // 类型是字符串
    result_bind[0].buffer=pwd_buf;                 // 数据存到pwd_buf
    result_bind[0].buffer_length=sizeof(pwd_buf);  // 缓冲区大小
    //绑定salt
    result_bind[1].buffer_type=MYSQL_TYPE_STRING;
    result_bind[1].buffer=salt_buf;
    result_bind[1].buffer_length=sizeof(salt_buf);

    mysql_stmt_bind_result(stmt,result_bind);

    //8.从预处理结果中抓取一行数据到缓冲区
    if(mysql_stmt_fetch(stmt)!=0){
      //没有查到数据
      mysql_stmt_close(stmt);
      MySQLPool::getInstance().releaseConnection(conn);
      std::string msg = "[User] 登录失败：用户不存在 -> " + username;
      LOG_WARN(msg);
      return "{\"code\":-1,\"msg\":\"用户不存在\"}";
    }

    //9.把缓冲区赋值给业务变量
    db_pwd=pwd_buf;
    salt=salt_buf;

    //10.释放资源
    mysql_stmt_close(stmt); //关闭预处理语句
    MySQLPool::getInstance().releaseConnection(conn); //归还连接

    //11.缓存数据
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
