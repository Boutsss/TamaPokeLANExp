"""TamaPoke desktop installer -- the web installer as a Windows program.

Does what web/index.html does, from the same files, with no website:
install the (password-locked) firmware, load the sprite packs onto the SD
card, and back up / restore the save. Every install backs the save up first.

    python tools/desktop_installer/tamapoke_installer.py        # from the repo
    tools/desktop_installer/build_exe.bat                       # -> TamaPokeInstaller.exe

The logic is installer_core.py; this file is only the window.
"""
import datetime
import os
import queue
import tempfile
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

import installer_core as core

APP = "TamaPoke Installer"
NOTICE = ("Unofficial, free fan project. Not affiliated with or endorsed by Nintendo, "
          "Game Freak, Creatures or The Pokemon Company. Pokemon is (c) its owners.")


def backups_dir():
    d = os.path.join(os.path.expanduser("~"), "Documents", "TamaPoke backups")
    os.makedirs(d, exist_ok=True)
    return d


def backup_name(text):
    try:
        _, blob = core.verify_backup(text)
        who = "".join(ch for ch in core.describe_backup(blob).get("trainer", "") if ch.isalnum()) or "save"
    except Exception:
        who = "save"
    return "tamapoke-%s-%s.tpsave" % (who, datetime.datetime.now().strftime("%Y-%m-%d-%H-%M-%S"))


