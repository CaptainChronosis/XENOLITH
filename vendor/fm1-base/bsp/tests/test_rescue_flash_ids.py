"""Offline rescue eligibility and protected-region checks; no USB access."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location(
    "fm1_rescue", Path(__file__).resolve().parents[1] / "tools/fm1_rescue.py")
rescue = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(rescue)


class Device:
    def __init__(self, fid, key=0x980F, typ=3):
        self.identity = key, typ, fid
        self.flash = bytearray(rescue.FLASH_SIZE)
        self.reads = []
        self.writes = []
        self.started = False
        self.restarted = False

    def inquiry(self):
        return "WL82", "test"

    def start_loader(self, loader):
        self.started = True

    def chip(self):
        return self.identity

    def read(self, addr, size):
        self.reads.append((addr, size))
        return bytes(self.flash[addr:addr + size])

    def write_sector(self, addr, data):
        self.writes.append(addr)
        self.flash[addr:addr + len(data)] = data

    def run_app(self):
        self.restarted = True


class RescueIdentityTests(unittest.TestCase):
    def run_rescue(self, dev, image=None, write=False):
        if image is None:
            image = bytes(rescue.APP_END)
        with tempfile.TemporaryDirectory() as directory:
            result = rescue.rescue(dev, image, b"test", write, directory,
                                   say=lambda *args, **kwargs: None)
            backups = list(Path(directory).glob("*.bin"))
            self.assertEqual(len(backups), 1)
            self.assertEqual(backups[0].stat().st_size, rescue.FLASH_SIZE)
        return result

    def test_both_known_parts_can_be_checked_without_writes(self):
        for fid in (0x856014, 0xB36014):
            with self.subTest(fid=fid):
                dev = Device(fid)
                self.assertFalse(self.run_rescue(dev))
                self.assertEqual(dev.reads, [(0, rescue.FLASH_SIZE)])
                self.assertEqual(dev.writes, [])
                self.assertFalse(dev.restarted)

    def test_unknown_or_wrong_platform_rejected_before_flash_read(self):
        identities = [(0x980F, 3, x) for x in
                      (0, 0xFFFFFF, 0x856015, 0xB36015, 0xEF4014, 0xFFB36014)]
        identities += [(0x980E, 3, x) for x in rescue.KNOWN_FLASH_IDS]
        identities += [(0x980F, 2, x) for x in rescue.KNOWN_FLASH_IDS]
        for key, typ, fid in identities:
            with self.subTest(key=key, typ=typ, fid=fid):
                dev = Device(fid, key, typ)
                with self.assertRaises(rescue.Fail):
                    self.run_rescue(dev, write=True)
                self.assertEqual(dev.reads, [])
                self.assertEqual(dev.writes, [])

    def test_protected_head_still_rejects_both_parts(self):
        for fid in rescue.KNOWN_FLASH_IDS:
            with self.subTest(fid=fid):
                dev = Device(fid)
                image = bytearray(rescue.APP_END)
                image[rescue.WRITE_MIN - 1] = 1
                with self.assertRaises(rescue.Fail):
                    self.run_rescue(dev, bytes(image), write=True)
                self.assertEqual(dev.writes, [])
                self.assertFalse(dev.restarted)

    def test_changed_app_sector_only_preserves_tail(self):
        for fid in rescue.KNOWN_FLASH_IDS:
            with self.subTest(fid=fid):
                dev = Device(fid)
                dev.flash[rescue.APP_END:] = b"\x5a" * (rescue.FLASH_SIZE - rescue.APP_END)
                image = bytearray(rescue.APP_END)
                image[rescue.WRITE_MIN] = 1
                self.assertTrue(self.run_rescue(dev, bytes(image), write=True))
                self.assertEqual(dev.writes, [rescue.WRITE_MIN])
                self.assertEqual(dev.flash[rescue.APP_END:],
                                 b"\x5a" * (rescue.FLASH_SIZE - rescue.APP_END))
                self.assertTrue(dev.restarted)


if __name__ == "__main__":
    unittest.main()
