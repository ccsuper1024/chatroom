#!/bin/bash
# 启动聊天室客户端

if [ "$#" -lt 1 ]; then
    echo "用法: ./start_client.sh <服务器IP> [端口]"
    echo "示例: ./start_client.sh 127.0.0.1 8080"
    exit 1
fi

SERVER_HOST=$1
SERVER_PORT=${2:-8080}

chmod +x chatroom_client
./chatroom_client "$SERVER_HOST" "$SERVER_PORT"
