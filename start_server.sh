#!/bin/bash

# 启动聊天室服务器脚本

# 获取脚本所在目录作为项目根目录
PROJECT_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$PROJECT_ROOT"

# 检查是否需要编译
if [ ! -d "build" ]; then
    echo "未检测到 build 目录，开始创建并编译..."
    mkdir build
    cd build
    cmake ..
    if [ $? -ne 0 ]; then
        echo "CMake 配置失败"
        exit 1
    fi
    make -j$(nproc)
    if [ $? -ne 0 ]; then
        echo "编译失败"
        exit 1
    fi
    cd ..
elif [ ! -f "build/bin/chatroom_server" ]; then
    echo "未检测到可执行文件，开始编译..."
    cmake --build build
    if [ $? -ne 0 ]; then
        echo "编译失败"
        exit 1
    fi
else
    echo "检测到已存在构建目录和可执行文件，跳过编译。"
fi

# 获取端口参数，默认8080
PORT=${1:-8080}

echo "===== 启动聊天室服务器 ====="
echo "工作目录: $(pwd)"
echo "端口: $PORT"
echo "按 Ctrl+C 停止服务器"
echo "============================"
echo ""

# 运行服务器
./build/bin/chatroom_server $PORT
