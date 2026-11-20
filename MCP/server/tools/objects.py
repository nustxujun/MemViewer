from mcp.types import Tool
from . import make_result


def get_tools():
    return [
        Tool(
            name="get_object_classes",
            description="Get UObject class summary (add/delete counts)",
            inputSchema={
                "type": "object",
                "properties": {
                    "limit": {"type": "integer", "default": 50},
                    "source": {
                        "type": "string",
                        "enum": ["current", "compare"],
                        "default": "current",
                    },
                },
            },
        ),
        Tool(
            name="get_objects",
            description="Get objects filtered by class",
            inputSchema={
                "type": "object",
                "properties": {
                    "class_filter": {"type": "string", "default": ""},
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
    async def get_object_classes(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)
        source = args.get("source", "current")
        session, serr = mgr.get_session(source)
        if serr:
            return make_result(serr)
        limit = args.get("limit", 50)
        return make_result(ta.get_object_classes(session, "native", limit, 0))

    async def get_objects(args):
        err = mgr.ensure_loaded()
        if err:
            return make_result(err)
        source = args.get("source", "current")
        session, serr = mgr.get_session(source)
        if serr:
            return make_result(serr)
        class_filter = args.get("class_filter", "")
        limit = args.get("limit", 50)
        offset = args.get("offset", 0)
        return make_result(
            ta.get_objects(session, "native", class_filter, limit, offset)
        )

    return {"get_object_classes": get_object_classes, "get_objects": get_objects}
