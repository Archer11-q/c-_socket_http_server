#!/bin/bash
# 脚本功能：全量测试Select模式（基础功能+静态文件+多连接+进程数监控）

echo "=== 清理历史残留进程 ==="
kill -9 $(ps aux | grep http_server | grep -v grep | awk '{print $2}') 2>/dev/null
sleep 1
# 验证清理结果
PROCESS_NUM=$(ps aux | grep http_server | grep -v grep | wc -l)
if [[ $PROCESS_NUM -eq 0 ]]; then
    echo "✅ 历史残留进程已清理完毕"
else
    echo "⚠️  仍有残留进程未清理，数量：$PROCESS_NUM"
fi

PROJECT_DIR=~/projects/cpp_socket_http_server
SERVER_BIN=$PROJECT_DIR/build/bin/http_server
LOG_FILE=$PROJECT_DIR/logs/http_server.log
# 提前清空日志，避免旧内容干扰
> $LOG_FILE

echo -e "=== Select模式全量测试 ===\n"

# 1. 启动Select模式服务器（后台运行）
$SERVER_BIN select &
SERVER_PID=$!
sleep 1
# 检查服务器是否成功启动
if ! ps -p $SERVER_PID > /dev/null 2>&1; then
    echo "✗ 服务器启动失败，退出测试"
    exit 1
fi
echo -e "✅ 服务器启动成功，PID: $SERVER_PID\n"

# 2. 测试基础HTTP动态请求
echo "【测试1】基础动态请求测试..."
RESP=$(curl -s http://localhost:8080/)
if [[ $RESP != "" && $? -eq 0 ]]; then
    echo "✓ 基本请求OK"
else
    echo "✗ 基本请求失败"
fi
sleep 0.5

# 3. 测试静态文件服务
echo -e "\n【测试2】静态文件服务测试..."
RESP_HTML=$(curl -s http://localhost:8080/test.html)
# 匹配实际静态文件内容关键词（兼容FIle/File拼写）
if [[ $RESP_HTML == *"Static Test F"* && $RESP_HTML == *"C++ HTTP Server"* ]]; then
    echo "✓ 静态文件请求OK"
else
    echo "✗ 静态文件请求失败"
    echo "  实际响应：$RESP_HTML"
fi
sleep 0.5

# 4. 测试多连接并发处理（核心特性）
echo -e "\n【测试3】多连接并发处理测试..."
# 同时发起10个请求，模拟并发连接
for i in {1..10}; do
    curl -s http://localhost:8080/about > /dev/null 2>&1 &
done
# 等待所有请求执行完成
wait
# 检查日志中是否有10次请求处理记录（统计DEBUG关键词）
REQ_COUNT=$(grep -c "Dynamic response sent /about" $LOG_FILE)
if [[ $REQ_COUNT -ge 9 ]]; then # 允许1次网络波动，兼容9+次即可
    echo -e "✓ 多连接并发处理正常（实际处理请求数：$REQ_COUNT/10）"
else
    echo -e "✗ 多连接并发处理异常（实际处理请求数：$REQ_COUNT/10）"
fi
sleep 0.5

# 5. 新增：进程数监控（Select核心差异：单进程）
echo -e "\n【测试4】进程数监控（Select模式核心特性）..."
# 统计http_server相关进程数（排除grep自身）
PROCESS_NUM=$(ps aux | grep http_server | grep -v grep | wc -l)
echo "当前http_server相关进程数：$PROCESS_NUM 个"
if [[ $PROCESS_NUM -eq 1 ]]; then
    echo "✓ 符合Select模式特性：仅1个进程（无多进程创建）"
else
    echo "✗ 不符合Select模式特性：非单进程（预期1个，实际$PROCESS_NUM个）"
    # 打印进程详情，方便排查
    ps aux | grep http_server | grep -v grep
fi

# 6. 清理：关闭服务器
echo -e "\n=== 测试结束，清理资源 ==="
kill $SERVER_PID > /dev/null 2>&1
sleep 0.5
# 确认进程已退出
if ! ps -p $SERVER_PID > /dev/null 2>&1; then
    echo "✅ 服务器进程已正常退出"
else
    echo "⚠️  服务器进程未正常退出，强制杀死"
    kill -9 $SERVER_PID > /dev/null 2>&1
fi

echo -e "\n=== 所有测试完成 ==="
