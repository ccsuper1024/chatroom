# 项目结构说明

## ✅ 重构完成

项目已成功重构为**服务器和客户端分离**的目录结构。

## 📂 新的目录结构

```
chatroom/
├── CMakeLists.txt              # 顶层CMake配置（子目录管理）
├── README.md                   # 完整项目文档
├── QUICKSTART.md               # 快速开始指南
├── STRUCTURE.md                # 本文件：项目结构说明
├── .gitignore                  # Git忽略配置
├── start_server.sh             # 服务器启动脚本
├── start_client.sh             # 客户端启动脚本
│
├── server/                     # 📦 服务器模块（独立）
│   ├── CMakeLists.txt          # 服务器专属CMake配置
│   ├── include/                # 服务器头文件
│   │   ├── http_server.h       # HTTP服务器实现
│   │   ├── chatroom_server.h   # 聊天室服务逻辑
│   │   └── json_utils.h        # JSON工具（共享）
│   ├── src/                    # 服务器源文件
│   │   ├── http_server.cpp
│   │   ├── chatroom_server.cpp
│   │   └── server_main.cpp     # 服务器入口
│   └── tests/                  # 服务器测试
│       └── chatroom_test.cpp
│
├── client/                     # 📱 客户端模块（独立）
│   ├── CMakeLists.txt          # 客户端专属CMake配置
│   ├── include/                # 客户端头文件
│   │   └── chatroom_client.h
│   └── src/                    # 客户端源文件
│       ├── chatroom_client.cpp
│       └── client_main.cpp     # 客户端入口
│
└── build/                      # 编译输出目录
    ├── server/
    │   ├── chatroom_server     # ✅ 服务器可执行文件
    │   └── chatroom_test       # ✅ 测试可执行文件
    └── client/
        └── chatroom_client     # ✅ 客户端可执行文件
```

## 🎯 重构优势

### 1. **模块化清晰**
- 服务器代码完全独立在 `server/` 目录
- 客户端代码完全独立在 `client/` 目录
- 每个模块有自己的 CMakeLists.txt

### 2. **易于维护**
- 各模块职责明确
- 便于团队协作开发
- 可以独立编译和测试

### 3. **可扩展性强**
- 容易添加新的模块（如管理后台、机器人客户端等）
- 每个模块可以独立部署
- 便于代码复用

### 4. **编译隔离**
- 编译产物按模块组织在 `build/server/` 和 `build/client/`
- 清晰的依赖关系

## 📋 CMake结构

### 顶层 CMakeLists.txt
```cmake
# 管理全局配置和第三方库
- 设置C++20标准
- 下载spdlog、nlohmann/json、googletest
- 引入server和client子目录
```

### server/CMakeLists.txt
```cmake
# 服务器模块配置
- 编译服务器库 libchatroom_server_lib.a
- 编译服务器可执行文件 chatroom_server
- 编译测试程序 chatroom_test
```

### client/CMakeLists.txt
```cmake
# 客户端模块配置
- 编译客户端库 libchatroom_client_lib.a
- 编译客户端可执行文件 chatroom_client
- 引用server/include中的json_utils.h
```

## 🏗️ 架构升级 (Architecture Upgrade)

### Multi-Reactor 模型
项目已从单线程 EventLoop 升级为 **Master-Slave Reactor** 多线程模型：
- **Master Loop (Main Thread)**: 负责监听端口、接受新连接 (`Acceptor`)。
- **Slave Loops (IO Threads)**: 负责已连接 socket 的读写事件 (`TcpConnection`)。
- **ThreadPool**: 使用 `EventLoopThreadPool` 管理 IO 线程池，默认开启 4 个工作线程。

### 存储层升级 (Storage Layer)
消息存储后端已从文件系统 (JSON) 迁移至 **SQLite 数据库**：
- **DatabaseManager**: 单例模式封装所有数据库操作。
- **SQLite3**: 采用轻量级嵌入式关系型数据库，支持高并发读写。
- **Schema**: 消息表包含自增 ID、用户名、内容、时间戳，支持高效的历史消息查询 (`since_id`)。
- **Reliability**: 解决了服务器重启后消息索引丢失的问题，数据更安全。
- **Worker Thread Pool**: 负责处理业务逻辑（如消息路由、数据库操作）。

