#!/bin/bash

# 启动聊天室服务器脚本

cd "$(dirname "$0")/build"

# 检查是否已编译
if [ ! -f "./server/chatroom_server" ]; then
    echo "错误：服务器未编译。请先运行 'cd build && make' 编译项目。"
    exit 1
fi

# 获取端口参数，默认8080
PORT=${1:-8080}

echo "===== 启动聊天室服务器 ====="
echo "端口: $PORT"
echo "按 Ctrl+C 停止服务器"
echo "============================"
echo ""

./server/chatroom_server $PORT
