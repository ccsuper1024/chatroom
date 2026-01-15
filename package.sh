#!/bin/bash

# ChatRoom 打包脚本
# 用于生成客户端和服务器的独立发布包

set -e

# 配置
BUILD_DIR="build"
DIST_DIR="dist"
VERSION="1.0.0"

# 颜色
GREEN='\033[0;32m'
NC='\033[0m'

echo "======================================="
echo "      ChatRoom 打包程序"
echo "======================================="

# 1. 编译项目
echo -e "${GREEN}[1/4] 正在编译项目...${NC}"
mkdir -p $BUILD_DIR
cd $BUILD_DIR
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..

# 2. 准备目录结构
echo -e "${GREEN}[2/4] 准备发布目录...${NC}"
rm -rf $DIST_DIR
mkdir -p $DIST_DIR/client
mkdir -p $DIST_DIR/server

# 3. 打包客户端
echo -e "${GREEN}[3/4] 打包客户端...${NC}"
cp $BUILD_DIR/client/chatroom_client $DIST_DIR/client/

# 创建客户端启动脚本
cat > $DIST_DIR/client/start_client.sh << 'EOF'
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
EOF
chmod +x $DIST_DIR/client/start_client.sh

# 创建客户端压缩包
cd $DIST_DIR
tar -czf chatroom-client-$VERSION.tar.gz client/
echo "生成的包: $DIST_DIR/chatroom-client-$VERSION.tar.gz"
cd ..

# 4. 打包服务器
echo -e "${GREEN}[4/4] 打包服务器...${NC}"
cp $BUILD_DIR/server/chatroom_server $DIST_DIR/server/

# 创建服务器启动脚本
cat > $DIST_DIR/server/start_server.sh << 'EOF'
#!/bin/bash
# 启动聊天室服务器

PORT=${1:-8080}

chmod +x chatroom_server
echo "启动服务器，端口: $PORT"
./chatroom_server "$PORT"
EOF
chmod +x $DIST_DIR/server/start_server.sh

# 创建服务器压缩包
cd $DIST_DIR
tar -czf chatroom-server-$VERSION.tar.gz server/
echo "生成的包: $DIST_DIR/chatroom-server-$VERSION.tar.gz"
cd ..

echo "======================================="
echo -e "${GREEN}打包完成！${NC}"
echo "客户端包: dist/chatroom-client-$VERSION.tar.gz"
echo "服务器包: dist/chatroom-server-$VERSION.tar.gz"
echo "======================================="
