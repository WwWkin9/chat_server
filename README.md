# Chat Server

简要说明
- 这是一个基于 CMake 的 C++ 聊天服务器示例项目，包含核心服务实现、单元测试和一个简单的集成 smoke 测试。项目使用 SQLite 作为可选持久化后端（见 `PersistenceSqlite`）。

前提（Prerequisites）
- **CMake**：建议 >= 3.10
- **编译器**：支持 C++17 的 GCC 或 Clang
- **SQLite3**：可选，若启用持久化
- **Python 3**：可选，用于运行集成测试（`pytest`）

快速开始 — 构建

1. 在仓库根目录创建构建目录并生成构建文件：

```bash
mkdir -p build
cd build
cmake ..
```

2. 构建项目：

```bash
cmake --build . -- -j$(nproc)
```

或者直接从仓库根使用已有构建目录：

```bash
cmake --build build -- -j$(nproc)
```

构建产物位于 `build/` 目录。

运行服务
- 在 `build/` 中查找可执行文件（例如 `chat_server`）：

```bash
find build -type f -executable -maxdepth 3 -name "chat_server*"
```

- 运行（示例）：

```bash
./build/bin/chat_server --config ../config.yaml
```

（具体命令行选项请参见源码中的启动逻辑或 `--help`）

前端演示页
- 新增了一个独立的静态前端页面，位于 `frontend/index.html`。
- 直接在浏览器中打开该文件即可查看页面设计，后续也可以很方便地接入真实接口。

测试
- 使用 CTest 运行所有单元测试：

```bash
cd build
ctest --output-on-failure -j1
```

- 运行单个测试二进制可直接执行 `build/tests/...` 下的可执行文件。

- 运行集成 smoke 测试（Python）：

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install pytest
pytest tests/integration/test_smoke.py::test_smoke -q
```

调试与日志
- 日志与配置项散见 `src/Config.cpp`、`src/Logger.cpp`，可以通过修改配置或在源码中增加日志级别来排查问题。

源码结构（要点）
- `src/`：服务实现源代码
- `include/`：头文件
- `tests/`：单元测试与集成测试
- `build/`：CMake 构建输出（本地生成）

Git 与远程
- 仓库已初始化为 Git。若要添加远程并推送：

```bash
git remote add origin <REMOTE_URL>
git branch -M main
git push -u origin main
```

贡献
- 欢迎通过 Issue/PR 提交改进。请在 PR 中包含可复现的步骤和相关测试用例。

许可证
- 当前仓库未指定许可证。将仓库公开或分享前，请添加合适的 `LICENSE` 文件。

联系方式
- 如需帮助，可在仓库中打开 Issue 或直接联系维护者（见 GitHub 仓库页面）。
