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
  - 客户端目前一次 `recv` 读取服务器响应，然后通过 `\r\n\r\n` 切分 header/body，并直接返回剩余部分作为 JSON。
  - 潜在问题：
    - 响应较大或网络慢时，一次 `recv` 可能拿不到完整响应，导致 JSON 被截断；
    - 没有利用 `Content-Length` 做完整性校验。
  - 可改进方向：
    - 完整解析响应头，读取 `Content-Length`，循环 `recv`，直到读取指定长度；
    - 为将来扩展（如压缩、分块传输）预留解析结构。
- HTTP 请求解析与防御
  - 请求解析通过 `parseRequestFromBuffer` 完成，支持粘包处理，但尚未明确限制最大 header/body 长度。
  - 可改进方向：
    - 对单个请求体大小设置上限，超限时返回 400 并丢弃多余数据；
    - 对异常请求的错误信息做更精细分类，而不仅是通用 "bad request"。
- 连接重试策略
  - 客户端封装了发送/接收重试逻辑，失败时尝试重连并重发请求。
  - 可改进方向：
    - 将重试次数、重连间隔、超时时间统一放入配置（如 client.yaml）；
    - 记录包含服务器地址、路径的重试日志，便于排查网络问题。

### 3. 并发与性能

- EventLoop 扩展性
  - 当前 epoll 事件数组大小固定为 64，当并发连接很多且活跃事件过多时，需要多轮 epoll 才能处理完。
  - 可改进方向：
    - 当 `epoll_wait` 返回的事件数接近当前容量时，动态扩容事件数组；
    - 在需要时增加简单的统计（例如每轮处理的事件数量），便于评估负载情况。
- 线程池利用
  - 线程池参数从 `conf/server.yaml` 读取，但缺少运行时指标和错误统计。
  - 可改进方向：
    - 在线程池中增加队列长度、拒绝任务计数等指标，并暴露到 `/metrics`；
    - 在 README/QUICKSTART 中给出根据 CPU 核数调整线程配置的建议。
- 会话清理线程
  - 会话清理目前使用 `while(running_) + sleep_for` 固定间隔轮询。
  - 可改进方向：
    - 引入 `condition_variable` 等机制，在 stop 时主动唤醒，缩短退出时间；
    - 允许通过配置动态调整清理间隔，适配不同规模场景。

### 4. 安全性与滥用防护

- 输入校验
  - 用户名和消息内容几乎未做限制，主要依赖 JSON 解析是否成功。
  - 可改进方向：
    - 增加用户名和消息长度限制，过滤控制字符和不可见字符；
    - 在 HTTP 层对超长请求体直接拒绝，避免占用过多内存。
- 限流与反滥用
  - 当前系统尚未实现对单连接/单 IP 的频率限制。
  - 可改进方向：
    - 基于 `connection_id` 或 IP 简单统计单位时间内的请求数量，超限时限制发送或短暂封禁；
    - 将限流信息写入日志，便于分析异常流量。
- 错误信息暴露
  - 多数 handler 会直接把 `std::exception::what()` 写入响应 JSON 返回给客户端。
  - 可改进方向：
    - 对外只暴露通用错误描述和错误码，详细异常信息只写日志；
    - 统一错误返回格式，便于客户端处理。

### 5. 代码结构与测试

- 单元测试覆盖率
  - 目前 gtest 仅做了“能构造/能运行”的简单检查，没有覆盖具体业务逻辑。
  - 可改进方向：
    - 为 `ChatRoomServer` 的各个 handler 编写白盒测试，模拟 `HttpRequest`，验证消息、会话等内部状态变化；
    - 增加端到端测试：启动真实 server，用轻量 HTTP 客户端发请求，验证关键路径。
- 结构抽象
  - 聊天业务和监控统计（/metrics）均集中在 `ChatRoomServer` 中。
  - 可改进方向：
    - 抽离监控相关逻辑为独立组件（如 `MetricsCollector`），通过注入或组合方式使用；
    - 将来扩展新业务时，避免 ChatRoomServer 过于臃肿。
- 错误处理风格统一
  - 当前不同 handler 在状态码和错误结构上略有差异。
  - 可改进方向：
    - 设计统一的错误码枚举和错误响应结构；
    - 统一 4xx/5xx 的使用规则，区分客户端错误与服务端错误。

### 6. 配置与运维体验

- 配置集中化
  - 线程池、连接检查、心跳等配置分散在多个文件和 YAML 中。
  - 可改进方向：
    - 建立统一配置模块，负责加载 server/client 配置并提供访问接口；
    - 在文档中补充“推荐配置”和调优指南。
- 日志与监控
  - 当前已使用 spdlog 封装 Logger，接口统一。
  - 可改进方向：
    - 支持通过配置切换日志级别、输出目标（控制台/文件）；
    - 在 `/metrics` 中扩展更多运行时指标：例如请求计数、错误计数、线程池负载等。

### 7. 客户端交互体验

- CLI 命令丰富度
  - 目前客户端只支持普通消息和 `/quit`。
  - 可改进方向：
    - 增加 `/users`、`/stats`、`/help` 等命令，调用现有的 `/users`、`/metrics` 接口；
    - 为特殊命令增加简单的前缀约定，便于将来扩展。
- 退出流程与信号处理
  - 通过全局原子变量控制接收线程退出，未处理 Ctrl+C。
  - 可改进方向：
    - 捕获 SIGINT，在 handler 中设置 `g_running = false` 并输出提示；
    - 确保在所有线程退出后再关闭 socket，避免资源泄漏。

---

**重构完成时间**：2026-01-20  
**CMake配置**：✅ 通过  
**编译状态**：✅ 成功  
**测试状态**：✅ 全部通过

### 2026-01-20 改进记录
- **协议健壮性**：客户端 `sendHttpRequest` 现在完整解析 `Content-Length` 并循环读取响应，解决了大包截断问题；增加了 5 秒接收超时设置。
- **客户端交互**：新增 `/users`、`/stats`、`/help` 命令，支持 SIGINT 优雅退出。
- **配置与运维**：完成配置集中化 (ServerConfig) 和日志可配置化。
