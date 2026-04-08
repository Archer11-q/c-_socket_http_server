#include<gtest/gtest.h>
#include"../src/db/DB.cpp"


// 测试用例：数据库链接成功测试
TEST(DTest,Connect)
{
    //获取数据库单例实例
    DB &db=DB::instance();
    //尝试链接本地MySQL数据库
    bool result=db.connect("127.0.0.1","root","123456","http_server_db");
    //断言：预期数据库连接成功
    EXPECT_TRUE(result);

    //关闭数据库连接，释放资源
    db.close();
}

// 测试用例：数据库查询功能测试
TEST(DTets,Query)
{
    DB &db=DB::instance();

    //建立数据库连接
    db.connect("127.0.0.1","root","123456","http_server_db");

    //执行简单查询SQL。获取结果集指针
    MYSQL_RES *res=db.query("SELECT 1");

    //断言：预期查询结果集不为空
    EXPECT_NE(res,nullptr);

    //释放MySQL结果集内存，避免内存泄露
    mysql_free_result(res);
    db.close();
}

// 测试用例：数据库执行语句功能单元测试
TEST(DTest,Execute)
{
    DB &db=DB::instance();

    db.connect("127.0.0.1","root","123456","http_server_db");

    //执行建表SQL
    bool result=db.execute("CREATE TABLE IF NOT EXISTS test_table (id INT)");

    //断言：预期SQL执行成功
    EXPECT_TRUE(result);

    db.close();
}
