#!/usr/bin/env python3
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

def find_upx(solution_dir: str | None) -> str | None:
    if solution_dir:
        cand = Path(solution_dir) / "tools" / "upx.exe"
        if cand.is_file():
            return str(cand)
    return shutil.which("upx") or shutil.which("upx.exe")

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("target", help="要压缩的可执行文件路径")
    ap.add_argument("--solution-dir", default=None, help="解决方案目录")
    args = ap.parse_args()

    target = Path(args.target)
    if not target.is_file():
        print(f"[upx] 目标不存在，跳过：{target}", file=sys.stderr)
        return 0

    upx = find_upx(args.solution_dir)
    if not upx:
        print("[upx] 未在 SolutionDir\\tools 或 PATH 中找到 UPX，跳过压缩（构建继续）。",
              file=sys.stderr)
        return 0

    cmd = [upx]
    cmd.append(str(target))

    try:
        r = subprocess.run(cmd, capture_output=True, text=True)
    except OSError as e:
        print(f"[upx] 调用失败，跳过：{e}", file=sys.stderr)
        return 0

    if r.returncode != 0:
        print(f"[upx] 未压缩（退出码 {r.returncode}），跳过。\n{r.stderr.strip()}",
              file=sys.stderr)
        return 0

    if r.stdout:
        print(r.stdout.strip())
    print(f"[upx] 已压缩：{target}")
    return 0

if __name__ == "__main__":
    sys.exit(main())