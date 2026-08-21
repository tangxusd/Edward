#!/usr/bin/env python3
import argparse
import json
import pathlib
import shutil
import subprocess
import sys


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=True, text=True, capture_output=True)


def timestamps(path: pathlib.Path) -> list[float]:
    result = run([
        "ffprobe", "-v", "error", "-select_streams", "v:0",
        "-show_entries", "frame=best_effort_timestamp_time",
        "-of", "json", str(path),
    ])
    frames = json.loads(result.stdout)["frames"]
    return [float(frame["best_effort_timestamp_time"]) for frame in frames]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool", required=True)
    parser.add_argument("--workdir", required=True)
    args = parser.parse_args()
    workdir = pathlib.Path(args.workdir).resolve()
    if workdir.exists():
        shutil.rmtree(workdir)
    workdir.mkdir(parents=True)
    source = workdir / "source.mp4"
    output = workdir / "fixture-vfr.mp4"
    run([
        "ffmpeg", "-hide_banner", "-loglevel", "error", "-f", "lavfi",
        "-i", "testsrc2=size=160x90:rate=10:duration=1", "-c:v", "libx264",
        "-pix_fmt", "yuv420p", "-y", str(source),
    ])
    run([
        sys.executable, args.tool, "--input", str(source), "--output", str(output),
        "--durations-ms", "40,90,60",
    ])
    assert output.is_file()
    points = timestamps(output)
    deltas = [round(points[index + 1] - points[index], 6) for index in range(len(points) - 1)]
    assert len(points) >= 3
    assert len(set(deltas)) > 1, deltas
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
