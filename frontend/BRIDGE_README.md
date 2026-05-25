WebSocket <-> TCP 桥 使用说明

目的
- 提供一个浏览器可连接的 WebSocket 端点，并将消息翻译为后端 chat_server 使用的长度前缀 TCP 帧（4 字节大端）与其转发。

安装依赖 (推荐在项目根目录执行并使用虚拟环境)

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r frontend/requirements.txt
```

运行桥接（默认假设 chat_server 在本机 8080 端口）

```bash
# 在项目根目录
source .venv/bin/activate
python3 frontend/ws_bridge.py --server-host 127.0.0.1 --server-port 8080 --ws-host 127.0.0.1 --ws-port 8765
```

运行前端静态页面（可选）

```bash
# 在项目根目录
python3 -m http.server 4173 --directory frontend --bind 127.0.0.1
# 然后在浏览器打开 http://127.0.0.1:4173
```

说明
- 浏览器通过 WebSocket 发送 JSON 消息：{type:'join', room:100, username:'lin'} 或 {type:'msg', text:'hello'}。
- 桥接会把这些消息转换为后端的文本帧，例如 `CHAT|1|JOIN|100|lin`、`CHAT|1|MSG|hello`。
- 桥接把来自后端的 TCP 帧原样转发给浏览器，浏览器会收到 JSON 格式 {type:'frame', body:'...'}。
