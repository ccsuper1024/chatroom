#!/bin/bash

# 停止聊天室服务器脚本

# 查找服务器进程ID
PID=$(pgrep -f "chatroom_server")

if [ -z "$PID" ]; then
    echo "未发现正在运行的聊天室服务器。"
    exit 0
fi

echo "正在停止聊天室服务器 (PID: $PID)..."

# 发送 SIGTERM 信号
kill $PID

# 等待进程结束
for i in {1..5}; do
    if ! kill -0 $PID 2>/dev/null; then
        echo "服务器已成功停止。"
        exit 0
    fi
    sleep 1
done

# 如果还在运行，强制杀死
if kill -0 $PID 2>/dev/null; then
    echo "服务器未响应，正在强制停止..."
    kill -9 $PID
    echo "服务器已强制停止。"
fi
