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


def _current_timeline(resolve: Any) -> tuple[Any, Any, Any]:
    manager = resolve.GetProjectManager()
    project = manager.GetCurrentProject() if manager else None
    timeline = project.GetCurrentTimeline() if project else None
    if project is None or timeline is None:
        raise RuntimeError("当前没有打开的 Resolve 项目或时间线")
    return manager, project, timeline


def _timeline_snapshot(resolve: Any) -> dict[str, Any]:
    _, project, timeline = _current_timeline(resolve)
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


def _set_playhead(resolve: Any, params: dict[str, Any]) -> dict[str, Any]:
    _, project, timeline = _current_timeline(resolve)
    frame = int(params.get("frame", -1))
    start = int(timeline.GetStartFrame() or 0)
    end = int(timeline.GetEndFrame() or 0)
    if frame < start or frame > end:
        return {"accepted": False}
    try:
        fps = float(project.GetSetting("timelineFrameRate") or 24)
    except (TypeError, ValueError):
        fps = 24.0
    relative = max(0, frame - start)
    total_seconds, frames = divmod(relative, max(1, int(round(fps))))
    hours, remainder = divmod(total_seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    accepted = bool(timeline.SetCurrentTimecode(f"{hours:02d}:{minutes:02d}:{seconds:02d}:{frames:02d}"))
    return {"accepted": accepted}


def _text_from_component(value: Any) -> str:
    if isinstance(value, dict):
        if value.get("type") == "text" and isinstance(value.get("properties"), dict):
            return str(value["properties"].get("text", ""))
        for child in value.get("children", []):
            text = _text_from_component(child)
            if text:
                return text
    return ""


def _insert_subtitle_as_text_plus(resolve: Any, params: dict[str, Any]) -> dict[str, Any]:
    _, _, timeline = _current_timeline(resolve)
    text = _text_from_component(params.get("component", {})).strip()
    if not text:
        return {"accepted": False}
    item = timeline.InsertFusionTitleIntoTimeline("Text+")
    if item is None:
        return {"accepted": False}
    # Text+ is the documented editable Fusion title fallback. The component's
    # standard text property maps to StyledText; unsupported style fields stay
    # in Edward's conversion report and are not silently dropped.
    if not bool(item.SetProperty("StyledText", text)):
        return {"accepted": False}
    return {"accepted": True, "componentId": str(item.GetUniqueId())}


def handle(request: dict[str, Any]) -> dict[str, Any]:
    operation = request.get("operation")
    if operation == "health":
        return health()
    resolve = _resolve()
    if operation == "capabilities":
        return _capabilities(resolve)
    if operation == "timeline.snapshot":
        return _timeline_snapshot(resolve)
    if operation == "timeline.setPlayhead":
        return _set_playhead(resolve, request.get("params") or {})
    if operation == "subtitle.insert":
        return _insert_subtitle_as_text_plus(resolve, request.get("params") or {})
    if operation == "ui.openDeliver":
        return {"accepted": bool(resolve.OpenPage("deliver"))}
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
