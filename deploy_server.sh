#!/bin/bash

# 部署服务器到远程机器并自动运行
# 使用方法: ./deploy_server.sh [port]

set -e

REMOTE_HOSTS=("192.168.233.128" "192.168.233.132")
REMOTE_DIR="~/code/cplusplus/chatroom"
SSH_BASE_OPTS="-o ControlMaster=auto -o ControlPersist=600 -o ControlPath=~/.ssh/cm-%r@%h-%p"

SERVER_PORT=${1:-8080}

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "======================================="
echo "     部署聊天室服务器到远程服务器"
echo "======================================="
echo ""

PACKAGE_FILE=$(ls dist/chatroom-server-*.tar.gz 2>/dev/null | head -n 1)

if [ -z "$PACKAGE_FILE" ]; then
    echo -e "${RED}错误: 服务器发布包不存在！${NC}"
    echo -e "${YELLOW}请先运行打包脚本：${NC}"
    echo "  ./package.sh"
    exit 1
fi

echo -e "${GREEN}✓ 找到服务器发布包: $PACKAGE_FILE${NC}"
echo ""

for HOST in "${REMOTE_HOSTS[@]}"; do
    echo "--------------------------------------"
    echo -e "${YELLOW}正在部署到: $HOST${NC}"
    echo "--------------------------------------"

    if ! ping -c 1 -W 2 "$HOST" > /dev/null 2>&1; then
        echo -e "${RED}✗ 无法连接到 $HOST (网络不可达)${NC}"
        echo ""
        continue
    fi

    echo -e "${GREEN}✓ 主机可达${NC}"

    echo "创建远程目录..."
    ssh $SSH_BASE_OPTS "$HOST" "mkdir -p $REMOTE_DIR" 2>/dev/null

    echo "上传服务器包..."
    if scp $SSH_BASE_OPTS -q "$PACKAGE_FILE" "$HOST:$REMOTE_DIR/" 2>/dev/null; then
        echo -e "${GREEN}✓ 服务器包已上传${NC}"
    else
        echo -e "${RED}✗ 上传失败${NC}"
        echo ""
        continue
    fi

    echo "配置远程服务器..."
    PACKAGE_NAME=$(basename "$PACKAGE_FILE")
    ssh $SSH_BASE_OPTS "$HOST" "cd $REMOTE_DIR && tar -xzf $PACKAGE_NAME && chmod +x server/start_server.sh" 2>/dev/null

    echo "检查已有服务器进程..."
    RUNNING=$(ssh $SSH_BASE_OPTS "$HOST" "pgrep -f chatroom_server" 2>/dev/null || echo "")

    if [ -n "$RUNNING" ]; then
        echo -e "${YELLOW}! 检测到已运行的服务器进程 (PID: $RUNNING)${NC}"
        echo "  停止旧进程..."
        ssh $SSH_BASE_OPTS "$HOST" "pkill -f chatroom_server" 2>/dev/null || true
        sleep 1
    fi

    echo "在后台启动服务器，端口: $SERVER_PORT ..."
    START_CMD="cd $REMOTE_DIR/server && ./start_server.sh $SERVER_PORT"
    ssh $SSH_BASE_OPTS "$HOST" "nohup bash -c '$START_CMD' > $REMOTE_DIR/server/chatroom_server.log 2>&1 &" 2>/dev/null

    echo -e "${GREEN}✓ 服务器部署并启动完成: $HOST${NC}"
    echo ""
done

echo "======================================="
echo -e "${GREEN}服务器部署任务完成！${NC}"
echo "======================================="
echo ""
echo "提示："
echo "  1. 服务器日志: ssh <host> 'tail -f $REMOTE_DIR/server/chatroom_server.log'"
echo "  2. 客户端部署脚本 deploy_client.sh 使用的 REMOTE_HOSTS 与本脚本一致"
echo ""

