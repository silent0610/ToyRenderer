#!/usr/bin/env python3
"""把模型清单交给一次 MyToyRenderer 进程，批量测预处理时间。"""

import argparse
import subprocess
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description="Write a model list and run one batch process.")
    parser.add_argument("models", nargs="+", help="glTF paths relative to the asset directory")
    parser.add_argument("--exe", default="Build/windows/x64/release/MyToyRenderer.exe")
    parser.add_argument("--list", default="Build/bench_models.txt")
    parser.add_argument("--out", default="Build/bench.csv")
    parser.add_argument("--resolution", type=int, default=128)
    parser.add_argument("--queries", type=int, default=4096)
    parser.add_argument("--warmup", type=int, default=50, help="frames skipped per model before timing")
    parser.add_argument("--repeat", type=int, default=50, help="timed frames averaged per model")
    args = parser.parse_args()

    list_path = Path(args.list)
    list_path.parent.mkdir(parents=True, exist_ok=True)
    list_path.write_text("\n".join(args.models) + "\n", encoding="utf-8")

    command = [
        args.exe,
        "--batch-list",
        str(list_path),
        "--out",
        args.out,
        "--resolution",
        str(args.resolution),
        "--queries",
        str(args.queries),
        "--repeat",
        str(args.repeat),
        "--warmup",
        str(args.warmup),
    ]
    print(" ".join(command), flush=True)
    return subprocess.call(command)


if __name__ == "__main__":
    sys.exit(main())
