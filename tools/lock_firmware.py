#!/usr/bin/env python3
"""Password-locks the web installer's firmware.

The installer page is public (GitHub Pages), so anything it can flash, anyone
can flash. This encrypts the ONE part that is the game -- app.bin, flashed at
0x10000 -- so the page can only install it for somebody who knows the password.
The other three parts (bootloader, partition table, boot_app0) are the stock
Espressif/Arduino files every ESP32-S3 sketch ships and are left as they are.

    python3 tools/lock_firmware.py                 # lock web/firmware/app.bin
    TAMAPOKE_PASSWORD=... python3 tools/lock_firmware.py

The password comes from $TAMAPOKE_PASSWORD, else the first line of
.install-password at the repo root (gitignored). There is NO default: a build
with no password refuses rather than shipping the game in the clear.

What it writes:
  web/firmware/app.bin.enc    b"TPLK" + 12-byte IV + AES-256-GCM(app.bin)
  web/manifest.json           the 0x10000 part -> firmware/app.bin.enc?v=<hash>,
                              and a "lock" block: KDF, iterations, salt, and a
                              known plaintext encrypted under the same key so
                              the page can say "wrong password" before it
                              downloads 1.7 MB.

The key is PBKDF2-HMAC-SHA256(password, salt, iterations) -- what WebCrypto
offers natively, so web/lock.js needs no library. GCM is authenticated: a wrong
password or a damaged file fails to decrypt, it never flashes garbage.

WHAT THIS IS NOT: the source is public on a public fork, so anybody who wants
the firmware can still build it with arduino-cli. This gates the convenient
one-click install, which is what was asked for. Change the password by editing
.install-password and re-running this; the old one stops working at the next
Pages deploy.
"""
import argparse
import base64
import hashlib
import json
import os
import secrets
import sys

from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAGIC = b"TPLK"
ITERATIONS = 600000           # OWASP's 2023 figure for PBKDF2-SHA256
CHECK_TEXT = b"TAMAPOKE-LOCK-OK"
APP_OFFSET = 0x10000


def b64(data):
    return base64.b64encode(data).decode("ascii")


def password_from_env():
    pw = os.environ.get("TAMAPOKE_PASSWORD", "")
    if not pw:
        path = os.path.join(ROOT, ".install-password")
        if os.path.exists(path):
            with open(path, encoding="utf-8") as f:
                pw = f.readline().strip()
    return pw


def derive(password, salt, iterations):
    kdf = PBKDF2HMAC(algorithm=hashes.SHA256(), length=32, salt=salt, iterations=iterations)
    return kdf.derive(password.encode("utf-8"))


def encrypt(key, data):
    iv = secrets.token_bytes(12)
    return MAGIC + iv + AESGCM(key).encrypt(iv, data, None)


def lock_file(password, src, dst, iterations=ITERATIONS):
    """Encrypts src into dst and returns the manifest's "lock" block."""
    salt = secrets.token_bytes(16)
    key = derive(password, salt, iterations)
    with open(src, "rb") as f:
        plain = f.read()
    with open(dst, "wb") as f:
        f.write(encrypt(key, plain))
    check = encrypt(key, CHECK_TEXT)
    return {
        "v": 1,
        "kdf": "PBKDF2-SHA256",
        "iterations": iterations,
        "salt": b64(salt),
        "check": b64(check),
        "size": len(plain),
        "sha256": hashlib.sha256(plain).hexdigest(),
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--in", dest="src", default=os.path.join(ROOT, "web", "firmware", "app.bin"))
    ap.add_argument("--out", dest="dst", default=None)
    ap.add_argument("--password", default=None, help="for tests; normally use the env or the file")
    ap.add_argument("--iterations", type=int, default=ITERATIONS)
    ap.add_argument("--print-lock", action="store_true",
                    help="print the lock block and do not touch the manifest (tests)")
    a = ap.parse_args()

    password = a.password or password_from_env()
    if not password:
        print("no install password: set TAMAPOKE_PASSWORD or write one into .install-password")
        print("(refusing to ship the firmware unlocked)")
        return 1
    if len(password) < 8:
        print("the install password must be at least 8 characters")
        return 1
    if not os.path.exists(a.src):
        print("missing %s -- run tools/build_web.sh (it compiles it)" % a.src)
        return 1
    dst = a.dst or a.src + ".enc"
    lock = lock_file(password, a.src, dst, a.iterations)
    if a.print_lock:
        print(json.dumps(lock))
        return 0

    path = os.path.join(ROOT, "web", "manifest.json")
    with open(path, encoding="utf-8") as f:
        m = json.load(f)
    digest = hashlib.sha256(open(dst, "rb").read()).hexdigest()[:16]
    found = 0
    for build in m.get("builds", []):
        for part in build.get("parts", []):
            if int(part["offset"]) == APP_OFFSET:
                part["path"] = "firmware/app.bin.enc?v=" + digest
                found += 1
    if found != 1:
        print("expected exactly one part at 0x10000 in the manifest, found %d" % found)
        return 1
    m["lock"] = lock
    with open(path, "w", encoding="utf-8", newline="") as f:
        json.dump(m, f, indent=2)
    print("locked app.bin (%d bytes) -> %s" % (lock["size"], os.path.relpath(dst, ROOT)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
