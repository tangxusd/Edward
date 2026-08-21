#!/usr/bin/env python3
"""Edward 的 Resolve Studio 官方直连语义侧车。

协议：每行一个 JSON 请求，每行一个 JSON 响应。侧车不创建项目、不改变时间线，
只提供读取能力；写操作会在后续加入显式确认门。
"""

from __future__ import annotations

import json
import sys
from typing import Any

from resolve_direct_health import health


def _resolve():
    status = health()
    if not status.get("connected"):
        raise RuntimeError(str(status.get("error", "Resolve Studio 未连接")))
    import DaVinciResolveScript  # type: ignore

    value = DaVinciResolveScript.scriptapp("Resolve")
    if value is None:
        raise RuntimeError("scriptapp(Resolve) 返回空值")
    return value


def _capabilities(resolve: Any) -> dict[str, Any]:
    return {
        "studioVersion": str(resolve.GetVersionString()),
        "timeline": resolve.GetProjectManager() is not None,
        "fusion": True,
        "render": True,
    }


def _timeline_snapshot(resolve: Any) -> dict[str, Any]:
    manager = resolve.GetProjectManager()
    project = manager.GetCurrentProject() if manager else None
    timeline = project.GetCurrentTimeline() if project else None
    if project is None or timeline is None:
        raise RuntimeError("当前没有打开的 Resolve 项目或时间线")
    tracks: list[dict[str, Any]] = []
    for kind, is_video in (("video", True), ("audio", False)):
        count = int(timeline.GetTrackCount(kind) or 0)
        for index in range(1, count + 1):
            tracks.append({
                "id": f"{kind}{index}",
                "name": str(timeline.GetTrackName(kind, index) or f"{kind}{index}"),
                "video": is_video,
                "audio": not is_video,
            })
    start_frame = int(timeline.GetStartFrame() or 0)
    fps_text = str(project.GetSetting("timelineFrameRate") or "24")
    try:
        fps = float(fps_text)
    except ValueError:
        fps = 24.0
    timecode = str(timeline.GetCurrentTimecode() or "")
    parts = timecode.split(":")
    playhead_frame = start_frame
    if len(parts) == 4 and all(part.isdigit() for part in parts):
        hours, minutes, seconds, frames = (int(part) for part in parts)
        playhead_frame = int(round((hours * 3600 + minutes * 60 + seconds) * fps + frames))
    return {
        "projectName": str(project.GetName()),
        "timelineName": str(timeline.GetName()),
        "playheadTimecode": timecode,
        "playheadFrame": playhead_frame,
        "timelineStartFrame": start_frame,
        "timelineEndFrame": int(timeline.GetEndFrame() or 0),
        "fpsNumerator": int(round(fps)),
        "fpsDenominator": 1,
        "tracks": tracks,
    }


def handle(request: dict[str, Any]) -> dict[str, Any]:
    operation = request.get("operation")
    if operation == "health":
        return health()
    resolve = _resolve()
    if operation == "capabilities":
        return _capabilities(resolve)
    if operation == "timeline.snapshot":
        return _timeline_snapshot(resolve)
    raise ValueError(f"不支持的只读操作: {operation}")


def main() -> int:
    for line in sys.stdin:
        try:
            request = json.loads(line)
            result = handle(request)
            response = {"ok": True, "result": result}
        except Exception as exc:
            response = {"ok": False, "error": {"code": "resolve_sidecar_error", "message": str(exc)}}
        print(json.dumps(response, ensure_ascii=False, separators=(",", ":")), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
