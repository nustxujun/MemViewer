from mcp.types import Tool
from . import make_result


def get_tools():
    return [
        Tool(
            name="get_memory_summary",
            description="Memory usage summary (used/available/overhead min/max/avg in MB)",
            inputSchema={"type": "object", "properties": {}},
        ),
        Tool(
            name="get_checkpoints",
            description="List all checkpoints in the trace",
            inputSchema={"type": "object", "properties": {}},
        ),
    ]


def get_handlers(mgr, ta):
    async def get_memory_summary(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)
        return make_result(mgr.current_session.get_memory_summary())

    async def get_checkpoints(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)
        return make_result(mgr.current_session.get_checkpoints())

    return {"get_memory_summary": get_memory_summary, "get_checkpoints": get_checkpoints}
