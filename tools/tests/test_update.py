# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline wrapper checks. These never enumerate or open physical MIDI."""
import contextlib
import io
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import update


class Backend:
    def input_names(self):
        return ["Unrelated keyboard", "XENOLITH 0"]


class Installer:
    PORT_RE = re.compile("fm-?1", re.I)
    def __init__(self):
        self.calls = []
    def main(self, argv, backend=None, ask=None):
        self.calls.append(argv)
        self.confirm = ask
        return 5


class UpdaterTests(unittest.TestCase):
    def test_bundled_package(self):
        result = update.inspect_package(update.ROOT / "firmware/XENOLITH1.fwsc", True)
        self.assertEqual(result["sha256"], json.loads((update.ROOT / "firmware/package.json").read_text())["sha256"])

    def test_tamper_rejected_before_protocol(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "firmware").mkdir()
            (root / "firmware/package.json").write_text('{"sha256":"wrong"}')
            file = root / "tampered.fwsc"
            file.write_bytes(b"invalid")
            with patch.object(update, "ROOT", root), self.assertRaises(ValueError):
                update.inspect_package(file, True)

    def test_missing_and_wrong_suffix(self):
        with self.assertRaises(ValueError):
            update.inspect_package("missing.fwsc")
        with self.assertRaises(ValueError):
            update.inspect_package(__file__)

    def test_empty_port_and_bad_choice(self):
        with self.assertRaises(ValueError):
            update.select_port([])
        with self.assertRaises(ValueError):
            update.select_port(["FM-1"], lambda _: "2")
        self.assertIsNone(update.select_port(["FM-1"], lambda _: ""))

    def test_identity_does_not_pass_package(self):
        installer = Installer()
        result = update.run_action("2", backend=Backend(), installer=installer, ask=lambda _: "2")
        self.assertEqual(result, 5)
        self.assertEqual(installer.calls, [["--info", "--port", "XENOLITH 0"]])

    def test_install_keeps_confirmation_and_error(self):
        installer = Installer()
        result = update.run_action("1", backend=Backend(), installer=installer, ask=lambda _: "2")
        self.assertEqual(result, 5)
        self.assertNotIn("--yes", installer.calls[0])
        self.assertNotIn("--force", installer.calls[0])
        self.assertEqual(installer.calls[0][-2:], ["--port", "XENOLITH 0"])
        self.assertFalse(installer.confirm("Install? "))  # port answer "2" is not consent

    def test_cancel_does_not_install(self):
        installer = Installer()
        self.assertEqual(update.run_action("3", backend=Backend(), installer=installer, ask=lambda _: ""), 0)
        self.assertEqual(installer.calls, [])

    def test_invalid_action(self):
        with self.assertRaises(ValueError):
            update.run_action("4", backend=Backend(), installer=Installer())


if __name__ == "__main__":
    with contextlib.redirect_stdout(io.StringIO()):
        unittest.main(verbosity=2)
