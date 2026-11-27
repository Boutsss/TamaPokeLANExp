#!/usr/bin/env python3
"""Tests the desktop installer's core (tools/desktop_installer/installer_core.py)
without a board, against things it did not write itself:

  - CRC-16/CCITT-FALSE against its PUBLISHED check value, not a second copy;
  - tools/fixtures/golden.tpsave, which the real firmware produced;
  - tools/lock_firmware.py, the tool that locks the release, for the unlock;
  - the committed web/ release itself: its manifest and every sprite pack.

    python tools/check_desktop_installer.py     (needs: pip install cryptography)
"""
import os
import sys
import tempfile
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools", "desktop_installer"))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import installer_core as core  # noqa: E402

bad = 0


def ck(ok, what):
    global bad
    print("%s  %s" % ("PASS" if ok else "FAIL", what))
    if not ok:
        bad += 1


def raises(fn, *a):
    try:
        fn(*a)
    except Exception:
        return True
    return False


# ---- the save format
ck(core.crc16(b"123456789") == 0x29B1, "crc16 gives CRC-16/CCITT-FALSE's published check value 0x29B1")
golden = open(os.path.join(ROOT, "tools", "fixtures", "golden.tpsave"), encoding="ascii").read()
cmds, blob = core.verify_backup(golden)
ck(cmds[-1] == "IMPORT" and blob[:4] == b"TKPS", "a backup the real firmware wrote verifies")
lines = golden.strip().splitlines()
ck(raises(core.verify_backup, "\n".join(lines[:-2] + lines[-1:])), "a capture missing a line is refused (byte count)")
ck(raises(core.verify_backup, "\n".join(lines[:-1])), "a capture with no commit line is refused")
# flip one hex digit in the first data line
i = next(k for k, ln in enumerate(lines) if ln.startswith("IMPORT "))
ln = lines[i]
flipped = ln[:20] + ("0" if ln[20] != "0" else "1") + ln[21:]
ck(raises(core.verify_backup, "\n".join(lines[:i] + [flipped] + lines[i + 1:])), "one changed digit fails the checksum")
ck(raises(core.verify_backup, "hello\nIMPORT\n"), "something that is not a backup is refused")

# ---- the lock, against the real lock tool
try:
    import lock_firmware
    with tempfile.TemporaryDirectory() as t:
        app = os.urandom(70000)
        src, dst = os.path.join(t, "app.bin"), os.path.join(t, "app.bin.enc")
        open(src, "wb").write(app)
        lock = lock_firmware.lock_file("correct horse", src, dst, iterations=1000)
        ck(raises(core.check_password, "wrong horse", lock), "a wrong password is refused")
        key = core.check_password("correct horse", lock)
        ck(core.unlock_app(key, lock, dst) == app, "the right one unlocks exactly what lock_firmware.py locked")
        enc = bytearray(open(dst, "rb").read()); enc[-1] ^= 1; open(dst, "wb").write(enc)
        ck(raises(core.unlock_app, key, lock, dst), "a damaged locked file is refused, never flashed")
except ImportError as e:
    print("SKIP  the lock checks (%s)" % e)

# ---- the committed release
data = os.path.join(ROOT, "web")
m, parts = core.load_manifest(data)
ck(sorted(o for o, _ in parts) == [0x0, 0x8000, 0xE000, 0x10000], "the manifest's four parts, at their offsets, all present")
ck("lock" in m, "and the game part is locked")
paks = core.load_paks(data)
ck(len(paks) >= 1, "sprite packs found (%d)" % len(paks))
allok = True
for name, meta in paks.items():
    raw = open(meta["file"], "rb").read()
    crc = "%08x" % (zlib.crc32(raw) & 0xFFFFFFFF)
    items = core.parse_pak(raw)
    if crc != meta["crc32"].lower() or len(raw) != meta["bytes"] or not items:
        allok = False
        print("      %s: crc %s vs %s, %d files" % (name, crc, meta["crc32"], len(items)))
ck(allok, "every pack matches paks.json and parses")
ck(raises(core.parse_pak, b"TPAK\x01\x00\x05abc"), "a truncated pack is refused")

print("check_desktop_installer: %s" % ("%d FAILED" % bad if bad else "all passed"))
sys.exit(1 if bad else 0)
