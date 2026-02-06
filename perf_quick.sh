#!/bin/bash

#性能快速对比脚本: Fork/Select/Epoll三种模式，每种测试5秒，输出QPS指标
echo "=== 快速性能对比（每种模式测试5秒）===\n"

#定义统一测试函数：模式标识+模式名称
test_mode() {
  local mode=$1;  #模式标识
  local name=$2;  #模式名称

  echo -n "正在测试$name模式..."
  #后台启动服务器，日志重定向到日志文件，避免刷屏
  ./http_server $mode >> /home/archer/projects/cpp_socket_http_server/logs/http_server.log 2>&1 &
  local pid=$!
  sleep 3  #等待服务器完全初始化

  #wrk性能测试 2线程+50并发+5秒，访问静态文件
  #提取QPS指标并格式化输出
  wrk -t2 -c50 -d5s http://localhost:8080/index.html 2>/dev/null | \
  awk '/Requests\/sec/ {print name "模式 QPS：" $2} END {if(!NR) print name "模式 QPS：【测试失败，无数据】"}' name="$name"

  #终止服务器进程，清理资源，避免端口占用
  kill -9 $pid 2>/dev/null
  wait $pid 2>/dev/null 2>&1
  sleep 2 #等待端口释放，防止影响下一个模式测试
  echo -e "--------------------------------"
}

#依次测试三种模式
test_mode "fork" "Fork"
test_mode "select" "Select"
test_mode "epoll" "Epoll"

#测试完成总结
echo -e "=== 性能对比测试完成 ==="
echo -e "📌 预期量级：Fork~500-1000 | Select~2000-5000 | Epoll~8000-15000"
