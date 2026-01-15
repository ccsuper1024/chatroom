#!/bin/bash
# 启动聊天室服务器

PORT=${1:-8080}

chmod +x chatroom_server
echo "启动服务器，端口: $PORT"
./chatroom_server "$PORT"