class App:
    def __init__(self, root):
        self.root = root
        self.events = queue.Queue()
        self.busy = False
        root.title(APP)
        root.minsize(620, 640)

        self.data_dir = core.find_data_dir()
        self.manifest, self.parts, self.paks = None, [], {}
        data_error = None
        if self.data_dir:
            try:
                self.manifest, self.parts = core.load_manifest(self.data_dir)
                self.paks = core.load_paks(self.data_dir)
            except Exception as e:
                data_error = str(e)
        else:
            data_error = "could not find the installer files (manifest.json and firmware/) next to this program"

        pad = {"padx": 10, "pady": 4}
        top = ttk.Frame(root); top.pack(fill="x", **pad)
        version = self.manifest.get("version", "?") if self.manifest else "?"
        ttk.Label(top, text="%s  -  firmware v%s" % (APP, version), font=("Segoe UI", 14, "bold")).pack(anchor="w")
        ttk.Label(top, text=NOTICE, wraplength=590, foreground="#666").pack(anchor="w")

        # -- the board
        b = ttk.LabelFrame(root, text="Board"); b.pack(fill="x", **pad)
        self.port = tk.StringVar()
        self.port_box = ttk.Combobox(b, textvariable=self.port, state="readonly", width=46)
        self.port_box.pack(side="left", padx=6, pady=6)
        ttk.Button(b, text="Refresh", command=self.refresh_ports).pack(side="left", padx=4)
        ttk.Button(b, text="Check board", command=self.check_board).pack(side="left", padx=4)
        ttk.Label(root, text="Plug the board in with USB. If it does not appear: hold BOOT, tap RESET, "
                  "release BOOT, then Refresh.", wraplength=590, foreground="#666").pack(anchor="w", padx=12)

        # -- install
        f = ttk.LabelFrame(root, text="1. Install the game"); f.pack(fill="x", **pad)
        row = ttk.Frame(f); row.pack(fill="x", padx=6, pady=6)
        ttk.Label(row, text="Install password:").pack(side="left")
        self.password = tk.StringVar()
        ttk.Entry(row, textvariable=self.password, show="*", width=24).pack(side="left", padx=6)
        self.install_btn = ttk.Button(row, text="Back up save, then install", command=self.install)
        self.install_btn.pack(side="left", padx=6)
        ttk.Label(f, text="Your save is backed up first and the save area is never erased.",
                  foreground="#666").pack(anchor="w", padx=6, pady=(0, 6))

        # -- sprites
        s = ttk.LabelFrame(root, text="2. Sprite packs (onto the board's microSD card)"); s.pack(fill="x", **pad)
        self.pak_vars, self.pak_status = {}, {}
        grid = ttk.Frame(s); grid.pack(fill="x", padx=6, pady=4)
        for i, (name, meta) in enumerate(self.paks.items()):
            v = tk.BooleanVar(value=(name == "kanto"))
            self.pak_vars[name] = v
            ttk.Checkbutton(grid, text="%s  (%d MB)" % (name.title(), meta["bytes"] // 1000000),
                            variable=v).grid(row=i // 3 * 2, column=i % 3, sticky="w", padx=6)
            lbl = ttk.Label(grid, text="", foreground="#666")
            lbl.grid(row=i // 3 * 2 + 1, column=i % 3, sticky="w", padx=24)
            self.pak_status[meta["index"]] = lbl
        if not self.paks:
            ttk.Label(grid, text="(no sprite packs found next to this program)").grid(row=0, column=0)
        self.pak_btn = ttk.Button(s, text="Load selected packs", command=self.load_packs)
        self.pak_btn.pack(anchor="w", padx=6, pady=6)

        # -- save
        v = ttk.LabelFrame(root, text="3. Save backups"); v.pack(fill="x", **pad)
        r2 = ttk.Frame(v); r2.pack(fill="x", padx=6, pady=6)
        self.backup_btn = ttk.Button(r2, text="Back up save...", command=self.backup)
        self.backup_btn.pack(side="left")
        self.restore_btn = ttk.Button(r2, text="Restore save...", command=self.restore)
        self.restore_btn.pack(side="left", padx=6)
        ttk.Button(r2, text="Open backups folder", command=lambda: os.startfile(backups_dir())).pack(side="left", padx=6)

        # -- progress and log
        self.progress = ttk.Progressbar(root, mode="determinate")
        self.progress.pack(fill="x", padx=10, pady=(8, 2))
        self.log_box = tk.Text(root, height=12, state="disabled", wrap="word")
        self.log_box.pack(fill="both", expand=True, padx=10, pady=(2, 10))

        self.refresh_ports()
        if data_error:
            self.log("PROBLEM: " + data_error)
            for w in (self.install_btn, self.pak_btn):
                w.state(["disabled"])
        else:
            self.log("Ready. Installer files: %s" % self.data_dir)
        root.after(100, self.drain)

    # ---------------------------------------------------------------- plumbing
    def log(self, text):
        self.events.put(("log", text))

    def drain(self):
        try:
            while True:
                kind, val = self.events.get_nowait()
                if kind == "log":
                    self.log_box.configure(state="normal")
                    self.log_box.insert("end", val + "\n")
                    self.log_box.see("end")
                    self.log_box.configure(state="disabled")
                elif kind == "progress":
                    self.progress["value"] = val
                elif kind == "status":
                    idx, text = val
                    if idx in self.pak_status:
                        self.pak_status[idx].configure(text=text)
                elif kind == "done":
                    self.set_busy(False)
                elif kind == "call":
                    val()
        except queue.Empty:
            pass
        self.root.after(100, self.drain)

    def set_busy(self, busy):
        self.busy = busy
        for w in (self.install_btn, self.pak_btn, self.backup_btn, self.restore_btn):
            if busy:
                w.state(["disabled"])
            elif self.manifest or w in (self.backup_btn, self.restore_btn):
                w.state(["!disabled"])

    def run(self, job):
        """Runs job() on a worker thread; the window stays responsive."""
        if self.busy:
            return
        if not self.port.get():
            messagebox.showwarning(APP, "Pick the board's port first (plug it in, then Refresh).")
            return
        self.set_busy(True)

        def worker():
            try:
                job()
            except Exception as e:
                self.log("FAILED: %s" % e)
            finally:
                self.events.put(("progress", 0))
                self.events.put(("done", None))
        threading.Thread(target=worker, daemon=True).start()

    def device(self):
        return self.port.get().split("  ", 1)[0]

    def ask(self, question):
        """A yes/no from a worker thread, answered on the window's thread."""
        box, done = {}, threading.Event()

        def show():
            box["yes"] = messagebox.askyesno(APP, question)
            done.set()
        self.events.put(("call", show))
        done.wait()
        return box["yes"]

    def refresh_ports(self):
        ports = core.list_ports()
        self.port_box["values"] = [label for _, label in ports]
        if ports and self.port.get() not in self.port_box["values"]:
            self.port.set(ports[0][1])
        if not ports:
            self.port.set("")

    # ---------------------------------------------------------------- jobs
    def check_board(self):
        def job():
            b = core.Board(self.device(), self.log)
            try:
                bid = b.board_id()
                self.log("Connected%s." % (" to board " + bid if bid else ""))
                self.show_packs(b)
            finally:
                b.close()
        self.run(job)

    def show_packs(self, b):
        installed, sd = b.installed_packs()
        if installed is None:
            self.log("The board did not answer PACKS: restart it, then Check board again.")
            return
        if not sd:
            self.log("The board reports no microSD card.")
        current = 0
        for name, meta in self.paks.items():
            have = installed.get(meta["index"], "missing")
            ok = have == meta["crc32"].lower()
            current += ok
            text = "installed" if ok else ("not installed" if have == "missing" else "outdated - reload")
            self.events.put(("status", (meta["index"], text)))
        self.log("Sprite packs current on the card: %d of %d." % (current, len(self.paks)))

    def save_backup_text(self, text, path=None):
        path = path or os.path.join(backups_dir(), backup_name(text))
        with open(path, "w", encoding="ascii", newline="\n") as f:
            f.write(text)
        return path

    def install(self):
        password = self.password.get()
        if not password:
            messagebox.showwarning(APP, "Type the install password first.")
            return
        lock = self.manifest.get("lock")

        def job():
            if lock:
                self.log("Checking the password...")
                try:
                    key = core.check_password(password, lock)
                except core.WrongPassword:
                    self.log("Wrong password. Nothing was changed.")
                    return
                app = None
                for off, path in self.parts:
                    if off == core.APP_OFFSET:
                        app = core.unlock_app(key, lock, path)
                self.log("Password OK; the game (%d KB) is unlocked and verified." % (len(app) // 1024))
            else:
                app = open(dict(self.parts)[core.APP_OFFSET], "rb").read()
            # the backup, BEFORE anything is written
            self.log("Backing up the save on the board...")
            try:
                b = core.Board(self.device(), self.log)
                try:
                    text = b.capture_save()
                finally:
                    b.close()
                path = self.save_backup_text(text)
                self.log("Save backed up and verified: %s" % path)
            except Exception as e:
                self.log("Could not back up the save: %s" % e)
                if not self.ask("The save could not be backed up (%s).\n\n"
                                "A brand-new board has no save, which is fine. Install anyway?" % e):
                    self.log("Install cancelled.")
                    return
            self.log("Installing... do not unplug the board.")
            with tempfile.TemporaryDirectory() as tmp:
                core.flash(self.device(), self.parts, app, tmp, self.log)
            self.log("Installed. The board is restarting with v%s." % self.manifest.get("version", "?"))
        self.run(job)

    def load_packs(self):
        chosen = [n for n, v in self.pak_vars.items() if v.get()]
        if not chosen:
            messagebox.showinfo(APP, "Tick at least one region.")
            return

        def job():
            b = core.Board(self.device(), self.log)
            try:
                for name in chosen:
                    meta = self.paks[name]
                    self.log("Checking %s (%d MB)..." % (name.title(), meta["bytes"] // 1000000))

                    def prog(done, total, file, name=name):
                        self.events.put(("progress", 100 * done / max(1, total)))
                    b.load_pack(meta, prog)
                    self.events.put(("status", (meta["index"], "installed")))
                    self.log("%s is installed and verified." % name.title())
                self.show_packs(b)
            finally:
                b.close()
        self.run(job)

    def backup(self):
        def job():
            b = core.Board(self.device(), self.log)
            try:
                text = b.capture_save()
            finally:
                b.close()
            path = self.save_backup_text(text)
            self.log("Save backed up and verified: %s" % path)
        self.run(job)

    def restore(self):
        path = filedialog.askopenfilename(title="Pick a save backup", initialdir=backups_dir(),
                                          filetypes=[("TamaPoke save", "*.tpsave"), ("All files", "*.*")])
        if not path:
            return
        with open(path, encoding="ascii", errors="replace") as f:
            text = f.read()
        try:
            core.verify_backup(text)
        except ValueError as e:
            messagebox.showerror(APP, "That file is not a sound backup, so nothing was sent:\n\n%s" % e)
            return
        if not messagebox.askyesno(APP, "Replace the save on the board with\n%s?\n\n"
                                   "The board checks the whole file before replacing anything, then restarts."
                                   % os.path.basename(path)):
            return

        def job():
            b = core.Board(self.device(), self.log)
            try:
                b.send_backup(text)
            finally:
                b.close()
            self.log("Save restored. The board is restarting.")
        self.run(job)


def main():
    root = tk.Tk()
    App(root)
    root.mainloop()


if __name__ == "__main__":
    main()
