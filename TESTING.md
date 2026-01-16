# 测试与压力测试说明

本文档说明如何在本地对聊天室项目进行测试，包括：
- 单元测试（gtest）
- 多客户端并发压力测试脚本

## 1. 前置条件

- 已在本机安装 CMake 与编译工具链（g++ 等）
- 已完成项目编译

编译示例（在 `chatroom/` 目录下）：

```bash
cd chatroom
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

## 2. 单元测试

服务器端集成了基于 gtest 的单元测试（目标：`chatroom_test`）。

在 `chatroom/` 目录下执行：

```bash
cd chatroom/build
ctest        # 或直接 ./server/chatroom_test
```

如果你使用 IDE 或 CMake GUI，也可以通过相应的测试入口运行 `chatroom_test`。

## 3. 并发压力测试

为方便验证服务器在高并发场景下的表现，提供了一个自动化压力测试脚本：

- 脚本路径：`chatroom/stress_test_clients.sh`
- 作用：启动多个客户端进程，自动输入用户名和消息，持续向服务器发送请求

### 3.1 脚本参数说明

在 `chatroom/` 目录下执行：

```bash
./stress_test_clients.sh [server_host] [server_port] [client_count] [messages_per_client] [interval_seconds]
```

- `server_host`：服务器地址，默认 `192.168.233.130`
- `server_port`：服务器端口，默认 `8080`
- `client_count`：模拟的并发客户端数量，默认 `20`
- `messages_per_client`：每个客户端发送的消息条数，默认 `50`
- `interval_seconds`：两条消息之间的时间间隔（秒），默认 `0.1`

脚本会在 `chatroom/logs/stress/` 目录下为每个客户端生成一份日志，例如：

- `logs/stress/client_1.log`
- `logs/stress/client_2.log`
- ...

### 3.2 压力测试前准备

1. 确保服务器已启动，例如：

```bash
cd chatroom
./start_server.sh 8080
```

2. 确保客户端已编译（`build/client/chatroom_client` 存在）。

### 3.3 示例：连接默认服务器 IP 进行 50 客户端压力测试

在一个终端中启动服务器（如部署在 `192.168.233.130` 上的 8080 端口）：

```bash
cd chatroom
./start_server.sh 8080
```

在另一个终端中启动压力测试：

```bash
cd chatroom
./stress_test_clients.sh 192.168.233.130 8080 50 100 0.05
```

含义：

- 连接到服务器 `192.168.233.130:8080`
- 启动 `50` 个客户端进程
- 每个客户端发送 `100` 条消息
- 每条消息间隔 `0.05` 秒

### 3.4 结果观察与调优

- 服务器日志：查看 `logs/chatroom.log`（如果已启用日志）
- 压力客户端日志：查看 `logs/stress/` 目录下各客户端日志
- 系统资源：可结合 `top`、`htop`、`iostat` 等工具观察 CPU、内存、磁盘等指标

如果发现服务器在高并发下出现错误或性能瓶颈，可以根据日志与系统监控结果定位并优化：

- 调整最大并发连接数
- 优化线程池或事件循环配置
- 调整日志级别与日志输出频率
