#!/usr/bin/env python3
"""只读探针：检查 Edward 可使用的 Resolve Studio 连接模式。"""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
import sys
from pathlib import Path


def _default_module_dir() -> Path:
    if sys.platform == "darwin":
        return Path("/Library/Application Support/Blackmagic Design/DaVinci Resolve/Developer/Scripting/Modules")
    if sys.platform == "win32":
        return Path(os.environ.get("PROGRAMDATA", "C:/ProgramData")) / "Blackmagic Design/DaVinci Resolve/Support/Developer/Scripting/Modules"
    return Path("/opt/resolve/Developer/Scripting/Modules")


def _bridge_config() -> Path:
    override = os.environ.get("DAVINCI_RESOLVE_BRIDGE_CONFIG")
    return Path(override).expanduser() if override else Path.home() / ".config/davinci-resolve-mcp/bridge.json"


def probe() -> dict[str, object]:
    module_dir = Path(os.environ.get("RESOLVE_SCRIPT_MODULES", _default_module_dir())).expanduser()
    module_available = importlib.util.find_spec("DaVinciResolveScript") is not None
    if not module_available and module_dir.is_dir():
        module_available = importlib.util.find_spec("DaVinciResolveScript", [str(module_dir)]) is not None

    config_path = _bridge_config()
    bridge_configured = config_path.is_file()
    bridge_valid = False
    bridge_error = ""
    if bridge_configured:
        try:
            payload = json.loads(config_path.read_text(encoding="utf-8"))
            bridge_valid = (
                payload.get("host") in {"127.0.0.1", "localhost", "::1"}
                and isinstance(payload.get("port"), int)
                and 1 <= payload["port"] <= 65535
                and isinstance(payload.get("token"), str)
                and len(payload["token"]) >= 16
            )
            if not bridge_valid:
                bridge_error = "host/port/token 配置不符合本地认证 Bridge 要求"
        except (OSError, ValueError) as exc:
            bridge_error = f"bridge.json 无法读取: {exc}"

    if module_available:
        mode = "direct"
        next_step = "可使用 Resolve Studio 官方外部脚本直连；启动适配器后再执行真实集成测试。"
    elif bridge_valid:
        mode = "bridge_configured"
        next_step = "已发现认证 Bridge 配置；需在 Resolve Workspace > Scripts 中启动 resolve_bridge。"
    else:
        mode = "unavailable"
        next_step = "在 Resolve Preferences > General 将 External scripting using 设为 Local，或安装并启动上游认证 Bridge。"

    result: dict[str, object] = {
        "mode": mode,
        "resolve_app_running_assumed": True,
        "direct": {"module_available": module_available, "modules_path": str(module_dir)},
        "bridge": {"config_path": str(config_path), "configured": bridge_configured, "valid": bridge_valid},
        "next_step": next_step,
    }
    if bridge_error:
        result["bridge_error"] = bridge_error
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description="Edward Resolve Studio 只读连接探针")
    parser.add_argument("--json", action="store_true", help="输出 JSON")
    args = parser.parse_args()
    result = probe()
    if args.json:
        print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    else:
        print(f"mode: {result['mode']}")
        print(f"direct module: {result['direct']['module_available']}")
        print(f"bridge config: {result['bridge']['configured']} (valid={result['bridge']['valid']})")
        print(result["next_step"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
