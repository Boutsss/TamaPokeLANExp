"""The desktop installer's logic, with no window code in it.

Everything here is a Python twin of something the web installer already does,
and each is written against THE SAME files and the same board commands so the
two installers can never disagree about a release:

    unlock_app()       web/lock.js          + tools/lock_firmware.py
    parse_pak()        installer.js parsePak()
    verify_backup()    web/savefile.js      verifyBackup()
    Board.send_backup  web/savefile.js      sendBackup()
    Board.load_pack    installer.js         loadRegion()

The data it reads is the web installer's own folder (manifest.json, firmware/,
paks.json, sprites-<region>.pak), found next to the program. A release build
(tools/build_web.sh) therefore updates both installers at once.

tools/check_desktop_installer.py tests this module without a board.
"""
import base64
import hashlib
import json
import os
import queue
import re
import sys
import threading
import time
import zlib

# ---------------------------------------------------------------- the data folder

def data_dir_candidates():
    here = os.path.dirname(os.path.abspath(sys.executable if getattr(sys, "frozen", False) else __file__))
    out = [here, os.path.join(here, "web"), os.path.join(os.getcwd(), "web"), os.getcwd()]
    # running from the repository: tools/desktop_installer -> ../../web
    out.append(os.path.normpath(os.path.join(here, "..", "..", "web")))
    return out


def find_data_dir():
    """The folder holding manifest.json and firmware/, or None."""
    for d in data_dir_candidates():
        if os.path.isfile(os.path.join(d, "manifest.json")) and os.path.isdir(os.path.join(d, "firmware")):
            return d
    return None


def load_manifest(data_dir):
    with open(os.path.join(data_dir, "manifest.json"), encoding="utf-8") as f:
        m = json.load(f)
    builds = m.get("builds") or []
    if len(builds) != 1:
        raise ValueError("manifest.json should hold exactly one build")
    parts = []
    for p in builds[0].get("parts", []):
        path = p["path"].split("?", 1)[0]                   # drop the ?v= cache key
        parts.append((int(p["offset"]), os.path.join(data_dir, *path.split("/"))))
    for _, path in parts:
        if not os.path.isfile(path):
            raise ValueError("missing firmware file: %s" % os.path.basename(path))
    return m, parts


def load_paks(data_dir):
    """{region: meta} for every pack listed in paks.json whose file is present."""
    path = os.path.join(data_dir, "paks.json")
    if not os.path.isfile(path):
        return {}
    with open(path, encoding="utf-8") as f:
        regions = json.load(f).get("regions", {})
    out = {}
    for name, meta in regions.items():
        file = os.path.join(data_dir, "sprites-%s.pak" % name)
        if os.path.isfile(file):
            out[name] = dict(meta, file=file)
    return dict(sorted(out.items(), key=lambda kv: kv[1]["index"]))

# ---------------------------------------------------------------- the password lock

LOCK_MAGIC = b"TPLK"
CHECK_TEXT = b"TAMAPOKE-LOCK-OK"
APP_OFFSET = 0x10000


class WrongPassword(Exception):
    pass


def _key(password, lock):
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
    if lock.get("v") != 1 or lock.get("kdf") != "PBKDF2-SHA256":
        raise ValueError("this release uses a lock format this installer does not know")
    kdf = PBKDF2HMAC(algorithm=hashes.SHA256(), length=32,
                     salt=base64.b64decode(lock["salt"]), iterations=int(lock["iterations"]))
    return kdf.derive(password.encode("utf-8"))


def _open(key, blob):
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    if blob[:4] != LOCK_MAGIC:
        raise ValueError("not a locked TamaPoke file")
    return AESGCM(key).decrypt(blob[4:16], blob[16:], None)


def check_password(password, lock):
    """The derived key if the password is right; WrongPassword otherwise.
    Checked against the small known-plaintext block first, exactly as the page
    does, so a typo is reported in a second, not after decrypting 2.7 MB."""
    from cryptography.exceptions import InvalidTag
    key = _key(password, lock)
    try:
        ok = _open(key, base64.b64decode(lock["check"])) == CHECK_TEXT
    except InvalidTag:
        ok = False
    if not ok:
        raise WrongPassword("wrong password")
    return key


def unlock_app(key, lock, enc_path):
    """Decrypts app.bin.enc and proves it is the build the manifest describes."""
    with open(enc_path, "rb") as f:
        plain = _open(key, f.read())
    if len(plain) != int(lock["size"]) or hashlib.sha256(plain).hexdigest() != lock["sha256"]:
        raise ValueError("the unlocked firmware does not match its manifest (damaged download?)")
    return plain