这种架构显著提升了高并发场景下的吞吐量和响应速度，避免了 IO 操作阻塞主线程。

## 🔧 开发指南

### 编译整个项目
```bash
cd chatroom
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 只编译服务器
```bash
cd build
make chatroom_server -j$(nproc)
```

### 只编译客户端
```bash
cd build
make chatroom_client -j$(nproc)
```

### 运行测试
```bash
cd build
./server/chatroom_test
# 或
ctest --output-on-failure
```

## 📦 模块说明

### Server模块
**职责**：提供HTTP聊天室服务
- HTTP服务器实现（支持基本的GET/POST）
- 聊天室业务逻辑（消息存储、用户管理）
- RESTful API接口

**依赖**：
- spdlog（日志）
- nlohmann/json（JSON解析）
- pthread（线程支持）

### Client模块
**职责**：命令行聊天客户端
- HTTP客户端实现
- 命令行交互界面
- 消息接收线程

**依赖**：
- spdlog（日志）
- nlohmann/json（JSON解析）
- server/include/json_utils.h（共享头文件）

## 🚀 快速使用

1. **首次使用**：
   ```bash
   cd chatroom
   mkdir build && cd build
   cmake ..
   make -j$(nproc)
   cd ..
   ```

2. **启动服务器**（终端1）：
   ```bash
   ./start_server.sh        # 默认8080端口
   ```

3. **启动客户端**（终端2、3、4...）：
   ```bash
   ./start_client.sh        # 连接localhost:8080
   ```

## ✨ 下一步扩展建议

基于新的模块化结构，可以轻松添加：

1. **新的客户端类型**
   - `client_gui/` - 图形界面客户端
   - `client_bot/` - 聊天机器人

2. **新的服务模块**
   - `admin/` - 管理后台
   - `common/` - 共享工具库

3. **独立测试模块**
   - 移动 `tests/` 到顶层
   - 添加集成测试

## 🔍 当前可改进点概览

基于现有实现，项目已经具备一个功能完备的聊天室，但在功能体验、协议健壮性、并发性能和可维护性上还有不少演进空间。下面是当前版本的主要改进方向概览，便于后续重构时有清晰路径。

### 1. 聊天业务能力

- 消息历史管理
  - **状态**: 已实现
  - **实现细节**: 
    - 消息存放在 `std::vector<ChatMessage> messages_` 中。
    - 支持消息条数上限（`max_message_history`），自动淘汰旧消息。
    - 支持持久化：服务停止时保存至 `data/chat_history.json`，启动时加载。
- 获取消息接口设计
  - **状态**: 已实现
  - **实现细节**:
    - `/messages` 支持 `?since=N` 参数，只返回增量消息。
    - 服务端维护 `base_message_index_`，确保客户端即使在消息被淘汰后也能正确同步（返回剩余的最早消息）。
- 登录和用户会话
  - **状态**: 已实现
  - **实现细节**:
    - `handleLogin` 包含用户名合法性校验（长度、字符集）。
    - **唯一性检查**: 登录时检查用户名是否已被占用，防止重复登录。
    - 会话管理：使用 `connection_id` 标识会话，用户名登录后不可变更。
- 在线用户展示
  - **状态**: 已实现
  - **实现细节**:
    - 服务端 `/users` 接口返回用户列表，包含 `username`, `online_seconds`, `idle_seconds`。
    - 客户端 `/users` 命令解析 JSON 并格式化输出表格。
    - 服务端维护 `login_time` 和 `last_heartbeat` 以计算上述指标。

### 2. 协议健壮性与可靠性

- HTTP 响应读取
  - **状态**: 已实现
  - **实现细节**:
    - 客户端 `sendHttpRequest` 中实现了完整的响应读取逻辑。
    - 解析 `Content-Length`，循环 `recv` 直到接收完整 Body。
    - 支持自动重试机制。
- HTTP 请求解析与防御
  - **状态**: 已实现
  - **实现细节**:
    - `TcpConnection` 限制了最大请求体大小 (10MB)，超限返回标准 JSON 错误 (413)。
    - `parseRequestFromBuffer` 解析失败时返回标准 JSON 错误 (400)。
- 连接重试策略
  - **状态**: 已实现
  - **实现细节**:
    - 客户端封装了发送/接收重试逻辑。
    - 失败时尝试重连并重发请求。

### 3. 并发与性能

- EventLoop 扩展性
  - **状态**: 已实现
  - **实现细节**:
    - `EventLoop` 支持动态扩容 `epoll_event` 数组（当事件数满时翻倍）。
- 线程池利用
  - **状态**: 已实现
  - **实现细节**:
    - `HttpServer` 暴露了线程池队列大小、拒绝任务数等指标到 `/metrics`。
- 会话清理线程
  - **状态**: 已实现
  - **实现细节**:
    - 使用 `condition_variable` (`cleanup_cv_`) 实现等待。
    - 支持 `stop()` 时立即唤醒退出，无需等待完整周期。
    - 清理间隔可配置。

### 4. 安全性与滥用防护

- 输入校验
  - **状态**: 已实现
  - **实现细节**:
    - `validateUsername`: 限制长度和字符集（字母数字下划线）。
    - `validateMessage`: 限制长度和控制字符。
- 限流与反滥用
  - **状态**: 已实现
  - **实现细节**:
    - 基于 IP 的限流 (`checkRateLimit`)。
    - 支持配置窗口时间和最大请求数。
- 错误信息暴露
  - **状态**: 已实现
  - **实现细节**:
    - 全局统一使用 `CreateErrorResponse` 生成错误响应。
    - 即使是底层协议错误（如包过大、格式错误）也返回标准 JSON 格式。

### 5. 代码结构与测试

- 单元测试覆盖率
  - **状态**: 已实现
  - **现状**:
    - `tests/unit/server_test.cpp` 覆盖了输入校验、限流等核心逻辑。
    - `tests/integration/integration_test.cpp` 覆盖了基本聊天流程、消息持久化、**并发压力测试**和**异常安全测试**。
  - **计划**:
    - 持续补充边界用例。
- 结构抽象
  - **状态**: 部分实现
  - **实现细节**:
    - 监控逻辑已抽离为 `MetricsCollector`。
    - 配置逻辑已抽离为 `ServerConfig`。

- 错误处理风格统一
  - **状态**: 已实现
  - **实现细节**:
    - 定义了统一的 `ErrorCode` 枚举和 `AppError` 结构。
    - 提供了 `CreateErrorResponse` 辅助函数生成标准 JSON 错误响应。

### 6. 配置与运维体验

- 配置集中化
  - **状态**: 已实现
  - **实现细节**:
    - `ServerConfig` 单例管理所有服务端配置。
    - 支持从 YAML 加载配置。
- 日志与监控
  - **状态**: 已实现
  - **实现细节**:
    - 基于 spdlog 的统一日志封装。
    - `/metrics` 接口提供了丰富的运行时指标。

### 7. 客户端交互体验

- CLI 命令丰富度
  - **状态**: 已实现
  - **实现细节**:
    - 支持 `/users` (表格展示), `/stats` (服务器统计), `/help`。
    - 支持 `/quit` 退出。
- 退出流程与信号处理
  - **状态**: 已实现
  - **实现细节**:
    - 客户端捕获 SIGINT (Ctrl+C) 优雅退出。
    - 服务端 `stop()` 确保线程和资源正确释放。

---

**重构完成时间**：2026-01-20  
**CMake配置**：✅ 通过  
**编译状态**：✅ 成功  
**测试状态**：✅ 全部通过

### 2026-01-20 改进记录
- **协议健壮性**：客户端 `sendHttpRequest` 现在完整解析 `Content-Length` 并循环读取响应，解决了大包截断问题；增加了 5 秒接收超时设置。
- **客户端交互**：新增 `/users`、`/stats`、`/help` 命令，支持 SIGINT 优雅退出。
- **配置与运维**：完成配置集中化 (ServerConfig) 和日志可配置化。
  - **可观测性**：`/metrics` 接口已升级为 **Prometheus 格式**，支持直接对接 Grafana 监控大盘。
  - **测试增强**：新增 `IntegrationTest.PersistenceTest` (持久化验证)、`IntegrationTest.ConcurrencyTest` (并发压力验证) 和 `IntegrationTest.SecurityTest` (异常安全验证)。
- **错误处理统一**：底层协议错误（如包过大、格式错误）现在统一返回标准 JSON 错误格式，修复了直接断开连接导致客户端无法获取错误信息的问题。
