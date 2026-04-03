#pragma once
#include<mysql/mysql.h> //mysql依赖
#include<string>        //字符串依赖
#include<cstdio>
#include"../utils/Logger.h"

class DB{
public:
  static DB& instance();  //获取单例实例

  //连接数据库
  bool connect(const std::string& host, //主机地址
               const std::string& user, //用户名
               const std::string& pwd,  //密码
               const std::string& db);  //数据库名

  //执行查询语句 SELECT
  MYSQL_RES* query(const std::string& sql);

  //执行更新语句：INSERT/UPDATE、DELETE
  bool execute(const std::string& sql);

  //关闭数据库连接
  void close();

  //析构函数自动关闭连接
  ~DB();
private:
  //私有化构造单例
  DB();

  //MySQL连接句柄
  MYSQL* _conn;
};
