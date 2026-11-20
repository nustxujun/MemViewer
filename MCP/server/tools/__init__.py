import json
import time
from mcp.types import TextContent, Tool


def make_error(error_code, message):
    return [
        TextContent(
            type="text",
            text=json.dumps(
                {"error": error_code, "message": message}, ensure_ascii=False
            ),
        )
    ]


def make_result(data):
    return [
        TextContent(
            type="text",
            text=json.dumps(data, ensure_ascii=False, default=str),
        )
    ]


class ToolStats:
    """Per-tool usage statistics collector."""

    def __init__(self):
        self._stats: dict[str, dict] = {}

    def record(self, name: str, elapsed: float, response_bytes: int, success: bool):
        s = self._stats.get(name)
        if s is None:
            s = {
                "call_count": 0,
                "success_count": 0,
                "error_count": 0,
                "total_time": 0.0,
                "min_time": float("inf"),
                "max_time": 0.0,
                "total_response_bytes": 0,
                "max_response_bytes": 0,
                "last_time": 0.0,
                "last_response_bytes": 0,
            }
            self._stats[name] = s

        s["call_count"] += 1
        if success:
            s["success_count"] += 1
        else:
            s["error_count"] += 1
        s["total_time"] += elapsed
        s["min_time"] = min(s["min_time"], elapsed)
        s["max_time"] = max(s["max_time"], elapsed)
        s["total_response_bytes"] += response_bytes
        s["max_response_bytes"] = max(s["max_response_bytes"], response_bytes)
        s["last_time"] = elapsed
        s["last_response_bytes"] = response_bytes

    def get_report(self, tool_name: str | None = None) -> dict:
        if tool_name:
            s = self._stats.get(tool_name)
            if s is None:
                return {"error": "no_data", "message": f"No stats for tool: {tool_name}"}
            return self._format_entry(tool_name, s)

        if not self._stats:
            return {"tools": [], "summary": "No tools have been called yet."}

        total_calls = sum(s["call_count"] for s in self._stats.values())
        total_time = sum(s["total_time"] for s in self._stats.values())
        total_bytes = sum(s["total_response_bytes"] for s in self._stats.values())

        tools = [
            self._format_entry(name, s)
            for name, s in sorted(self._stats.items(), key=lambda x: x[1]["total_time"], reverse=True)
        ]
        return {
            "total_calls": total_calls,
            "total_time_s": round(total_time, 3),
            "total_response_kb": round(total_bytes / 1024, 2),
            "tools": tools,
        }

    def reset(self):
        self._stats.clear()

    @staticmethod
    def _format_entry(name: str, s: dict) -> dict:
        count = s["call_count"]
        return {
            "tool": name,
            "call_count": count,
            "success_count": s["success_count"],
            "error_count": s["error_count"],
            "avg_time_s": round(s["total_time"] / count, 3) if count else 0,
            "min_time_s": round(s["min_time"], 3) if s["min_time"] != float("inf") else 0,
            "max_time_s": round(s["max_time"], 3),
            "last_time_s": round(s["last_time"], 3),
            "total_response_kb": round(s["total_response_bytes"] / 1024, 2),
            "max_response_kb": round(s["max_response_bytes"] / 1024, 2),
            "last_response_kb": round(s["last_response_bytes"] / 1024, 2),
        }


tool_stats = ToolStats()


def register_all_tools(app, session_mgr, ta):
    from .trace import get_tools as trace_tools, get_handlers as trace_handlers
    from .summary import get_tools as summary_tools, get_handlers as summary_handlers
    from .categories import get_tools as cat_tools, get_handlers as cat_handlers
    from .timeline import get_tools as tl_tools, get_handlers as tl_handlers
    from .callstack import get_tools as cs_tools, get_handlers as cs_handlers
    from .objects import get_tools as obj_tools, get_handlers as obj_handlers
    from .rhi import get_tools as rhi_tools, get_handlers as rhi_handlers
    from .compare import get_tools as cmp_tools, get_handlers as cmp_handlers

    all_tools = [
        Tool(
            name="get_tool_stats",
            description="Query tool usage statistics: call count, response time (avg/min/max), response size, etc. "
                        "Can query a specific tool or all tools. Use action='reset' to clear stats.",
            inputSchema={
                "type": "object",
                "properties": {
                    "tool_name": {
                        "type": "string",
                        "description": "Optional. Query stats for a specific tool. Omit to get all tools.",
                    },
                    "action": {
                        "type": "string",
                        "enum": ["query", "reset"],
                        "description": "Optional. 'query' (default) returns stats, 'reset' clears all stats.",
                    },
                },
            },
        ),
    ]
    all_handlers = {}

    for get_t, get_h in [
        (trace_tools, trace_handlers),
        (summary_tools, summary_handlers),
        (cat_tools, cat_handlers),
        (tl_tools, tl_handlers),
        (cs_tools, cs_handlers),
        (obj_tools, obj_handlers),
        (rhi_tools, rhi_handlers),
        (cmp_tools, cmp_handlers),
    ]:
        all_tools.extend(get_t())
        all_handlers.update(get_h(session_mgr, ta))

    @app.list_tools()
    async def list_tools():
        return all_tools

    @app.call_tool()
    async def call_tool(name: str, arguments: dict):
        if name == "get_tool_stats":
            action = arguments.get("action", "query")
            if action == "reset":
                tool_stats.reset()
                return make_result({"message": "Stats cleared."})
            return make_result(tool_stats.get_report(arguments.get("tool_name")))

        handler = all_handlers.get(name)
        if handler is None:
            return make_error("unknown_tool", f"Unknown tool: {name}")

        t0 = time.perf_counter()
        try:
            result = await handler(arguments)
            elapsed = time.perf_counter() - t0
            resp_bytes = sum(len(c.text.encode("utf-8")) for c in result if hasattr(c, "text"))
            tool_stats.record(name, elapsed, resp_bytes, success=True)
            return result
        except Exception as e:
            elapsed = time.perf_counter() - t0
            tool_stats.record(name, elapsed, 0, success=False)
            return make_error("internal_error", str(e))