# ---------------------------------------------------------------- sprite packs

def crc32_file(path):
    c = 0
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            c = zlib.crc32(chunk, c)
    return "%08x" % (c & 0xFFFFFFFF)


def parse_pak(data):
    """[(name, bytes)] from a TPAK bundle; raises on anything malformed."""
    if len(data) < 6 or data[:4] != b"TPAK":
        raise ValueError("invalid bundle")
    count = int.from_bytes(data[4:6], "little")
    at, index = 6, []
    for _ in range(count):
        if at + 1 > len(data):
            raise ValueError("truncated bundle index")
        n = data[at]; at += 1
        if not n or at + n + 4 > len(data):
            raise ValueError("invalid bundle index")
        name = data[at:at + n].decode("ascii"); at += n
        size = int.from_bytes(data[at:at + 4], "little"); at += 4
        index.append((name, size))
    items = []
    for name, size in index:
        if at + size > len(data):
            raise ValueError("truncated bundle payload")
        items.append((name, data[at:at + size])); at += size
    if at != len(data):
        raise ValueError("bundle contains trailing data")
    return items

# ---------------------------------------------------------------- save backups (savefile.js)

SAVE_MAGIC = b"TKPS"
SAVE_VERSION = 1
SAVE_HEADER_BYTES = 8
SAVE_CRC_BYTES = 2
_HEADER_RE = re.compile(r"^#\s*TamaPoke save,\s*(\d+)\s*bytes", re.I)
_DATA_RE = re.compile(r"^IMPORT [0-9a-fA-F]+$")


def crc16(data):
    """CRC-16/CCITT-FALSE, the firmware's and savefile.js's (check value 0x29B1)."""
    c = 0xFFFF
    for b in data:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def verify_backup(text):
    """(commands, blob) for a sound backup; raises ValueError with the reason.
    The SAME rules on the way out (a fresh capture) and the way back in (a
    file to restore): a backup is only worth anything if it was verified when
    it was taken."""
    lines = [ln.strip() for ln in str(text).splitlines() if ln.strip()]
    commands, declared, commit = [], None, False
    for ln in lines:
        if ln.startswith("#"):
            m = _HEADER_RE.match(ln)
            if m:
                declared = int(m.group(1))
            continue
        if commit:
            raise ValueError("there is content after the final IMPORT line")
        if ln == "IMPORT":
            commit = True
            commands.append(ln)
            continue
        if not _DATA_RE.match(ln):
            raise ValueError("unrecognised backup line: %s" % ln[:32])
        if len(ln[7:]) % 2:
            raise ValueError("a data line has an odd number of hex digits")
        commands.append(ln)
    if not commit:
        raise ValueError("backup is missing its final IMPORT commit line")
    if len(commands) < 2:
        raise ValueError("backup has no data, only a commit line")
    blob = bytes.fromhex("".join(c[7:] for c in commands[:-1]))
    if declared is not None and len(blob) != declared:
        raise ValueError("backup is %d bytes but declares %d" % (len(blob), declared))
    if len(blob) < SAVE_HEADER_BYTES + SAVE_CRC_BYTES:
        raise ValueError("backup is too short to be a save (%d bytes)" % len(blob))
    if blob[:4] != SAVE_MAGIC:
        raise ValueError("this is not a TamaPoke save")
    if blob[4] != SAVE_VERSION:
        raise ValueError("save format version %d, this installer understands %d" % (blob[4], SAVE_VERSION))
    want = blob[-2] | (blob[-1] << 8)
    got = crc16(blob[:-2])
    if got != want:
        raise ValueError("checksum mismatch: the data says 0x%04X, the bytes give 0x%04X" % (want, got))
    return commands, blob


def describe_backup(blob):
    """{'trainer': ...} -- best effort, for naming the file. Never raises."""
    out = {}
    try:
        at, end = SAVE_HEADER_BYTES, len(blob) - SAVE_CRC_BYTES
        while at + 4 <= end:
            klen = blob[at]
            if not klen or at + 1 + klen + 3 > end:
                break
            key = blob[at + 1:at + 1 + klen].decode("ascii", "replace")
            at += 1 + klen + 1
            n = blob[at] | (blob[at + 1] << 8); at += 2
            if at + n > end:
                break
            val = blob[at:at + n]; at += n
            if key == "tnam" and n:
                out["trainer"] = val.split(b"\0", 1)[0].decode("ascii", "replace").strip()
    except Exception:
        pass
    return out

# ---------------------------------------------------------------- the board

ESPRESSIF_VID = 0x303A


