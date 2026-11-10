#!/usr/bin/env python3
"""Windows tool-path adapter for an explicitly selected local FM-1 platform (USB, no BLE)."""
import hashlib
import argparse
import importlib.util
import os
from pathlib import Path
import subprocess
import sys

BENCH = Path(__file__).resolve().parent
SOURCE = BENCH.parent / "vendor/fm1-base"
TOOLCHAIN = Path(os.environ.get("JIELI_PI32_ROOT", str(BENCH.parent / "toolchain/pi32")))
SDK = BENCH.parent / "sdk-resources"
sys.stdout.reconfigure(encoding="utf-8")
sys.stderr.reconfigure(encoding="utf-8")
os.environ["PYTHONUTF8"] = "1"
os.environ["X0X_FONTSET"] = "terminus"
upstream = None

def windows_tool(tool, *args):
    names = {"cc":"clang.exe", "pi32v2/bin/ld":"pi32v2-ld.exe", "common/bin/objcopy":"llvm-objcopy.exe",
             "common/bin/objdump":"llvm-objdump.exe", "common/bin/llvm-ar":"llvm-ar.exe", "common/bin/llvm-link":"llvm-link.exe"}
    if tool not in names:
        raise RuntimeError(f"No verified Windows mapping for upstream tool {tool}")
    rel = [str(a.resolve().relative_to(upstream.ROOT)) if isinstance(a, Path) else str(a) for a in args]
    if tool == "cc":
        rel = ["-target", "pi32v2", *rel]
    result = subprocess.run([str(TOOLCHAIN / "bin" / names[tool]), *rel], cwd=upstream.ROOT,
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode:
        sys.stderr.write(result.stdout + result.stderr)
        raise SystemExit(f"Windows build: {tool} failed ({result.returncode})")
    if result.stderr.strip():
        sys.stderr.write(result.stderr)
    return result.stdout

def main():
    global TOOLCHAIN, upstream
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=BENCH.parent / "app")
    parser.add_argument("--platform", type=Path, help="Local platform checkout; Xenolith defaults to xx29-fm1-platform, other apps to fm1-base.")
    parser.add_argument("--toolchain", type=Path, default=TOOLCHAIN)
    args = parser.parse_args()
    platform = (args.platform or (BENCH / "xx29-fm1-platform"
                if args.source.resolve() == (BENCH.parent / "XENOLITH/source").resolve() else SOURCE)).resolve()
    if not (platform / "tools/build.py").is_file():
        raise SystemExit(f"Missing platform builder: {platform}. No upstream fallback is used.")
    sys.path.insert(0, str(platform / "tools"))
    spec = importlib.util.spec_from_file_location("fm1_base_build", platform / "tools/build.py")
    upstream = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(upstream)
    TOOLCHAIN = args.toolchain.resolve()
    compiler = TOOLCHAIN / "bin/clang.exe"
    if hashlib.sha256(compiler.read_bytes()).hexdigest() != "d4bc97cf421c1ccc373dd8924ce88720ff7457b3f1478d15df4aeda6ae923bce":
        raise SystemExit("Unexpected JieLi compiler; build stopped.")
    for relative, expected in upstream.SDK_SHA256.items():
        path = SDK / "cpu/wl82/tools" / relative
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise SystemExit(f"SDK digest mismatch: {relative}; build stopped.")
    upstream.tc = windows_tool
    upstream.toolchain = lambda: TOOLCHAIN
    upstream.use_docker = lambda: False
    sys.argv = [str(platform / "tools/build.py"), str(args.source.resolve()), "--no-ble", "--sdk", str(SDK)]
    return upstream.main()

if __name__ == "__main__":
    raise SystemExit(main())
