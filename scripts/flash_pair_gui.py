#!/usr/bin/env python3
"""Flash one Pocket Morse board at a time, then securely pair both boards."""
from __future__ import annotations

import queue
import importlib.util
import os
import shutil
import subprocess
import sys
import threading
import tkinter as tk
from pathlib import Path
from tkinter import messagebox, ttk

ROOT = Path(__file__).resolve().parent.parent


def application_directory() -> Path:
    executable = Path(sys.executable).resolve()
    if sys.platform == "darwin":
        for parent in executable.parents:
            if parent.suffix == ".app":
                return parent.parent
    return executable.parent


def project_root() -> Path:
    if getattr(sys, "frozen", False):
        return application_directory() / "project"
    return ROOT


def platformio_command() -> list[str] | None:
    executable = shutil.which("pio") or shutil.which("platformio")
    if executable:
        return [executable]
    if getattr(sys, "frozen", False):
        candidates = [
            Path.home() / ".local" / "bin" / "pio",
            Path.home() / ".local" / "bin" / "pio.exe",
            Path.home() / "Library" / "Application Support" / "pipx" / "venvs" / "platformio" / "bin" / "pio",
            Path("/opt/homebrew/bin/pio"),
            Path("/usr/local/bin/pio"),
        ]
        app_data = os.environ.get("APPDATA")
        if app_data:
            candidates.extend(Path(app_data).glob("Python/Python*/Scripts/pio.exe"))
        candidates.extend(Path.home().glob("AppData/Local/Programs/Python/Python*/Scripts/pio.exe"))
        executable = next((str(path) for path in candidates if path.is_file()), None)
        return [executable] if executable else None
    if importlib.util.find_spec("platformio") is not None:
        return [sys.executable, "-m", "platformio"]
    return None


def firmware_environment(role: str, flash_size: str, landscape: bool) -> str:
    if role not in {"A", "B"}:
        raise ValueError("Role must be A or B")
    if flash_size not in {"4 MB", "16 MB"}:
        raise ValueError("Flash size must be 4 MB or 16 MB")
    environment = f"device_{role.lower()}"
    if flash_size == "16 MB":
        environment += "_16mb"
    if landscape:
        environment += "_landscape"
    return environment


def serial_ports() -> list[tuple[str, str]]:
    try:
        from serial.tools import list_ports
    except ImportError as error:
        raise RuntimeError("Install USB serial support with: python3 -m pip install pyserial") from error
    return [(port.device, port.description or "Serial device") for port in list_ports.comports()]


class FlashPairApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("Pocket Morse · Flash & Pair")
        self.root.minsize(680, 600)
        self.messages: queue.Queue[tuple[str, object]] = queue.Queue()
        self.busy = False

        self.flash_port = tk.StringVar()
        self.role = tk.StringVar(value="A")
        self.role_hint = tk.StringVar(value="On-screen ID: A")
        self.role.trace_add("write", self._update_role_hint)
        self.flash_size = tk.StringVar(value="4 MB")
        self.landscape = tk.BooleanVar(value=False)
        self.pair_a = tk.StringVar()
        self.pair_b = tk.StringVar()
        self.status = tk.StringVar(value="Connect one board, choose its role, and flash it.")

        self._build_ui()
        self.refresh_ports()
        self.root.after(100, self._process_messages)

    def _build_ui(self) -> None:
        body = ttk.Frame(self.root, padding=20)
        body.pack(fill="both", expand=True)
        ttk.Label(body, text="POCKET MORSE", font=("TkDefaultFont", 18, "bold")).pack(anchor="w")
        ttk.Label(body, text="Flash each board separately. Pair the set once both are ready.").pack(anchor="w", pady=(2, 18))

        flash = ttk.LabelFrame(body, text="1 · Flash one board", padding=12)
        flash.pack(fill="x")
        ttk.Label(flash, text="Connected device").grid(row=0, column=0, sticky="w")
        self.flash_combo = ttk.Combobox(flash, textvariable=self.flash_port, state="readonly", width=38)
        self.flash_combo.grid(row=1, column=0, sticky="ew", pady=(4, 10))
        ttk.Button(flash, text="Refresh", command=self.refresh_ports).grid(row=1, column=1, padx=(8, 0), pady=(4, 10))

        options = ttk.Frame(flash)
        options.grid(row=2, column=0, columnspan=2, sticky="w")
        ttk.Label(options, text="Firmware role").pack(side="left")
        ttk.Combobox(options, textvariable=self.role, values=("A", "B"), state="readonly", width=5).pack(side="left", padx=(8, 8))
        ttk.Label(options, textvariable=self.role_hint).pack(side="left", padx=(0, 18))
        ttk.Label(options, text="Board flash").pack(side="left")
        ttk.Combobox(options, textvariable=self.flash_size, values=("4 MB", "16 MB"), state="readonly", width=8).pack(side="left", padx=(8, 18))
        ttk.Checkbutton(options, text="Landscape display", variable=self.landscape).pack(side="left")

        self.upload_button = ttk.Button(flash, text="Build & upload firmware", command=self.upload)
        self.upload_button.grid(row=3, column=0, sticky="w", pady=(12, 0))
        ttk.Label(flash, text="The selected role appears before LINK/UNLINK on the device. 4 MB is the safe default; select 16 MB only after reading the chip ID.", wraplength=560).grid(row=4, column=0, columnspan=2, sticky="w", pady=(9, 0))
        flash.columnconfigure(0, weight=1)

        pairing = ttk.LabelFrame(body, text="2 · Pair the two boards", padding=12)
        pairing.pack(fill="x", pady=(14, 0))
        ttk.Label(pairing, text="Connect both freshly flashed boards for this step.", wraplength=560).grid(row=0, column=0, columnspan=3, sticky="w")
        ttk.Label(pairing, text="Device A").grid(row=1, column=0, sticky="w", pady=(9, 0))
        ttk.Label(pairing, text="Device B").grid(row=1, column=1, sticky="w", padx=(10, 0), pady=(9, 0))
        self.pair_a_combo = ttk.Combobox(pairing, textvariable=self.pair_a, state="readonly", width=27)
        self.pair_a_combo.grid(row=2, column=0, sticky="ew", pady=(4, 0))
        self.pair_a_combo.bind("<<ComboboxSelected>>", self._pair_a_changed)
        self.pair_b_combo = ttk.Combobox(pairing, textvariable=self.pair_b, state="readonly", width=27)
        self.pair_b_combo.grid(row=2, column=1, sticky="ew", padx=(10, 0), pady=(4, 0))
        ttk.Button(pairing, text="Refresh", command=self.refresh_ports).grid(row=2, column=2, padx=(8, 0), pady=(4, 0))
        self.pair_button = ttk.Button(pairing, text="Generate key & pair A + B", command=self.pair)
        self.pair_button.grid(row=3, column=0, sticky="w", pady=(12, 0))
        ttk.Label(pairing, text="The random pair key is sent over USB and is never put in the firmware image or saved to a file.", wraplength=560).grid(row=4, column=0, columnspan=3, sticky="w", pady=(9, 0))
        pairing.columnconfigure(0, weight=1)
        pairing.columnconfigure(1, weight=1)

        ttk.Label(body, textvariable=self.status, wraplength=620).pack(anchor="w", pady=(12, 6))
        self.output = tk.Text(body, height=11, wrap="word", state="disabled")
        self.output.pack(fill="both", expand=True)
        ttk.Label(body, text="Pairing refuses configured boards and will not erase a used or corrupt inbox.", wraplength=620).pack(anchor="w", pady=(8, 0))

    def _update_role_hint(self, *_args: object) -> None:
        self.role_hint.set(f"On-screen ID: {self.role.get()}")

    def _append(self, text: str) -> None:
        self.output.configure(state="normal")
        self.output.insert("end", text)
        self.output.see("end")
        self.output.configure(state="disabled")

    def refresh_ports(self) -> None:
        dependency_error = ""
        try:
            ports = serial_ports()
        except RuntimeError as error:
            dependency_error = str(error)
            ports = []
        old = (self.flash_port.get(), self.pair_a.get(), self.pair_b.get())
        labels = [f"{device} — {description}" for device, description in ports]
        self.flash_combo.configure(values=labels)
        self.flash_port.set(self._choose_port(old[0], labels))
        self.pair_a_combo.configure(values=labels)
        self.pair_b_combo.configure(values=labels)
        self.pair_a.set(self._choose_port(old[1], labels))
        self.pair_b.set(self._choose_port(old[2], labels, exclude={self._selected_port(self.pair_a.get())}))
        self.pair_button.configure(state="normal" if len(labels) > 1 else "disabled")
        if dependency_error:
            self.status.set(dependency_error)
        else:
            self.status.set(f"{len(ports)} serial device(s) found." if ports else "No serial devices found. Connect a board and refresh.")

    def _pair_a_changed(self, _event: tk.Event[tk.Misc] | None = None) -> None:
        labels = list(self.pair_a_combo.cget("values"))
        selected_a = self._selected_port(self.pair_a.get())
        self.pair_b_combo.configure(values=[label for label in labels if self._selected_port(label) != selected_a])
        if self._selected_port(self.pair_b.get()) == selected_a:
            self.pair_b.set(self._choose_port("", labels, exclude={selected_a}))

    @classmethod
    def _choose_port(cls, previous: str, labels: list[str], exclude: set[str] | None = None) -> str:
        excluded = exclude or set()
        port = cls._selected_port(previous)
        if port and port not in excluded:
            match = next((label for label in labels if cls._selected_port(label) == port), "")
            if match:
                return match
        return next((label for label in labels if cls._selected_port(label) not in excluded), "")

    @staticmethod
    def _selected_port(value: str) -> str:
        return value.split(" — ", 1)[0] if value else ""

    def upload(self) -> None:
        port = self._selected_port(self.flash_port.get())
        if not port:
            messagebox.showerror("Choose a device", "Connect a board and select its serial port first.")
            return
        pio = platformio_command()
        if not pio:
            messagebox.showerror(
                "PlatformIO not found",
                "Install PlatformIO Core (on macOS, `pipx install platformio`) and reopen this app. "
                "The uploader checks PATH and the usual pipx/Homebrew locations.",
            )
            return
        if self.flash_size.get() == "16 MB" and not messagebox.askyesno(
            "Confirm 16 MB flash",
            f"Use the 16 MB partition layout for {port} only if its chip ID reports 16 MB.\n\n"
            "A 16 MB partition table must never be used on a 4 MB board. Continue?",
        ):
            return
        environment = firmware_environment(self.role.get(), self.flash_size.get(), self.landscape.get())
        command = [*pio, "run", "-e", environment, "-t", "upload", "--upload-port", port]
        self._run(command, f"Building {environment} for {port}…")

    def pair(self) -> None:
        port_a = self._selected_port(self.pair_a.get())
        port_b = self._selected_port(self.pair_b.get())
        if not port_a or not port_b or port_a == port_b:
            messagebox.showerror("Select two devices", "Connect both boards and choose different ports for A and B.")
            return
        if not messagebox.askyesno(
            "Pair these devices?",
            "This generates a new key for these two unpaired boards. Existing pair data is never overwritten.\n\n"
            "Both serial ports must be free of monitors. Continue?",
        ):
            return
        if getattr(sys, "frozen", False):
            helper = application_directory() / ("provision_pair.exe" if sys.platform == "win32" else "provision_pair")
            if not helper.is_file():
                messagebox.showerror("Pairing helper missing", "Extract the complete Pocket Morse uploader folder, including provision_pair.")
                return
            command = [str(helper), "--a", port_a, "--b", port_b]
        else:
            command = [sys.executable, str(ROOT / "scripts" / "provision_pair.py"), "--a", port_a, "--b", port_b]
        self._run(command, "Checking both board roles, initializing blank inboxes, then pairing…", cwd=project_root())

    def _run(self, command: list[str], status: str, cwd: Path | None = None) -> None:
        if self.busy:
            return
        self.busy = True
        self.status.set(status)
        self.upload_button.configure(state="disabled")
        self.pair_button.configure(state="disabled")
        self._append("\n$ " + " ".join(command) + "\n")
        threading.Thread(target=self._worker, args=(command, cwd or project_root()), daemon=True).start()

    def _worker(self, command: list[str], cwd: Path) -> None:
        try:
            process = subprocess.Popen(
                command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, bufsize=1,
            )
            assert process.stdout is not None
            for line in process.stdout:
                self.messages.put(("line", line))
            self.messages.put(("done", process.wait()))
        except OSError as error:
            self.messages.put(("error", str(error)))

    def _process_messages(self) -> None:
        try:
            while True:
                kind, value = self.messages.get_nowait()
                if kind == "line":
                    self._append(str(value))
                else:
                    self.busy = False
                    self.upload_button.configure(state="normal")
                    self.pair_button.configure(state="normal")
                    if kind == "error":
                        self.status.set("Could not start the operation.")
                        self._append(f"ERROR: {value}\n")
                    else:
                        code = int(value)
                        self.status.set("Operation completed." if code == 0 else f"Operation failed (exit code {code}).")
                        self._append(f"Exit code: {code}\n")
                        self.refresh_ports()
        except queue.Empty:
            pass
        self.root.after(100, self._process_messages)


def main() -> None:
    root = tk.Tk()
    FlashPairApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
