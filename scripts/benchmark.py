"""Measure an actual built BitFly process; requires psutil. No public torrents."""
import argparse
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import tempfile
import time

import psutil

parser = argparse.ArgumentParser()
parser.add_argument("executable", type=Path)
parser.add_argument("--runs", type=int, default=3)
parser.add_argument("--output", type=Path, default=Path("build/benchmark.json"))
args = parser.parse_args()
results = []
for _ in range(args.runs):
    with tempfile.TemporaryDirectory(prefix="bitfly-benchmark-") as directory:
        marker = Path(directory) / "frame.json"
        start = time.perf_counter()
        child = subprocess.Popen([str(args.executable.resolve()), "--startup-probe", str(marker), "--quit-after", "3000"])
        process = psutil.Process(child.pid)
        peak_rss = 0
        peak_committed = 0
        available_at_start = psutil.virtual_memory().available
        cpu_seconds = 0.0
        observed_frame_ms = None
        frame_cpu_seconds = None
        frame_observed_at = None
        idle_cpu_seconds = None
        idle_observed_seconds = None
        while child.poll() is None:
            try:
                memory = process.memory_info()
                peak_rss = max(peak_rss, memory.rss, getattr(memory, "peak_wset", 0))
                peak_committed = max(peak_committed, getattr(memory, "private", memory.vms))
                cpu = process.cpu_times()
                cpu_seconds = cpu.user + cpu.system
            except psutil.Error:
                pass
            if observed_frame_ms is None and marker.exists():
                observed_frame_ms = round((time.perf_counter() - start) * 1000, 2)
                frame_cpu_seconds = cpu_seconds
                frame_observed_at = time.perf_counter()
            if frame_observed_at is not None and idle_cpu_seconds is None and time.perf_counter() - frame_observed_at >= 2:
                idle_cpu_seconds = max(0.0, cpu_seconds - frame_cpu_seconds)
                idle_observed_seconds = round(time.perf_counter() - frame_observed_at, 3)
            if time.perf_counter() - start > 20:
                child.kill()
                raise RuntimeError("Benchmark process failed to exit")
            time.sleep(0.01)
        if child.returncode != 0 or not marker.exists():
            raise RuntimeError("No actual rendered frame or application failed")
        results.append({**json.loads(marker.read_text()), "launchToObservedFrameMs": observed_frame_ms,
                        "peakWorkingSetBytes": peak_rss, "peakCommittedBytes": peak_committed,
                        "availableSystemMemoryAtStartBytes": available_at_start, "cpuSeconds": cpu_seconds,
                        "idleCpuSeconds": idle_cpu_seconds, "idleObservedSeconds": idle_observed_seconds})
report = {"platform": platform.platform(), "processor": platform.processor(), "runs": results,
          "conditions": "empty-session app, first frame, 3-second idle, warm filesystem cache after first run; not a controlled cold boot",
          "medianFirstFrameMs": statistics.median(row["firstFrameMs"] for row in results),
          "executableBytes": args.executable.stat().st_size}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
