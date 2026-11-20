import sys
import os
import subprocess
import json
from pathlib import Path

BUILD_DIR = os.environ.get(
    "MEMCORE_BUILD_DIR",
    str(Path(__file__).parent.parent / "build" / "bindings" / "Release"),
)
sys.path.insert(0, BUILD_DIR)

import TraceAnalyzer_py as ta
from mcp.server import Server
from mcp.server.stdio import stdio_server

app = Server("mem-analyzer")


class SessionManager:
    """Manages loaded trace sessions with three-tier caching."""

    def __init__(self):
        self.current_session = None
        self.precomputed = None
        self.compare_session = None
        self.current_path = None

        self._range_cache = {}
        self._static_cache = {}
        self._compare_cache = {}
        self._current_range = None

        self.exporter_path = os.environ.get(
            "MEMCORE_EXPORTER",
            str(
                Path(__file__).parent.parent
                / "build"
                / "exporter"
                / "Release"
                / "exporter.exe"
            ),
        )
        self.cache_dir = os.environ.get(
            "MEMCORE_CACHE_DIR", str(Path(__file__).parent.parent / "caches")
        )
        self.config_dir = str(Path(__file__).parent.parent.parent / "TraceAnalyzer" / "configs")

    def load(self, path: str) -> dict:
        """Load a trace file, return summary dict."""
        os.makedirs(self.cache_dir, exist_ok=True)

        trace_path = Path(path)
        cache_name = f"{trace_path.stem}.memcache"
        cache_path = str(Path(self.cache_dir) / cache_name)

        progress_lines = []

        if not os.path.exists(cache_path):
            try:
                proc = subprocess.run(
                    [
                        self.exporter_path,
                        str(path),
                        "--output",
                        cache_path,
                        "--config-dir",
                        self.config_dir,
                    ],
                    capture_output=True,
                    text=True,
                    timeout=300,
                )
                progress_lines = [l for l in proc.stderr.split("\n") if l.strip()]
                if proc.returncode != 0:
                    return {"error": "export_failed", "stderr": proc.stderr}
            except Exception as e:
                return {"error": "export_failed", "message": str(e)}

        try:
            result = ta.load_cache(cache_path)
            self.current_session = result[0]
            self.precomputed = result[1]
            self.current_path = str(path)
            self._range_cache.clear()
            self._static_cache.clear()
            self._compare_cache.clear()
            self._current_range = None
            self.compare_session = None
        except Exception as e:
            return {"error": "load_failed", "message": str(e)}

        summary = self.current_session.get_summary()
        summary["path"] = str(path)
        if progress_lines:
            summary["progress"] = progress_lines[-1]
        return summary

    def set_range(self, begin: int, end: int):
        self.current_session.set_range(begin, end)
        self._range_cache.clear()
        self._current_range = (begin, end)

    def get_cached(self, cache_type: str, key, compute_fn):
        if cache_type == "range":
            cache = self._range_cache
        elif cache_type == "compare":
            cache = self._compare_cache
        else:
            cache = self._static_cache
        if key not in cache:
            cache[key] = compute_fn()
        return cache[key]

    def ensure_loaded(self):
        if self.current_session is None:
            return {"error": "no_trace_loaded", "message": "请先调用 load_trace"}
        return None

    def get_session(self, source: str = "current"):
        if source == "compare":
            if self.compare_session is None:
                return None, {"error": "no_compare_data", "message": "请先调用 compare_traces"}
            return self.compare_session, None
        return self.current_session, None


session_mgr = SessionManager()

from tools import register_all_tools

register_all_tools(app, session_mgr, ta)


async def main():
    async with stdio_server() as (read_stream, write_stream):
        await app.run(read_stream, write_stream, app.create_initialization_options())


if __name__ == "__main__":
    import asyncio

    asyncio.run(main())
