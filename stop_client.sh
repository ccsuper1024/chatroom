#!/bin/bash

# 停止聊天室客户端脚本

# 查找客户端进程ID
PIDS=$(pgrep -f "chatroom_client")

if [ -z "$PIDS" ]; then
    echo "未发现正在运行的聊天室客户端。"
    exit 0
fi

echo "正在停止聊天室客户端 (PIDS: $PIDS)..."

# 发送 SIGTERM 信号
kill $PIDS

# 等待进程结束
sleep 1

# 检查是否还有残留
PIDS_REMAINING=$(pgrep -f "chatroom_client")
if [ -n "$PIDS_REMAINING" ]; then
    echo "部分客户端未响应，正在强制停止..."
    kill -9 $PIDS_REMAINING
    echo "所有客户端已强制停止。"
else
    echo "所有客户端已停止。"
fi
