from mcp.types import Tool
from . import make_result, make_error


def get_tools():
    return [
        Tool(
            name="set_frame_range",
            description="Set analysis frame range. Recomputes range-dependent data.",
            inputSchema={
                "type": "object",
                "properties": {
                    "begin": {"type": "integer"},
                    "end": {"type": "integer"},
                },
                "required": ["begin", "end"],
            },
        ),
        Tool(
            name="compare_traces",
            description="Compare two traces and compute diff. Use source='compare' in other tools to analyze.",
            inputSchema={
                "type": "object",
                "properties": {
                    "path1": {
                        "type": "string",
                        "description": "Path to first .memtrace file",
                    },
                    "path2": {
                        "type": "string",
                        "description": "Path to second .memtrace file",
                    },
                },
                "required": ["path1", "path2"],
            },
        ),
    ]


def get_handlers(mgr, ta):
    async def set_frame_range(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)
        begin = args.get("begin", 0)
        end = args.get("end", 0)
        mgr.set_range(begin, end)
        return make_result({"status": "ok", "range": {"begin": begin, "end": end}})

    async def compare_traces(args):
        path1 = args.get("path1", "")
        path2 = args.get("path2", "")
        if not path1 or not path2:
            return make_error("invalid_param", "Both path1 and path2 are required")

        if mgr.current_path != path1:
            result1 = mgr.load(path1)
            if "error" in result1:
                return make_result(result1)

        data1 = mgr.current_session.get_full_data()

        try:
            session2 = ta.load_trace(path2)
            data2 = session2.get_full_data()
        except Exception as e:
            return make_error("load_failed", f"Failed to load path2: {e}")

        try:
            diff_data = ta.get_diff(data1, data2)
            mgr.compare_session = ta.create_session_from_data(diff_data)
            mgr._compare_cache.clear()
        except Exception as e:
            return make_error("compare_failed", str(e))

        return make_result(
            {
                "status": "ok",
                "message": "Diff computed. Use source='compare' in other tools to analyze differences.",
            }
        )

    return {"set_frame_range": set_frame_range, "compare_traces": compare_traces}
