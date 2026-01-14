#!/bin/bash

# 部署客户端到远程服务器并自动运行
# 使用方法: ./deploy_client.sh [server_host] [server_port]

set -e

# 配置
REMOTE_HOSTS=("192.168.233.128" "192.168.233.132")
REMOTE_DIR="~/code/cplusplus/chatroom"
LOCAL_CLIENT="build/client/chatroom_client"

# 服务器配置（从参数获取，默认值）
SERVER_HOST=${1:-"192.168.233.1"}  # 聊天服务器地址
SERVER_PORT=${2:-8080}             # 聊天服务器端口

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "======================================="
echo "     部署聊天室客户端到远程服务器"
echo "======================================="
echo ""

# 检查客户端程序是否存在
if [ ! -f "$LOCAL_CLIENT" ]; then
    echo -e "${RED}错误: 客户端应用程序不存在！${NC}"
    echo -e "${YELLOW}请先编译项目：${NC}"
    echo "  cd build && cmake .. && make -j\$(nproc)"
    exit 1
fi

echo -e "${GREEN}✓ 找到客户端应用程序: $LOCAL_CLIENT${NC}"
echo ""

# 遍历每个远程主机
for HOST in "${REMOTE_HOSTS[@]}"; do
    echo "--------------------------------------"
    echo -e "${YELLOW}正在部署到: $HOST${NC}"
    echo "--------------------------------------"
    
    # 检查主机是否可达
    if ! ping -c 1 -W 2 "$HOST" > /dev/null 2>&1; then
        echo -e "${RED}✗ 无法连接到 $HOST (网络不可达)${NC}"
        echo ""
        continue
    fi
    
    echo -e "${GREEN}✓ 主机可达${NC}"
    
    # 创建远程目录
    echo "创建远程目录..."
    if ssh "$HOST" "mkdir -p $REMOTE_DIR" 2>/dev/null; then
        echo -e "${GREEN}✓ 远程目录已准备${NC}"
    else
        echo -e "${RED}✗ 无法创建远程目录 (可能需要配置SSH密钥)${NC}"
        echo ""
        continue
    fi
    
    # 复制客户端程序
    echo "上传客户端程序..."
    if scp -q "$LOCAL_CLIENT" "$HOST:$REMOTE_DIR/chatroom_client" 2>/dev/null; then
        echo -e "${GREEN}✓ 客户端程序已上传${NC}"
    else
        echo -e "${RED}✗ 上传失败${NC}"
        echo ""
        continue
    fi
    
    # 设置执行权限
    ssh "$HOST" "chmod +x $REMOTE_DIR/chatroom_client" 2>/dev/null
    
    # 检查远程主机是否已有客户端在运行
    echo "检查运行状态..."
    RUNNING=$(ssh "$HOST" "pgrep -f chatroom_client" 2>/dev/null || echo "")
    
    if [ -n "$RUNNING" ]; then
        echo -e "${YELLOW}! 检测到已运行的客户端进程 (PID: $RUNNING)${NC}"
        echo "  停止旧进程..."
        ssh "$HOST" "pkill -f chatroom_client" 2>/dev/null || true
        sleep 1
    fi
    
    # 在后台启动客户端
    echo "启动客户端连接到服务器 $SERVER_HOST:$SERVER_PORT ..."
    
    # 创建启动脚本
    ssh "$HOST" "cat > $REMOTE_DIR/start_client.sh << 'EOF'
#!/bin/bash
cd $REMOTE_DIR
echo \"正在连接到聊天服务器 $SERVER_HOST:$SERVER_PORT ...\"
echo \"客户端已启动，请输入用户名登录\"
exec ./chatroom_client $SERVER_HOST $SERVER_PORT
EOF
chmod +x $REMOTE_DIR/start_client.sh" 2>/dev/null
    
    # 在screen或tmux会话中启动（如果可用）
    if ssh "$HOST" "command -v screen" > /dev/null 2>&1; then
        ssh "$HOST" "screen -dmS chatroom bash -c 'cd $REMOTE_DIR && ./start_client.sh'" 2>/dev/null
        echo -e "${GREEN}✓ 客户端已在screen会话中启动${NC}"
        echo -e "  ${YELLOW}连接到会话: ssh $HOST -t 'screen -r chatroom'${NC}"
    elif ssh "$HOST" "command -v tmux" > /dev/null 2>&1; then
        ssh "$HOST" "tmux new-session -d -s chatroom 'cd $REMOTE_DIR && ./start_client.sh'" 2>/dev/null
        echo -e "${GREEN}✓ 客户端已在tmux会话中启动${NC}"
        echo -e "  ${YELLOW}连接到会话: ssh $HOST -t 'tmux attach -t chatroom'${NC}"
    else
        # 直接后台运行（不推荐，因为无法交互）
        ssh "$HOST" "cd $REMOTE_DIR && nohup ./chatroom_client $SERVER_HOST $SERVER_PORT > chatroom.log 2>&1 &" 2>/dev/null
        echo -e "${YELLOW}! 客户端已在后台启动（无交互模式）${NC}"
        echo -e "  ${YELLOW}建议安装screen或tmux以支持交互式客户端${NC}"
    fi
    
    echo -e "${GREEN}✓ 部署完成: $HOST${NC}"
    echo ""
done

echo "======================================="
echo -e "${GREEN}部署任务完成！${NC}"
echo "======================================="
echo ""
echo "提示："
echo "  1. 客户端需要交互式输入用户名和消息"
echo "  2. 使用screen或tmux连接到远程会话进行交互"
echo "  3. 查看日志: ssh <host> 'cat $REMOTE_DIR/chatroom.log'"
echo ""
