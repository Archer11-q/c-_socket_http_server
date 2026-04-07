#include<gtest/gtest.h>
#include"http/HttpHandler.h"


// 测试用例：解析HTTP请求路径
TEST(HttpHandlerTest,ParseHttpPath)
{
    //创建HttpHandler对象,比较提取的路径是否相等
    HttpHandler handler;

    //1.测试一：正常HTTP请求，提取静态页面路径
    std::string request1="GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(handler.parse_http_path(request1),"/index.html");

    //2.测试2：仅包含根路径的HTTP请求
    std::string request2="GET / HTTP/1.1\r\n\r\n";
    EXPECT_EQ(handler.parse_http_path(request2),"/");

    //3。测试3：POST请求，提取接口路径
    std::string request3="POST /api/login HTTP/1.1\r\n\r\n";
    EXPECT_EQ(handler.parse_http_path(request3),"/api/login");

    //4.测试4：非法格式请求，默认返回根路径
    std::string request4="INVALID REQUEST";
    EXPECT_EQ(handler.parse_http_path(request4),"/");
}


// 测试用例：解析HTTP请求方法
TEST(HttpHandlerTest,ParseHttpMethod)
{
    HttpHandler handler;

    //1.测试一：解析标准GET请求方法
    std::string request1="GET /index.html HTTP/1.1\r\n\r\n";
    EXPECT_EQ(handler.parse_http_method(request1),"GET");

    //2.测试二：解析标准POST请求方法
    std::string request2="POST /api/login HTTP/1.1\r\n\r\n";
    EXPECT_EQ(handler.parse_http_method(request2),"POST");

    //3.测试三：非法请求方法，默认返回GET
    std::string request3="INVALID_REQUEST_NO_SPACE";
    EXPECT_EQ(handler.parse_http_method(request3),"GET");
}


// 测试用例：判断HTTP请求是否接收完整
TEST(HttpHandlerTest,IsRequestComplete)
{
    HttpHandler handler;

    //1.测试一：携带完整头部和结束符的请求，应判定为完整
    const char* complete="GET /HTTP/1.1\r\nHost: localhost\r\n\r\n";
    EXPECT_TRUE(handler.isRequestComplete(complete,strlen(complete)));

    //2.测试二：缺少最后一组\r\n结束符，请求不完整
    const char* incomplete="GET /HTTP/1.1\r\nHost: localhost\r\n";
    EXPECT_FALSE(handler.isRequestComplete(incomplete,strlen(incomplete)));

    //3.测试三：仅包含请求方法的极短请求，不完整
    const char* short_req="GET";
    EXPECT_FALSE(handler.isRequestComplete(short_req,strlen(short_req)));
}


// 测试用例：判断是否需要保持长连接
TEST(HttpHandlerTest,ShouldKeepAlive)
{
    HttpHandler handler;

    //1.测试一：HTTP/1.1默认开启长连接
    std::string request1="GET / HTTP/1.1\r\n\r\n";
    EXPECT_TRUE(handler.shouldKeepAlive(request1));

    //2.测试二：请求头明确指定keep-alive，保持连接
    std::string request2="GET / HTTP/1.1\r\nConnection: keep-alive\r\n\r\n";
    EXPECT_TRUE(handler.shouldKeepAlive(request2));

    //3.测试三：请求头明确指定close，关闭连接
    std::string request3="GET / HTTP/1.1\r\nConnection: close\r\n\r\n";
    EXPECT_FALSE(handler.shouldKeepAlive(request3));
}