def list_ports():
    """[(device, label)], the board's own USB device first."""
    from serial.tools import list_ports as lp
    ports = list(lp.comports())
    ports.sort(key=lambda p: (p.vid != ESPRESSIF_VID, p.device))
    out = []
    for p in ports:
        label = p.device
        if p.vid == ESPRESSIF_VID:
            label += "  (ESP32-S3 board)"
        elif p.description and p.description != "n/a":
            label += "  (%s)" % p.description
        out.append((p.device, label))
    return out


class Board:
    """One serial connection to the running firmware's console.

    DTR and RTS are held LOW before the port opens: this board's USB-CDC
    resets on that signal, and the default toggle on open restarted the game
    every time a tool connected (tools/debugger was written for the same
    reason). A reader thread turns the byte stream into lines."""

    def __init__(self, device, log=print):
        import serial
        self.log = log
        self.ser = serial.Serial()
        self.ser.port = device
        self.ser.baudrate = 115200
        self.ser.timeout = 0.1
        # A write that cannot complete (the board restarted, the cable moved)
        # must raise, not hang the installer forever.
        self.ser.write_timeout = 10
        self.ser.dtr = False
        self.ser.rts = False
        self.ser.open()
        self.lines = queue.Queue()
        self._stop = False
        self._t = threading.Thread(target=self._pump, daemon=True)
        self._t.start()

    def _pump(self):
        carry = b""
        while not self._stop:
            try:
                # Whatever has arrived, at least one byte: read(4096) waited out
                # the whole timeout for the board's one-character '#' acks, 80
                # of them a file, and loading a region took 15 minutes.
                data = self.ser.read(max(1, self.ser.in_waiting))
            except Exception:
                break
            if not data:
                continue
            carry += data
            while b"\n" in carry:
                ln, carry = carry.split(b"\n", 1)
                self.lines.put(ln.decode("utf-8", "replace").rstrip("\r").strip())

    def close(self):
        self._stop = True
        try:
            self.ser.close()
        except Exception:
            pass
        self._t.join(1)

    def flush_lines(self):
        while not self.lines.empty():
            self.lines.get_nowait()

    def write_line(self, s):
        self.ser.write((s + "\n").encode("ascii"))
        self.ser.flush()

    def read_line(self, timeout):
        try:
            return self.lines.get(timeout=max(0.0, timeout))
        except queue.Empty:
            return None

    def wait_for(self, tokens, timeout=6.0):
        end = time.time() + timeout
        while time.time() < end:
            ln = self.read_line(end - time.time())
            if ln is None:
                return None
            if ln in tokens:
                return ln
        return None

    def command_lines(self, command, timeout=6.0):
        """(outcome 'DONE'/'ERR'/None, lines before it)."""
        self.flush_lines()
        self.write_line(command)
        out, end = [], time.time() + timeout
        while time.time() < end:
            ln = self.read_line(end - time.time())
            if ln is None:
                return None, out
            if ln in ("DONE", "ERR"):
                return ln, out
            out.append(ln)
        return None, out

    def expect_done(self, command, timeout=6.0):
        self.flush_lines()
        self.write_line(command)
        return self.wait_for(("DONE", "ERR"), timeout) == "DONE"

    # -- what is on the card
    def board_id(self):
        _, lines = self.command_lines("STATS")
        for ln in lines:
            m = re.match(r"^board=([0-9A-Fa-f]{6,16})$", ln)
            if m:
                return m.group(1).upper()
        return ""

    def installed_packs(self):
        """({index: 'missing'|'legacy'|crc}, sd_present) or (None, None) if the
        firmware does not answer PACKS."""
        outcome, lines = self.command_lines("PACKS", 20.0)
        if outcome is None:
            return None, None
        out = {}
        for ln in lines:
            m = re.match(r"^PACK (\d+) (missing|legacy|[0-9a-fA-F]{8})$", ln)
            if m:
                out[int(m.group(1))] = m.group(2).lower()
        return out, outcome != "ERR"

    # -- sprites (PUT protocol: 2 KB blocks, each acknowledged with '#')
    def put_file(self, name, data):
        self.flush_lines()
        self.write_line("PUT %s %d" % (name, len(data)))
        if self.wait_for(("OK", "ERR")) != "OK":
            return False
        for at in range(0, len(data), 2048):
            self.ser.write(data[at:at + 2048])
            if self.wait_for(("#", "ERR")) != "#":
                return False
        return self.wait_for(("DONE", "ERR"), 30.0) == "DONE"

    def load_pack(self, meta, progress=lambda done, total, name: None):
        """Verifies the bundle file, then sends it the way the page does:
        PACK BEGIN, every file (3 tries each), PACK COMMIT with its crc."""
        with open(meta["file"], "rb") as f:
            data = f.read()
        if len(data) != int(meta["bytes"]):
            raise ValueError("expected %d bytes, the file has %d" % (meta["bytes"], len(data)))
        crc = "%08x" % (zlib.crc32(data) & 0xFFFFFFFF)
        if crc != meta["crc32"].lower():
            raise ValueError("checksum %s does not match %s" % (crc, meta["crc32"]))
        items = parse_pak(data)
        if not self.expect_done("PACK BEGIN %d" % meta["index"]):
            raise RuntimeError("the board refused to begin the pack")
        for i, (name, blob) in enumerate(items):
            progress(i, len(items), name)
            for attempt in range(3):
                if self.put_file(name, blob):
                    break
                self.log("retrying %s (%d/2)" % (name, attempt + 1))
                time.sleep(0.25)
            else:
                raise RuntimeError("could not send %s after 3 tries" % name)
        progress(len(items), len(items), "")
        if not self.expect_done("PACK COMMIT %d %s" % (meta["index"], crc), 30.0):
            raise RuntimeError("the files arrived but the board could not validate the pack")

    # -- the save
    def capture_save(self):
        """The verified EXPORT text; raises if it is not a sound backup."""
        self.flush_lines()
        self.write_line("EXPORT")
        lines, end = [], time.time() + 60.0      # a full PC box is ~1400 lines
        while time.time() < end:
            ln = self.read_line(end - time.time())
            if ln is None:
                raise RuntimeError("the board did not finish the export")
            if ln == "EXPORT FAIL":
                raise RuntimeError("the firmware could not build a backup")
            if ln.startswith("# TamaPoke save") or ln.startswith("IMPORT "):
                lines.append(ln)
            if ln == "IMPORT":
                lines.append(ln)
                break
        if not lines or lines[-1] != "IMPORT":
            raise RuntimeError("the export stopped before the end")
        text = "\n".join(lines) + "\n"
        verify_backup(text)                      # throws if it is not sound
        return text

    def send_backup(self, text):
        """savefile.js sendBackup(): every chunk acknowledged, then the commit,
        which the firmware validates as a whole before replacing anything."""
        commands, _ = verify_backup(text)
        errors = ("IMPORT ODD", "IMPORT BAD", "IMPORT FULL")
        acks, first = True, True
        for c in commands[:-1]:
            self.write_line(c)
            if not acks:
                time.sleep(0.125)
                continue
            reply = self.wait_for(("IMPORT MORE",) + errors, 4.0 if first else 8.0)
            if reply and reply != "IMPORT MORE":
                raise RuntimeError(reply)
            if not reply:
                if not first:
                    raise RuntimeError("the board stopped acknowledging save data")
                acks = False
                time.sleep(0.125)
            first = False
        self.flush_lines()
        self.write_line("IMPORT")
        result = self.wait_for(("IMPORT OK", "IMPORT REJECTED", "IMPORT EMPTY"), 15.0)
        if result != "IMPORT OK":
            raise RuntimeError(result or "the board did not answer")

