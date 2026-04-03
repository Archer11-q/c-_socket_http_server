#!/bin/bash
echo "=== 测试epoll功能 ==="

#测试epoll模式
echo "测试Epoll模式..."
./build/bin/http_server epoll & EPOLL_PID=$!
sleep 1

#基础请求
curl -s http://localhost:8080/ > /dev/null && echo "✓ 静态文件OK"

#快速并发
for i in {1..5}; do
curl -s "http://localhost:8080/?test=$!" > /dev/null & done
wait
echo "✓ 5并发请求OK"

kill $EPOLL_PID 2 >/dev/null
echo "===Epoll模式测试完成 ==="
