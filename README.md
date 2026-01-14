# HTTP聊天室项目

基于HTTP协议的多人聊天室，使用C++20实现。

## 特性

- 使用C++20标准
- 基于HTTP协议通信
- JSON数据格式
- 支持多人同时在线
- 命令行客户端
- 使用CMake构建
- 集成测试框架（gtest）
- 日志系统（spdlog）

## 项目结构

```
chatroom/
├── CMakeLists.txt          # 顶层CMake配置文件
├── README.md               # 项目文档
├── QUICKSTART.md           # 快速开始指南
├── .gitignore              # Git忽略配置
├── start_server.sh         # 服务器启动脚本
├── start_client.sh         # 客户端启动脚本
│
├── server/                 # 服务器代码目录
│   ├── CMakeLists.txt      # 服务器CMake配置
│   ├── include/            # 服务器头文件
│   │   ├── http_server.h
│   │   ├── chatroom_server.h
│   │   └── json_utils.h
│   ├── src/                # 服务器源代码
│   │   ├── http_server.cpp
│   │   ├── chatroom_server.cpp
│   │   └── server_main.cpp
│   └── tests/              # 服务器测试代码
│       └── chatroom_test.cpp
│
├── client/                 # 客户端代码目录
│   ├── CMakeLists.txt      # 客户端CMake配置
│   ├── include/            # 客户端头文件
│   │   └── chatroom_client.h
│   └── src/                # 客户端源代码
│       ├── chatroom_client.cpp
│       └── client_main.cpp
│
└── build/                  # 编译输出目录
    ├── server/
    │   ├── chatroom_server # 服务器程序
    │   └── chatroom_test   # 测试程序
    └── client/
        └── chatroom_client # 客户端程序
```

## 依赖库

- [spdlog](https://github.com/gabime/spdlog) - 日志库
- [nlohmann/json](https://github.com/nlohmann/json) - JSON解析库
- [googletest](https://github.com/google/googletest) - 测试框架

所有依赖库通过CMake的FetchContent自动下载。

## 编译

```bash
# 进入项目目录
cd chatroom

# 创建构建目录
mkdir build
cd build

# 配置和编译
cmake ..
make -j$(nproc)
```

## 运行

### 启动服务器

#### 1. 启动服务器
```bash
cd build
./server/chatroom_server 8080
```

### 启动客户端

#### 2. 启动客户端
```bash
cd build
./client/chatroom_client 127.0.0.1 8080
```

### 使用说明

1. 启动客户端后，输入用户名登录
2. 输入消息并按回车发送
3. 输入 `/quit` 退出聊天室

## 运行测试

```bash
# 在build目录下
./server/chatroom_test

# 或使用ctest
ctest --output-on-failure
```

## API接口

### POST /login
登录接口

请求体：
```json
{
    "username": "用户名"
}
```

响应：
```json
{
    "success": true,
    "message": "登录成功",
    "username": "用户名"
}
```

### POST /send
发送消息接口

请求体：
```json
{
    "username": "用户名",
    "content": "消息内容"
}
```

响应：
```json
{
    "success": true,
    "message": "消息发送成功"
}
```

### GET /messages
获取消息接口

响应：
```json
{
    "success": true,
    "messages": [
        {
            "username": "用户名",
            "content": "消息内容",
            "timestamp": "2026-01-14 15:30:45"
        }
    ]
}
```

### GET /users
获取在线用户列表

响应：
```json
{
    "success": true,
    "users": []
}
```

## 开发说明

- 服务器使用简单的HTTP/1.1协议实现
- 消息存储在内存中（重启后丢失）
- 客户端定时轮询获取新消息（每秒一次）
- 线程安全：使用互斥锁保护共享数据

## 待改进

- [ ] 实现WebSocket支持，实现真正的实时通信
- [ ] 添加用户身份验证
- [ ] 持久化消息存储
- [ ] 维护在线用户列表
- [ ] 添加私聊功能
- [ ] 支持文件传输
- [ ] 添加更多单元测试

## 许可证

MIT License
