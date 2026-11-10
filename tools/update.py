#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Portable Xenolith menu; the pinned upstream module owns the update protocol."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def load_installer():
    path = ROOT / "tools/fm1_install.py"
    spec = importlib.util.spec_from_file_location("xenolith_pinned_installer", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def inspect_package(path, bundled=False):
    path = Path(path).expanduser().resolve()
    if path.suffix.lower() != ".fwsc" or not path.is_file():
        raise ValueError("Choose an existing .fwsc file.")
    if path.stat().st_size > 32 * 1024 * 1024:
        raise ValueError("Package exceeds 32 MiB.")
    raw = path.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if bundled:
        manifest = json.loads((ROOT / "firmware/package.json").read_text(encoding="utf-8"))
        if digest != manifest["sha256"]:
            raise ValueError("Bundled firmware checksum differs. Extract a fresh release ZIP.")
    installer = load_installer()
    try:
        product, _, _ = installer.load_package(str(path), False)
    except installer.InstallError as error:
        raise ValueError(str(error)) from error
    return {"path": str(path), "identity": product, "bytes": len(raw), "sha256": digest}


def select_port(names, ask=input):
    if not names:
        raise ValueError("No MIDI input is connected.")
    for i, name in enumerate(names, 1):
        print(f"  {i:2}  {name}")
    choice = ask("FM-1 port number [Enter cancels]: ").strip()
    if not choice:
        return None
    if not choice.isdigit() or not 1 <= int(choice) <= len(names):
        raise ValueError("Choose one number from the list.")
    return names[int(choice) - 1]


def run_action(action, package=None, backend=None, ask=input, installer=None):
    if action not in ("1", "2", "3"):
        raise ValueError("Unknown selection. Nothing was written.")
    bundled = action == "1"
    checked = None
    if action != "2":
        if bundled:
            package = ROOT / "firmware/XENOLITH1.fwsc"
        elif package is None:
            package = ask("Local .fwsc path [Enter cancels]: ").strip().strip('"')
            if not package:
                return 0
        checked = inspect_package(package, bundled)
        print(f"\nPackage : {checked['identity']}\nSHA-256 : {checked['sha256']}\n")
    installer = installer or load_installer()
    backend = backend or installer.MidoBackend()
    port = select_port(backend.input_names(), ask)
    if port is None:
        return 0
    # Only the selected unfamiliar name is added; unrelated devices stay outside
    # automatic reconnect discovery. The update protocol remains upstream's.
    installer.PORT_RE = re.compile(installer.PORT_RE.pattern + "|XENOLITH|JIANT|" + re.escape(port), re.I)
    argv = ["--info", "--port", port] if action == "2" else [checked["path"], "--port", port]
    return installer.main(argv, backend=backend,
                          ask=lambda prompt: ask(prompt).strip().lower() in ("y", "yes"))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-package", action="store_true", help="Validate bundled package without opening MIDI")
    args = parser.parse_args(argv)
    if args.check_package:
        print(json.dumps(inspect_package(ROOT / "firmware/XENOLITH1.fwsc", True), indent=2))
        return 0
    print("\n+-----------------------------------------+\n"
          "| XX29 / XENOLITH / FIRMWARE DISPATCH       |\n"
          "+-----------------------------------------+\n"
          "  1  INSTALL BUNDLED XENOLITH\n"
          "  2  READ DEVICE IDENTITY\n"
          "  3  INSTALL LOCAL .FWSC\n"
          "  0  EXIT\n")
    choice = input("Selection: ").strip()
    if choice in ("", "0"):
        return 0
    try:
        return run_action(choice)
    except ImportError:
        print("Run Setup updater.bat once to install MIDI support.", file=sys.stderr)
        return 2


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, EOFError, KeyboardInterrupt) as error:
        print(f"Updater stopped: {error}", file=sys.stderr)
        raise SystemExit(1)
