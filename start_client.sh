#!/bin/bash

# 启动聊天室客户端脚本

cd "$(dirname "$0")/build"

# 检查是否已编译
if [ ! -f "./client/chatroom_client" ]; then
    echo "错误：客户端未编译。请先运行 'cd build && make' 编译项目。"
    exit 1
fi

# 获取服务器地址和端口，默认127.0.0.1:8080
HOST=${1:-127.0.0.1}
PORT=${2:-8080}

echo "===== 启动聊天室客户端 ====="
echo "连接到: $HOST:$PORT"
echo "============================"
echo ""

./client/chatroom_client $HOST $PORT
