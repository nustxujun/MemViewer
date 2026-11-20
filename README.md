# MemViewer

MemViewer 是一个面向 Unreal Engine 游戏的实验性内存分析项目，涵盖运行时采集、离线符号化、Windows 桌面可视化和 MCP 查询。

## 模块

| 模块 | 说明 |
| --- | --- |
| [MemProfiler](Plugins/MemProfiler/) | Unreal Engine 运行时插件，采集内存分配与释放、调用栈、UObject、RHI 和 Lua 数据。Windows 和 iOS 提供采集启动入口，Android 保留平台实现与打包配置，启动初始化目前处于停用状态。 |
| [MemoryTraceParser](MemoryTraceParser/) | 命令行工具，读取原始 Trace 和符号文件，生成供分析使用的 `.memtrace`。 |
| [TraceAnalyzer](TraceAnalyzer/) | MemViewer 和 MCP 共用的 C++20 静态库，负责解析 Trace、按帧筛选数据、聚合统计和处理缓存。 |
| [MemViewer](MemViewer/) | 基于 DirectX 12 和 Dear ImGui 的 Windows 桌面应用，可查看内存时间线、调用栈、内存分类、UObject 和 RHI 资源。 |
| [MemAnalyzer MCP](MCP/) | 通过 MCP 客户端加载 Trace，查询内存摘要、调用栈、分类、对象和资源，支持按帧范围查询和对比两份 Trace。 |
| [Tools](Tools/) | 构建脚本、Trace 导出和符号生成工具。 |
