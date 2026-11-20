from mcp.types import Tool
from . import make_result, make_error


def get_tools():
    return [
        Tool(
            name="load_trace",
            description="Load a .memtrace file for analysis. Automatically preprocesses and caches.",
            inputSchema={
                "type": "object",
                "properties": {
                    "path": {"type": "string", "description": "Path to .memtrace file"}
                },
                "required": ["path"],
            },
        ),
        Tool(
            name="get_trace_info",
            description="Get current trace basic info (frames, counts)",
            inputSchema={"type": "object", "properties": {}},
        ),
    ]


def get_handlers(mgr, ta):
    async def load_trace(args):
        path = args.get("path", "")
        if not path:
            return make_error("invalid_param", "path is required")
        result = mgr.load(path)
        return make_result(result)

    async def get_trace_info(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)
        summary = mgr.current_session.get_summary()
        summary["path"] = mgr.current_path
        return make_result(summary)

    return {"load_trace": load_trace, "get_trace_info": get_trace_info}
