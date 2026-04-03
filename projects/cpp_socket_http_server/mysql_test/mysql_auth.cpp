#include<iostream>
#include<string>        //string依赖
#include<cstdlib>       //rand依赖
#include<cstring>       //字符串函数依赖
#include<cstdio>        //c标准输入输出依赖
#include<openssl/sha.h> //OpenSSL加密库头文件依赖
#include<mysql/mysql.h> //MYSQL C语言API头文件依赖

//工具1：生成16位随机盐
//作用：给每个用户生成唯一随机盐，避免相同密码产生相同哈希值
void generate_salt(char *salt,int length=16){
  //盐值可用字符集：字母+数字
  const char *chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  //计算字符集总长度
  int chars_len=strlen(chars);

  //循环生成指定长度的随机盐
  for(int i=0;i<length;i++){
    //随机取一个字符，存入salt数组
    salt[i]=chars[rand()%chars_len];
  }
  //在字符串末尾添加结束符，保证是合法C字符串
  salt[length]='\0';
}


//工具2：对输入的明文字符串进行SHA256哈希计算
//作用：将明文密码不可逆加密，保证密码不明文存储
std::string sha256(const std::string &input){
  //定义数组存储SHA256原始哈希结果，固定长度32位
  unsigned char hash[SHA256_DIGEST_LENGTH];

  //调用OpenSSL的SHA256函数，计算输入字符串的哈希值
  SHA256((const unsigned char*)input.c_str(),input.length(),hash);

  //定义缓冲区存储最终64位十六进制字符串
  char buf[65]={0};

  //将32字节原始哈希转为64为十六进制字符串
  for(int i=0;i<SHA256_DIGEST_LENGTH;++i){
    sprintf(buf+i*2,"%02x",hash[i]);
  }

  //返回哈希结果字符串
  return std::string(buf);
}


//工具3：密码加盐哈希
//作用：提升密码安全性，防止彩虹表攻击
std::string hash_password(const std::string &password,const std::string &salt){
  //密码+盐拼接后，再进行哈希计算
  return sha256(password+salt);
}


//全局变量：MySQL数据库连接句柄
//作用：保存数据库连接，供所有函数使用
MYSQL *g_conn=nullptr;

//初始化并创建MySQL数据库连接
bool init_mysql(){
  //初始化MySQL连接对象
  g_conn=mysql_init(nullptr);
  if(!g_conn) {
    printf("mysql_init 初始化失败\n");
    return false;
  }

  //连接MySQL数据库
  if(!mysql_real_connect(
    g_conn,               //MySQL连接句柄
    "localhost",          //数据库地址
    "root",               //数据库用户名
    "123456",             //数据库密码
    "http_server",        //要连接的数据库名
    3306,                 //MySQL默认端口
    NULL,                 //UNIX套接字，本地连接填NULL
    0                     //客户端标志，默认0
  )) {
       //连接失败，打印错误信息
       printf("Mysql connect 连接失败：%s\n",mysql_error(g_conn));
       return false;
  }

  printf("MySQL 连接成功\n");  
  return true;

}

//用户注册：将用户名和加密密码存入数据库
bool register_user(const std::string &username,const std::string &password){
  //定义数组存储16位盐值
  char salt[17];
  //生成随机盐
  generate_salt(salt);
  //对密码进行加盐哈希
  std::string pwd_hash=hash_password(password,salt);

  //定义SQL豫剧缓冲区
  char sql[512];
  //拼接INSERT SQL语句：将用户名、哈希密码、盐存入数据库
  snprintf(sql,sizeof(sql),
    "INSERT INTO users (username,password_hash,salt) VALUES('%s','%s','%s')",
    username.c_str(),
    pwd_hash.c_str(),
    salt
    );

  //执行SQL语句
  if(mysql_query(g_conn,sql)){
    //执行失败，打印错误信息
    printf("注册失败：%s\n",mysql_error(g_conn));
    return false;
  }

  printf("注册成功\n");
  return true;
}

//用户登陆验证
//逻辑：根据用户名查询数据库->取出盐和哈希密码->对输入密码加密->对比是否一致
bool login_user(const std::string &username,const std::string &password){
  //定义SQL语句缓冲区
  char sql[512];
  //拼接SELECT SQL语句，根据用户名查询哈希密码和盐
  snprintf(sql,sizeof(sql),
    "SELECT password_hash,salt FROM users WHERE username='%s'",
    username.c_str()
  );

  //执行SQL查询
  if(mysql_query(g_conn,sql)){
    return false;
  }

  //获取查询结果集
  MYSQL_RES *res=mysql_store_result(g_conn);
  if(!res) return false;

  //从结果集读取一行数据
  MYSQL_ROW row=mysql_fetch_row(res);
  if(!row){
    //没有找到该用户
    mysql_free_result(res);
    return false;
  }

  //从数据库中取出存储的哈希密码和盐
  std::string db_hash=row[0];
  std::string db_salt=row[1];

  //使用相同规则对用户输入的密码进行加密
  std::string input_hash=hash_password(password,db_salt);

  //释放结果采集内存，避免内存泄漏
  mysql_free_result(res);

  //对比两个哈希值，和同则密码正确
  return input_hash==db_hash;
}


//主函数：程序入口，用于测试注册和登录功能
int main(){
  srand(time(NULL));

  //初始化数据库连接
  if(!init_mysql()) return -1;

  //测试注册用户，用户名testuser，密码123456
  register_user("testuser","123456");

  //测试登录验证
  if(login_user("testuser","123456"))
    printf("登录成功\n");
  else
    printf("登录失败\n");

  //关闭数据库连接
  mysql_close(g_conn);
  return 0;
}
