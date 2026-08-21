#!/usr/bin/env python3
"""Resolve Studio 官方外部脚本直连健康检查。

仅执行 `scriptapp("Resolve")` 和版本读取，不修改项目、时间线或渲染队列。
"""

from __future__ import annotations

import argparse
import importlib
import json
import os
import sys
from pathlib import Path

LAST_RESOLVE = None


def _paths() -> tuple[Path, Path]:
    if sys.platform == "darwin":
        api = Path("/Library/Application Support/Blackmagic Design/DaVinci Resolve/Developer/Scripting")
        lib = Path("/Applications/DaVinci Resolve/DaVinci Resolve.app/Contents/Libraries/Fusion/fusionscript.so")
    elif sys.platform == "win32":
        api = Path(os.environ.get("PROGRAMDATA", "C:/ProgramData")) / "Blackmagic Design/DaVinci Resolve/Support/Developer/Scripting"
        lib = Path(os.environ.get("PROGRAMFILES", "C:/Program Files")) / "Blackmagic Design/DaVinci Resolve/fusionscript.dll"
    else:
        api = Path("/opt/resolve/Developer/Scripting")
        lib = Path("/opt/resolve/libs/Fusion/fusionscript.so")
    return (
        Path(os.environ.get("RESOLVE_SCRIPT_API", api)).expanduser(),
        Path(os.environ.get("RESOLVE_SCRIPT_LIB", lib)).expanduser(),
    )


def health() -> dict[str, object]:
    global LAST_RESOLVE
    api, library = _paths()
    modules = Path(os.environ.get("RESOLVE_SCRIPT_MODULES", api / "Modules")).expanduser()
    if str(modules) not in sys.path:
        sys.path.insert(0, str(modules))
    os.environ.setdefault("RESOLVE_SCRIPT_API", str(api))
    os.environ.setdefault("RESOLVE_SCRIPT_LIB", str(library))
    result: dict[str, object] = {
        "mode": "direct",
        "api_path": str(api),
        "library_path": str(library),
        "modules_path": str(modules),
        "api_found": api.is_dir(),
        "library_found": library.is_file(),
        "connected": False,
    }
    try:
        dvr = importlib.import_module("DaVinciResolveScript")
        resolve = dvr.scriptapp("Resolve")
        if resolve is None:
            result["error"] = "scriptapp(Resolve) 返回空值；请确认 Resolve Studio 正在运行并允许 External scripting。"
            return result
        result["connected"] = True
        LAST_RESOLVE = resolve
        result["product"] = str(resolve.GetProductName())
        result["version"] = str(resolve.GetVersionString())
    except ModuleNotFoundError:
        result["error"] = "找不到 DaVinciResolveScript；请检查 Resolve Studio 的 Scripting API 路径。"
    except Exception as exc:  # Resolve API 的底层异常类型由安装版本决定
        result["error"] = f"官方脚本直连失败: {exc}"
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description="Edward Resolve Studio 官方直连健康检查")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    result = health()
    if args.json:
        print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    else:
        print("已连接" if result["connected"] else "未连接")
        if result.get("error"):
            print(result["error"])
    return 0 if result["connected"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
