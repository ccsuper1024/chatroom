# 部署脚本使用说明

## 📦 deploy_client.sh - 客户端部署脚本

### 功能
自动将编译好的客户端程序部署到远程服务器并启动运行。

### 目标服务器
- 192.168.233.128
- 192.168.233.132

### 使用方法

#### 1. 基本用法（使用默认配置）
```bash
./deploy_client.sh
```
默认连接到聊天服务器 192.168.233.1:8080

#### 2. 指定聊天服务器地址
```bash
./deploy_client.sh <服务器IP> <端口>
```

示例：
```bash
# 部署客户端，连接到192.168.233.1:8080的聊天服务器
./deploy_client.sh 192.168.233.1 8080

# 部署客户端，连接到本地聊天服务器
./deploy_client.sh 127.0.0.1 8080
```

### 脚本执行流程

1. ✅ **检查本地发布包** - 验证 `dist/chatroom-client-*.tar.gz` 是否存在
2. 🌐 **检测远程主机连通性** - ping测试每个目标服务器
3. 📁 **创建远程目录** - 在远程服务器创建 `~/code/cplusplus/chatroom`
4. 📤 **上传客户端程序** - 通过scp复制客户端到远程服务器
5. 🔄 **停止旧进程** - 如果检测到运行中的客户端，先停止
6. ▶️ **启动新客户端** - 在screen/tmux会话中启动客户端

### 前置要求

#### SSH密钥配置
确保已配置SSH密钥免密登录：

```bash
# 如果还没有SSH密钥，先生成
ssh-keygen -t ed25519

# 复制公钥到远程服务器
ssh-copy-id 192.168.233.128
ssh-copy-id 192.168.233.132
```

#### 远程服务器要求
- 安装screen或tmux（推荐，用于交互式客户端）
  ```bash
  # Ubuntu/Debian
  sudo apt install screen
  # 或
  sudo apt install tmux
  ```

### 连接到远程客户端

部署后，客户端会在screen或tmux会话中运行，您可以连接到会话进行交互：

#### 使用screen
```bash
ssh 192.168.233.128 -t 'screen -r chatroom'
```

#### 使用tmux
```bash
ssh 192.168.233.128 -t 'tmux attach -t chatroom'
```

#### 退出会话但保持运行
- Screen: 按 `Ctrl+A` 然后按 `D`
- Tmux: 按 `Ctrl+B` 然后按 `D`

### 查看日志

如果客户端在后台运行（无screen/tmux）：
```bash
ssh 192.168.233.128 'cat ~/code/cplusplus/chatroom/chatroom.log'
```

### 停止远程客户端

```bash
# 停止特定服务器的客户端
ssh 192.168.233.128 'pkill -f chatroom_client'

# 或终止screen会话
ssh 192.168.233.128 'screen -X -S chatroom quit'

# 或终止tmux会话
ssh 192.168.233.128 'tmux kill-session -t chatroom'
```

### 部署示例场景

#### 场景1: 测试环境（本地服务器）
```bash
# 1. 在本机启动服务器
./start_server.sh

# 2. 部署客户端到远程机器，连接到本机
./deploy_client.sh 192.168.233.1 8080

# 3. 连接到远程客户端进行交互
ssh 192.168.233.128 -t 'screen -r chatroom'
```

#### 场景2: 生产环境（独立服务器）
```bash
# 假设聊天服务器运行在 192.168.233.100:9000
./deploy_client.sh 192.168.233.100 9000
```

### 故障排查

#### 问题1: "客户端发布包不存在"
**解决方法**：
```bash
./package.sh
```

#### 问题2: "无法连接到远程主机"
**可能原因**：
- 网络不通：检查网络连接
- SSH配置问题：确保SSH密钥已配置
- 防火墙：检查防火墙设置

**解决方法**：
```bash
# 测试SSH连接
ssh 192.168.233.128 'echo "连接成功"'

# 配置SSH密钥
ssh-copy-id 192.168.233.128
```

#### 问题3: 客户端无法连接到聊天服务器
**检查**：
1. 聊天服务器是否运行
2. IP地址和端口是否正确
3. 防火墙是否允许连接

### 完整部署流程示例

```bash
# 1. 打包项目 (自动编译并生成发布包)
cd /home/chenchao/code/cplusplus/httpServer_Client/chatroom
./package.sh

# 2. 在本机启动聊天服务器 (使用打包好的脚本)
cd dist/server
./start_server.sh 8080 &
cd ../..

# 3. 获取本机IP（假设是192.168.233.1）
ip addr show | grep "inet 192.168"

# 4. 部署客户端到远程服务器
./deploy_client.sh 192.168.233.1 8080

# 5. 连接到第一个远程客户端
ssh 192.168.233.128 -t 'screen -r chatroom'
# 输入用户名: Alice

# 6. 在另一个终端，连接到第二个远程客户端  
ssh 192.168.233.132 -t 'screen -r chatroom'
# 输入用户名: Bob

# 现在Alice和Bob可以在各自的终端中聊天了！
```

### 高级选项

如需修改目标服务器列表，编辑 `deploy_client.sh` 中的配置：

```bash
# 修改这一行
REMOTE_HOSTS=("192.168.233.128" "192.168.233.132" "新服务器IP")
```

### 安全提醒

⚠️ **注意**：
- 确保只在可信网络中使用
- 建议使用SSH密钥而非密码认证
- 定期检查远程服务器的安全状态
