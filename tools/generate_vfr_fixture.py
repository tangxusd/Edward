#!/usr/bin/env python3
"""从普通视频生成可验证的 H.264 VFR 性能夹具。"""

import argparse
import json
import os
import pathlib
import shutil
import subprocess
import tempfile


def run(command: list[str]) -> str:
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "外部媒体命令失败")
    return result.stdout


def parse_durations(value: str) -> list[int]:
    try:
        durations = [int(item.strip()) for item in value.split(",")]
    except ValueError as error:
        raise ValueError("--durations-ms 必须是逗号分隔的正整数") from error
    if not durations or any(item <= 0 for item in durations):
        raise ValueError("--durations-ms 必须至少包含一个正整数")
    return durations


def concat_escape(path: pathlib.Path) -> str:
    return str(path).replace("'", "'\\''")


def video_timestamps(path: pathlib.Path) -> list[float]:
    document = json.loads(run([
        "ffprobe", "-v", "error", "-select_streams", "v:0",
        "-show_entries", "frame=best_effort_timestamp_time", "-of", "json", str(path),
    ]))
    return [float(frame["best_effort_timestamp_time"]) for frame in document.get("frames", [])]


def main() -> int:
    parser = argparse.ArgumentParser(description="生成并验证 H.264 VFR MP4 夹具")
    parser.add_argument("--input", required=True, help="输入视频路径")
    parser.add_argument("--output", required=True, help="输出 MP4 路径")
    parser.add_argument("--durations-ms", default="40,90,60", help="逐帧持续时间循环，单位毫秒")
    args = parser.parse_args()

    source = pathlib.Path(args.input).resolve()
    output = pathlib.Path(args.output).resolve()
    durations = parse_durations(args.durations_ms)
    if not source.is_file():
        raise SystemExit("输入视频不存在或不是普通文件")
    if source == output:
        raise SystemExit("输出文件不能覆盖输入视频")
    output.parent.mkdir(parents=True, exist_ok=True)

    workspace = pathlib.Path(tempfile.mkdtemp(prefix=".edward-vfr-", dir=output.parent))
    temporary_output = workspace / "output.mp4"
    try:
        pattern = workspace / "frame-%08d.png"
        run([
            "ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(source),
            "-map", "0:v:0", "-vsync", "0", "-start_number", "0", "-y", str(pattern),
        ])
        frames = sorted(workspace.glob("frame-*.png"))
        if len(frames) < 2:
            raise RuntimeError("输入视频不足两帧，无法生成可验证 VFR 夹具")
        concat = workspace / "frames.ffconcat"
        with concat.open("w", encoding="utf-8", newline="\n") as file:
            file.write("ffconcat version 1.0\n")
            for index, frame in enumerate(frames):
                file.write(f"file '{concat_escape(frame)}'\n")
                file.write(f"duration {durations[index % len(durations)] / 1000.0:.6f}\n")
            file.write(f"file '{concat_escape(frames[-1])}'\n")
        run([
            "ffmpeg", "-hide_banner", "-loglevel", "error", "-f", "concat", "-safe", "0",
            "-i", str(concat), "-fps_mode", "vfr", "-an", "-c:v", "libx264",
            "-pix_fmt", "yuv420p", "-video_track_timescale", "1000", "-movflags", "+faststart",
            "-y", str(temporary_output),
        ])
        points = video_timestamps(temporary_output)
        deltas = {round(points[index + 1] - points[index], 6) for index in range(len(points) - 1)}
        if len(points) < 3 or len(deltas) < 2:
            raise RuntimeError("输出文件未包含可变帧时间戳")
        os.replace(temporary_output, output)
    finally:
        shutil.rmtree(workspace, ignore_errors=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