# ---------------------------------------------------------------- flashing

def flash(device, parts, app_bytes, workdir, log):
    """Writes the four parts with esptool, in-process. write-flash only
    touches the ranges it is given, so the save partition (NVS) is never
    erased -- the same promise the web installer's four-part manifest makes."""
    import contextlib
    import io
    import esptool
    args = ["--chip", "esp32s3", "--port", device, "--baud", "921600",
            "--before", "default-reset", "--after", "hard-reset", "write-flash", "-z"]
    for offset, path in parts:
        if offset == APP_OFFSET:
            path = os.path.join(workdir, "app.bin")
            with open(path, "wb") as f:
                f.write(app_bytes)
        args += ["0x%x" % offset, path]

    real_out = sys.stdout

    class Sink(io.TextIOBase):
        busy = False

        def write(self, s):
            # A logger that itself prints would land back here: send anything
            # written while we are already logging to the real stdout instead.
            if Sink.busy:
                if real_out:
                    real_out.write(s)
                return len(s)
            Sink.busy = True
            try:
                for ln in s.replace("\r", "\n").splitlines():
                    ln = ln.strip()
                    # esptool redraws a progress bar per block: keep the log to
                    # what a person reads (the parts written, verified, reset)
                    if ln and not ln.startswith("Writing at") and "=>" not in ln and "%" not in ln:
                        log(ln)
            finally:
                Sink.busy = False
            return len(s)
    sink = Sink()
    with contextlib.redirect_stdout(sink), contextlib.redirect_stderr(sink):
        esptool.main(args)
