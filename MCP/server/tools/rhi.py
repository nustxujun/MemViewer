import os
from mcp.types import Tool
from . import make_result


def get_tools():
    return [
        Tool(
            name="get_rhi_summary",
            description="RHI resource summary by type (Texture/Buffer counts and sizes)",
            inputSchema={
                "type": "object",
                "properties": {
                    "source": {
                        "type": "string",
                        "enum": ["current", "compare"],
                        "default": "current",
                    },
                },
            },
        ),
        Tool(
            name="get_rhi_resources",
            description="Get RHI resources with filtering",
            inputSchema={
                "type": "object",
                "properties": {
                    "type": {
                        "type": "integer",
                        "description": "RHI type index (0=Texture2D, 5=VertexBuffer, etc.)",
                    },
                    "name_filter": {"type": "string", "default": ""},
                    "limit": {"type": "integer", "default": 50},
                    "offset": {"type": "integer", "default": 0},
                    "source": {
                        "type": "string",
                        "enum": ["current", "compare"],
                        "default": "current",
                    },
                },
            },
        ),
    ]


def get_handlers(mgr, ta):
    def _get_analyzed(args):
        source = args.get("source", "current")
        session, serr = mgr.get_session(source)
        if serr:
            return None, serr

        cache_type = "compare" if source == "compare" else "range"
        cache_key = ("rhi_analyzed", source)

        def compute():
            config_path = os.path.join(mgr.config_dir, "rhi_config.ini")
            return ta.analyze_rhi(session, "native", config_path)

        return mgr.get_cached(cache_type, cache_key, compute), None

    async def get_rhi_summary(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)

        result, rerr = _get_analyzed(args)
        if rerr:
            return make_result(rerr)

        if isinstance(result, tuple):
            return make_result(result[0])
        return make_result(result)

    async def get_rhi_resources(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)

        result, rerr = _get_analyzed(args)
        if rerr:
            return make_result(rerr)

        type_idx = args.get("type", 0)
        name_filter = args.get("name_filter", "")
        limit = args.get("limit", 50)
        offset = args.get("offset", 0)

        if isinstance(result, tuple) and len(result) > 1:
            holder = result[1]
            resources = ta.get_rhi_resources(
                holder, type_idx, -1, name_filter, 0, 0, limit, offset
            )
            return make_result(resources)

        return make_result([])

    return {"get_rhi_summary": get_rhi_summary, "get_rhi_resources": get_rhi_resources}
