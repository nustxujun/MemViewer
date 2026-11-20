from mcp.types import Tool
from . import make_result


def get_tools():
    return [
        Tool(
            name="get_callstack_top",
            description="Get top callstack nodes by memory size",
            inputSchema={
                "type": "object",
                "properties": {
                    "depth": {"type": "integer", "default": 3},
                    "min_size": {"type": "integer", "default": 0},
                    "type": {
                        "type": "string",
                        "enum": ["native", "lua"],
                        "default": "native",
                    },
                    "filter": {
                        "type": "string",
                        "description": "Filter keywords separated by |",
                    },
                    "source": {
                        "type": "string",
                        "enum": ["current", "compare"],
                        "default": "current",
                    },
                    "limit": {"type": "integer", "default": 50},
                },
            },
        ),
        Tool(
            name="get_callstack_children",
            description="Get children of a callstack node",
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
        Tool(
            name="get_callstack_path",
            description="Get path from node to root",
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
        Tool(
            name="search_callstack",
            description="Search callstack nodes by keyword",
            inputSchema={
                "type": "object",
                "properties": {
                    "keyword": {"type": "string"},
                    "type": {
                        "type": "string",
                        "enum": ["native", "lua"],
                        "default": "native",
                    },
                    "case_sensitive": {"type": "boolean", "default": False},
                    "source": {
                        "type": "string",
                        "enum": ["current", "compare"],
                        "default": "current",
                    },
                    "limit": {"type": "integer", "default": 50},
                },
                "required": ["keyword"],
            },
        ),
        Tool(
            name="get_node_info",
            description="Get detailed info for a single callstack node",
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
    def _get_tree(args):
        data_type = args.get("type", "native")
        source = args.get("source", "current")
        filter_str = args.get("filter", "")
        filters = (
            [f.strip() for f in filter_str.split("|") if f.strip()]
            if filter_str
            else []
        )

        session, err = mgr.get_session(source)
        if err:
            return None, err

        cache_type = "compare" if source == "compare" else "range"
        cache_key = ("filtered_tree", data_type, tuple(filters))

        def compute():
            return ta.build_filtered_tree(session, data_type, filters, False)

        return mgr.get_cached(cache_type, cache_key, compute), None

    async def get_callstack_top(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)
        tree, terr = _get_tree(args)
        if terr:
            return make_result(terr)
        depth = args.get("depth", 3)
        min_size = args.get("min_size", 0)
        limit = args.get("limit", 50)
        nodes = ta.get_callstack_top(tree, depth, min_size, limit)
        return make_result(nodes)

    async def get_callstack_children(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)
        tree, terr = _get_tree(args)
        if terr:
            return make_result(terr)
        node_index = args.get("node_index", 0)
        return make_result(ta.get_callstack_children(tree, node_index))

    async def get_callstack_path(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)
        tree, terr = _get_tree(args)
        if terr:
            return make_result(terr)
        node_index = args.get("node_index", 0)
        return make_result(ta.get_callstack_path(tree, node_index))

    async def search_callstack(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)
        tree, terr = _get_tree(args)
        if terr:
            return make_result(terr)
        keyword = args.get("keyword", "")
        keywords = [k.strip() for k in keyword.split("|") if k.strip()]
        case_sensitive = args.get("case_sensitive", False)
        limit = args.get("limit", 50)
        return make_result(ta.search_callstack(tree, keywords, case_sensitive, limit))

    async def get_node_info(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)
        tree, terr = _get_tree(args)
        if terr:
            return make_result(terr)
        node_index = args.get("node_index", 0)
        return make_result(ta.get_node_info(tree, node_index))

    return {
        "get_callstack_top": get_callstack_top,
        "get_callstack_children": get_callstack_children,
        "get_callstack_path": get_callstack_path,
        "search_callstack": search_callstack,
        "get_node_info": get_node_info,
    }
