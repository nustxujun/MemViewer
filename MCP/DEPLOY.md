# MemAnalyzer MCP 部署指南

本文档指导 AI Agent 或开发者将 MemAnalyzer MCP 服务部署到任意支持 MCP 协议的客户端。

---

## 1. 前置要求

| 依赖 | 最低版本 | 说明 |
|------|----------|------|
| **CMake** | 3.18+ | 构建系统 |
| **MSVC** (Visual Studio) | VS 2019+ | 需支持 C++20；当前仅支持 Windows |
| **Python** | 3.10 (x64) | 需包含开发头文件（`Python3 Development` 组件） |
| **pip** | — | 安装 Python 依赖 |

> **注意**：pybind11 生成的 `.pyd` 模块与编译时使用的 Python 版本强绑定。如使用 Python 3.11/3.12，需重新编译。

---

## 2. 构建

### 2.1 使用 build.bat（推荐）

```bash
cd MCP
build.bat
```

构建成功后输出：
- `MCP/build/exporter/Release/exporter.exe`
- `MCP/build/bindings/Release/TraceAnalyzer_py.cp3xx-win_amd64.pyd`

清理构建目录：

```bash
build.bat clean
```

### 2.2 手动构建

```bash
cd MCP

# 配置
cmake -S . -B build -A x64

# 编译
cmake --build build --config Release
```

### 2.3 验证构建产物

确认以下文件存在：

```
MCP/build/exporter/Release/exporter.exe
MCP/build/bindings/Release/TraceAnalyzer_py.cp3xx-win_amd64.pyd
```

其中 `cp3xx` 与编译时 Python 版本对应（如 `cp310` = Python 3.10）。

---

## 3. 安装 Python 依赖

```bash
pip install -r MCP/server/requirements.txt
```

当前依赖：`mcp>=1.0.0`

---

## 4. 环境变量（可选）

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `MEMCORE_BUILD_DIR` | `MCP/build/bindings/Release` | `TraceAnalyzer_py.pyd` 所在目录 |
| `MEMCORE_EXPORTER` | `MCP/build/exporter/Release/exporter.exe` | exporter 可执行文件路径 |
| `MEMCORE_CACHE_DIR` | `MCP/caches` | `.memcache` 缓存目录 |

> 默认值基于相对路径，前提是工作目录为项目根（`MemViewer/`）。若从其他目录启动，需设置这些环境变量为绝对路径。

---

## 5. 客户端配置

MCP 服务通过 **stdio** 传输，启动命令为：

```bash
python MCP/server/server.py
```

以下是各平台的配置方式。

### 5.1 Cursor

在项目根目录创建或编辑 `.cursor/mcp.json`：

```json
{
  "mcpServers": {
    "mem-analyzer": {
      "command": "python",
      "args": ["MCP/server/server.py"]
    }
  }
}
```

### 5.2 Claude Desktop

编辑 Claude Desktop 配置文件：

- **Windows**：`%APPDATA%\Claude\claude_desktop_config.json`
- **macOS**：`~/Library/Application Support/Claude/claude_desktop_config.json`

```json
{
  "mcpServers": {
    "mem-analyzer": {
      "command": "python",
      "args": ["<绝对路径>/MCP/server/server.py"],
      "env": {
        "MEMCORE_BUILD_DIR": "<绝对路径>/MCP/build/bindings/Release",
        "MEMCORE_EXPORTER": "<绝对路径>/MCP/build/exporter/Release/exporter.exe",
        "MEMCORE_CACHE_DIR": "<绝对路径>/MCP/caches"
      }
    }
  }
}
```

> Claude Desktop 不会自动继承工作目录，必须使用绝对路径。

### 5.3 VS Code (Copilot / 其他 MCP 扩展)

在 `.vscode/settings.json` 中添加（具体字段取决于扩展）：

```json
{
  "mcp.servers": {
    "mem-analyzer": {
      "command": "python",
      "args": ["MCP/server/server.py"]
    }
  }
}
```

### 5.4 通用 MCP 客户端

任何支持 stdio 传输的 MCP 客户端均可使用：

```json
{
  "command": "python",
  "args": ["<路径>/MCP/server/server.py"],
  "transport": "stdio"
}
```

如客户端不在项目根目录启动，需通过环境变量指定路径（见第 4 节）。

---

## 6. 验证部署

部署完成后，使用以下步骤验证：

1. **检查工具列表**：客户端应能发现 21 个工具（`load_trace`、`get_trace_info` 等）
2. **加载测试 trace**：调用 `load_trace`，传入 `.memtrace` 文件路径
3. **查询摘要**：调用 `get_memory_summary` 确认返回内存数据
4. **检查统计**：调用 `get_tool_stats` 查看调用是否被正确记录

---

## 7. 目录结构

```
MCP/
├── build.bat              # 构建脚本
├── CMakeLists.txt         # CMake 主配置
├── mem-analyzer.mdc       # AI Agent 使用指南（Cursor MDC 规则）
├── DEPLOY.md              # 本文档
├── configs/               # 分类/RHI/LLM 配置文件
│   ├── category_config.ini
│   ├── lua_category_config.ini
│   ├── rhi_config.ini
│   └── llm_config.ini
├── server/                # MCP Python 服务
│   ├── server.py          # 入口
│   ├── requirements.txt   # Python 依赖
│   └── tools/             # 工具实现模块
├── bindings/              # pybind11 Python 绑定
│   ├── CMakeLists.txt
│   └── bindings.cpp
├── exporter/              # C++ 命令行导出工具
│   ├── CMakeLists.txt
│   └── main.cpp
└── build/                 # 构建产物（gitignore）
    ├── bindings/Release/  → TraceAnalyzer_py.pyd
    └── exporter/Release/  → exporter.exe
```

---

## 8. 故障排查

### Python 版本不匹配

**症状**：`ImportError: DLL load failed` 或找不到 `TraceAnalyzer_py`

**解决**：确保运行时 Python 版本与编译时一致。检查 `.pyd` 文件名中的版本标识（如 `cp310` = 3.10）。

```bash
python --version
```

### exporter 找不到

**症状**：`load_trace` 返回 `export_failed`

**解决**：
1. 确认 `MCP/build/exporter/Release/exporter.exe` 存在
2. 或设置 `MEMCORE_EXPORTER` 环境变量指向正确路径

### 缓存目录权限

**症状**：`load_trace` 报写入错误

**解决**：确保 `MEMCORE_CACHE_DIR`（默认 `MCP/caches/`）目录可写。

### CMake 找不到 Python

**症状**：CMake 配置时报 `Could NOT find Python3`

**解决**：
1. 确认 Python 开发组件已安装（Windows 安装器中勾选 "Download debug binaries" 和 "pip"）
2. 确认 `python` 在 PATH 中
3. 或手动指定：`cmake -S . -B build -A x64 -DPython3_ROOT_DIR="C:/Python310"`
