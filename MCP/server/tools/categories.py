import os
from mcp.types import Tool
from . import make_result, make_error


def get_tools():
    return [
        Tool(
            name="get_categories",
            description="Get category breakdown of memory allocations",
            inputSchema={
                "type": "object",
                "properties": {
                    "type": {
                        "type": "string",
                        "enum": ["native", "lua"],
                        "default": "native",
                    },
                    "config_file": {
                        "type": "string",
                        "description": "Custom config file path (optional)",
                    },
                    "source": {
                        "type": "string",
                        "enum": ["current", "compare"],
                        "default": "current",
                    },
                },
            },
        ),
        Tool(
            name="get_subcategories",
            description="Get subcategories for a specific category",
            inputSchema={
                "type": "object",
                "properties": {
                    "category_name": {"type": "string"},
                    "type": {
                        "type": "string",
                        "enum": ["native", "lua"],
                        "default": "native",
                    },
                },
                "required": ["category_name"],
            },
        ),
        Tool(
            name="get_allocation_callstack",
            description="Get callstack for a specific node",
            inputSchema={
                "type": "object",
                "properties": {
                    "node_index": {"type": "integer"},
                    "type": {
                        "type": "string",
                        "enum": ["native", "lua"],
                        "default": "native",
                    },
                    "source": {
                        "type": "string",
                        "enum": ["current", "compare"],
                        "default": "current",
                    },
                },
                "required": ["node_index"],
            },
        ),
    ]


def get_handlers(mgr, ta):
    def _get_config(args):
        config_file = args.get("config_file", "")
        data_type = args.get("type", "native")
        if config_file:
            return config_file
        if data_type == "lua":
            return os.path.join(mgr.config_dir, "lua_category_config.ini")
        return os.path.join(mgr.config_dir, "category_config.ini")

    async def get_categories(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)

        data_type = args.get("type", "native")
        config_path = _get_config(args)
        source = args.get("source", "current")

        session, serr = mgr.get_session(source)
        if serr:
            return make_result(serr)

        cache_type = "compare" if source == "compare" else "range"
        cache_key = ("categories", data_type, config_path)

        def compute():
            return ta.analyze_categories(session, config_path, data_type)

        result = mgr.get_cached(cache_type, cache_key, compute)
        return make_result(result)

    async def get_subcategories(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)
        config_path = _get_config(args)
        category_name = args.get("category_name", "")
        cats = ta.parse_category_config(config_path)
        for cat in cats:
            if cat["name"] == category_name:
                return make_result(cat.get("subs", []))
        return make_error("not_found", f"Category '{category_name}' not found")

    async def get_allocation_callstack(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)

        source = args.get("source", "current")
        session, serr = mgr.get_session(source)
        if serr:
            return make_result(serr)

        node_index = args.get("node_index", 0)
        data_type = args.get("type", "native")
        try:
            tree = ta.build_filtered_tree(session, data_type)
            path = ta.get_callstack_path(tree, node_index)
            return make_result(path)
        except Exception as e:
            return make_error("invalid_param", str(e))

    return {
        "get_categories": get_categories,
        "get_subcategories": get_subcategories,
        "get_allocation_callstack": get_allocation_callstack,
    }
