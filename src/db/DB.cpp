#include"DB.hpp"

//私有化构造，初始化连接为空
DB::DB(){}

//单例模式，保证全局只有一个数据库连接
DB& DB::instance(){
  static DB ins;
  return ins;
}

//连接MySQL数据库
bool DB::connect(const std::string& host,const std::string& user,const std::string& pwd,const std::string& db) {
  try{
    MySQLPool::getInstance().init(host,user,pwd,db,8);
    return true;
  } catch (...){
    LOG_ERROR("[DB] 连接池初始化失败");
    return false;
  }
}

//执行查询SQL
MYSQL_RES* DB::query(const std::string& sql){
  MYSQL *conn=MySQLPool::getInstance().getConnection();
  if(!conn){
    LOG_ERROR("[DB] 查询失败：数据库未连接");
    return nullptr;
  }

  //执行SQL语句
  if(mysql_query(conn,sql.c_str())!=0){
    std::string err="[DB] 查询执行错误："+std::string(mysql_error(conn))+", SQL: "+sql;
    LOG_ERROR(err);
    return nullptr;
  }

  //获取结果集并返回
  MYSQL_RES* res=mysql_store_result(conn);

  //归还连接
  MySQLPool::getInstance().releaseConnection(conn);
  return res;
}

//执行更新类SQL
bool DB::execute(const std::string& sql){
  MYSQL* conn=MySQLPool::getInstance().getConnection();
  if(!conn){
    LOG_ERROR("[DB] 执行失败：数据库未连接");
    return false;
  }

  if(mysql_query(conn,sql.c_str())!=0){
    std::string err="[DB] 更新执行错误："+std::string(mysql_error(conn))+", SQL: "+sql;
    LOG_ERROR(err);
    return false;
  }
  //归还连接
  MySQLPool::getInstance().releaseConnection(conn);
  return true;
}

//关闭连接
void DB::close(){
  MySQLPool::getInstance().closePool();
}

//析构自动关闭
DB::~DB(){
  //close();
}
