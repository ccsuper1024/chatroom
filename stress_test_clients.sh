#!/bin/bash

# 聊天室服务器自动化压力测试脚本
# 用多个客户端进程模拟并发连接与消息发送
# 用法:
#   ./stress_test_clients.sh [server_host] [server_port] [client_count] [messages_per_client] [interval_seconds]
#
# 示例:
#   ./stress_test_clients.sh 192.168.233.130 8080 50 100 0.05

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
CLIENT_BIN="$BUILD_DIR/client/chatroom_client"
LOG_DIR="$SCRIPT_DIR/logs/stress"

HOST=${1:-192.168.233.130}
PORT=${2:-8080}
CLIENT_COUNT=${3:-20}
MESSAGES_PER_CLIENT=${4:-50}
MESSAGE_INTERVAL=${5:-0.1}

mkdir -p "$LOG_DIR"

if [ ! -x "$CLIENT_BIN" ]; then
    echo "错误：未找到客户端可执行文件：$CLIENT_BIN"
    echo "请先在 chatroom 目录下执行："
    echo "  mkdir -p build && cd build"
    echo "  cmake .. && make -j\$(nproc)"
    exit 1
fi

echo "======================================="
echo "      启动聊天室压力测试"
echo "======================================="
echo "服务器地址     : $HOST:$PORT"
echo "客户端数量     : $CLIENT_COUNT"
echo "每个客户端消息 : $MESSAGES_PER_CLIENT"
echo "消息间隔(秒)   : $MESSAGE_INTERVAL"
echo "日志目录       : $LOG_DIR"
echo "======================================="

PIDS=()

for ((i = 1; i <= CLIENT_COUNT; ++i)); do
    USER="stress_user_$i"
    LOG_FILE="$LOG_DIR/client_${i}.log"

    (
        echo "$USER"
        for ((m = 1; m <= MESSAGES_PER_CLIENT; ++m)); do
            echo "[$USER] message $m at $(date +%s)"
            sleep "$MESSAGE_INTERVAL"
        done
    ) | "$CLIENT_BIN" "$HOST" "$PORT" >"$LOG_FILE" 2>&1 &

    PIDS+=($!)
    sleep 0.01
done

echo ""
echo "已启动 $CLIENT_COUNT 个客户端进程。"
echo "等待所有客户端发送完消息..."

FAIL_COUNT=0
for pid in "${PIDS[@]}"; do
    if ! wait "$pid"; then
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
done

echo ""
if [ "$FAIL_COUNT" -eq 0 ]; then
    echo "✅ 压力测试完成：所有客户端正常退出。"
else
    echo "⚠️ 压力测试完成：有 $FAIL_COUNT 个客户端异常退出，请检查日志目录：$LOG_DIR"
fi

echo "提示：可结合服务器日志与系统监控工具 (top, htop, iostat 等) 观察压力表现。"
