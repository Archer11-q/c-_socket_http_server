#include"DB.hpp"

//私有化构造，初始化连接为空
DB::DB(): _conn(nullptr){}

//单例模式，保证全局只有一个数据库连接
DB& DB::instance(){
  static DB ins;
  return ins;
}

//连接MySQL数据库
bool DB::connect(const std::string& host,const std::string& user,const std::string& pwd,const std::string& db) {
  //初始化MySQL对象
  _conn=mysql_init(nullptr);
  if(!_conn){
    LOG_ERROR("[DB] mysql_init 初始化失败");
    return false;
  }

  //建立实际连接
  if(!mysql_real_connect(_conn,host.c_str(),user.c_str(),pwd.c_str(),db.c_str(),3306,nullptr,0)){
    std::string msg="[DB] 数据库连接失败：%s"+std::string(mysql_error(_conn));
    LOG_ERROR(msg);
    return false;
  }

  //设置编码为utf8mb4，支持中文和表情
  mysql_set_character_set(_conn,"utf8mb4");
  LOG_INFO("[DB] 数据库连接成功");
  return true;
}

//执行查询SQL
MYSQL_RES* DB::query(const std::string& sql){
  if(!_conn){
    LOG_ERROR("[DB] 查询失败：数据库未连接");
    return nullptr;
  }

  //执行SQL语句
  if(mysql_query(_conn,sql.c_str())!=0){
    std::string err="[DB] 查询执行错误："+std::string(mysql_error(_conn))+", SQL: "+sql;
    LOG_ERROR(err);
    return nullptr;
  }

  //获取结果集并返回
  return mysql_store_result(_conn);
}

//执行更新类SQL
bool DB::execute(const std::string& sql){
  if(!_conn){
    LOG_ERROR("[DB] 执行失败：数据库未连接");
    return false;
  }

  if(mysql_query(_conn,sql.c_str())!=0){
    std::string err="[DB] 更新执行错误："+std::string(mysql_error(_conn))+", SQL: "+sql;
    LOG_ERROR(err);
    return false;
  }
  return true;
}

//关闭连接
void DB::close(){
  if(_conn){
    mysql_close(_conn);
    _conn=nullptr;
    LOG_INFO("[DB] 数据库连接已关闭");
  }
}

//析构自动关闭
DB::~DB(){
  //close();
}
