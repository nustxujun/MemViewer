from mcp.types import Tool
from . import make_result, make_error


def get_tools():
    return [
        Tool(
            name="get_timeline",
            description="Get timeline data (pre-computed, <10ms). Series: trk/phy/lua/custom",
            inputSchema={
                "type": "object",
                "properties": {
                    "series": {
                        "type": "string",
                        "description": "Series name (trk/phy/lua or custom name). If empty, lists available series.",
                    },
                    "max_points": {
                        "type": "integer",
                        "default": 500,
                        "description": "Max data points (auto-downsampled)",
                    },
                },
            },
        ),
        Tool(
            name="get_timeline_categories",
            description="Get category timeline (one line per category)",
            inputSchema={
                "type": "object",
                "properties": {
                    "config_file": {"type": "string"},
                    "max_points": {"type": "integer", "default": 500},
                },
            },
        ),
    ]


def get_handlers(mgr, ta):
    async def get_timeline(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)
        if mgr.precomputed is None:
            return make_error("no_precomputed", "No precomputed data available")

        series_name = args.get("series", "")
        max_points = args.get("max_points", 500)

        if not series_name:
            return make_result(ta.list_timeline_series(mgr.precomputed))

        try:
            result = ta.get_precomputed_timeline(mgr.precomputed, series_name, max_points)
            return make_result(result)
        except Exception as e:
            return make_error("not_found", str(e))

    async def get_timeline_categories(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)
        if mgr.precomputed is None:
            return make_error("no_precomputed", "No precomputed data available")

        max_points = args.get("max_points", 500)

        all_series = ta.list_timeline_series(mgr.precomputed)
        cat_series = [s for s in all_series if s["name"].endswith("_category")]

        if not cat_series:
            return make_result({"message": "No category timelines available"})

        results = []
        for s in cat_series:
            try:
                data = ta.get_precomputed_timeline(
                    mgr.precomputed, s["name"], max_points
                )
                results.append(data)
            except Exception:
                pass

        return make_result(results)

    return {"get_timeline": get_timeline, "get_timeline_categories": get_timeline_categories}
