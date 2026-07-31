#!/usr/bin/env python3
"""
PTZ Camera Control – UDP companion app  (Python 3 / tkinter)
=============================================================
Sends control packets to the camera controller on UDP port 5355.
Requires only the Python standard library (no pip installs).

Keyboard shortcuts (identical to the original web interface):
  1-6          Select zoom speed 1-6
  f            Focus mode
  w            White balance (K) mode
  a            Exposure F mode
  s            Exposure S (shutter) mode
  g  / e       Exposure Gain mode
  p            Pan speed – Slow
  [            Pan speed – Medium
  ]  / .       Pan speed – Fast
  Arrow keys   Pan (L/R) and Tilt (U/D) at the current speed  [keep-alive]
  z            Zoom In at the current zoom speed               [keep-alive]
  x            Zoom Out at the current zoom speed              [keep-alive]

On-screen D-pad buttons send DIR commands, which are context-sensitive:
  zoom/focus/WB/exposure when the matching mode is selected, or
  pan/tilt when a Pan Speed mode is selected.
"""

import tkinter as tk
import socket
import threading
import queue
import time

# ── Protocol constants ────────────────────────────────────────────────────────
CONTROL_PORT       = 5355
STATUS_INTERVAL_MS = 5000   # How often to request a status update from the device
KEEPALIVE_MS       = 100    # Keep-alive repeat rate while a key is held
# Must be strictly less than firmware KEY_UDP_TIMEOUT_MS (300 ms)

# ── Camera command IDs (must match firmware CameraCommands enum) ──────────────
ZOOM_1, ZOOM_2, ZOOM_3 = 1, 2, 3
ZOOM_4, ZOOM_5, ZOOM_6 = 4, 5, 6
ZOOM_7          = 7      # exists in firmware but not normally in the UI
FOCUS           = 8
WB_K            = 9
EXP_F           = 10
EXP_S           = 11
EXP_GAIN        = 12
PAN_TILT_FAST   = 13
PAN_TILT_MEDIUM = 14
PAN_TILT_SLOW   = 15

# ── Mode-button layout ────────────────────────────────────────────────────────
ZOOM_MODES  = [(1,"Z1"),(2,"Z2"),(3,"Z3"),(4,"Z4"),(5,"Z5"),(6,"Z6")]
PARAM_MODES = [(FOCUS,"Focus"),(WB_K,"WB"),(EXP_F,"Exp F"),(EXP_S,"Exp S"),(EXP_GAIN,"Gain")]
SPEED_MODES = [(PAN_TILT_FAST,"Fast"),(PAN_TILT_MEDIUM,"Med"),(PAN_TILT_SLOW,"Slow")]

# ── Hotkey → camera command (keysym strings used by tkinter) ─────────────────
HOTKEY_CMD: dict = {
    "1": ZOOM_1, "2": ZOOM_2, "3": ZOOM_3,
    "4": ZOOM_4, "5": ZOOM_5, "6": ZOOM_6,
    "f": FOCUS,
    "w": WB_K,
    "a": EXP_F,
    "s": EXP_S,
    "g": EXP_GAIN,  "e": EXP_GAIN,
    "p": PAN_TILT_SLOW,
    "bracketleft":  PAN_TILT_MEDIUM,
    "bracketright": PAN_TILT_FAST,
    "period":       PAN_TILT_FAST,
}

# ── Colour palette (Catppuccin-Mocha inspired) ────────────────────────────────
C = {
    "bg":      "#1e1e2e",
    "surface": "#313244",
    "overlay": "#45475a",
    "text":    "#cdd6f4",
    "subtext": "#bac2de",
    "muted":   "#6c7086",
    "blue":    "#89b4fa",
    "green":   "#a6e3a1",
    "red":     "#f38ba8",
    "peach":   "#fab387",
    "yellow":  "#f9e2af",
}


class CameraControlApp:
    """Main application window for UDP-based PTZ camera control."""

    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("PTZ Camera Control")
        self.root.resizable(False, False)
        self.root.configure(bg=C["bg"])

        # ── UDP socket (send + receive on the same socket) ────────────────────
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(0.05)
        self.target: tuple = None  # (ip, port) or None when disconnected

        # ── UI state variables ────────────────────────────────────────────────
        self.ip_var       = tk.StringVar(value="camera.local")
        self.port_var     = tk.StringVar(value=str(CONTROL_PORT))
        self.conn_label   = tk.StringVar(value="Not connected")
        self.hostname_var = tk.StringVar(value="—")
        self.wb_k_var     = tk.StringVar(value="—")
        self.exp_f_var    = tk.StringVar(value="—")
        self.exp_s_var    = tk.StringVar(value="—")
        self.exp_g_var    = tk.StringVar(value="—")

        # Currently highlighted mode button
        self.current_cmd: int = ZOOM_1
        self.cmd_buttons: dict = {}  # cmd_id → tk.Button

        # Key / button hold-state for keep-alive mechanism
        self.key_held: dict = {}          # action_name → bool
        self.release_timers: dict = {}    # action_name → after() id
        self.keepalive_jobs: dict = {}    # action_name → after() id

        # Background receive thread → main-thread queue
        self.recv_q: queue.SimpleQueue = queue.SimpleQueue()
        threading.Thread(target=self._recv_loop, daemon=True).start()

        self._build_ui()
        self._bind_keys()
        self._poll_recv_queue()
        self._schedule_status_poll()

    # =========================================================================
    # Networking
    # =========================================================================

    def _send(self, msg: str) -> None:
        """Fire-and-forget UDP send. Silent if not connected."""
        if not self.target:
            return
        try:
            self.sock.sendto(msg.encode(), self.target)
        except OSError:
            pass

    def _recv_loop(self) -> None:
        """Background thread: receive UDP replies and push to the queue."""
        while True:
            try:
                data, _ = self.sock.recvfrom(512)
                self.recv_q.put(data.decode(errors="replace").strip())
            except socket.timeout:
                pass
            except OSError:
                time.sleep(0.1)

    def _poll_recv_queue(self) -> None:
        """Drain the receive queue from the Tk event loop (safe for UI updates)."""
        while not self.recv_q.empty():
            try:
                self._handle_incoming(self.recv_q.get_nowait())
            except Exception:
                pass
        self.root.after(100, self._poll_recv_queue)

    def _handle_incoming(self, msg: str) -> None:
        """Parse a STATUS reply from the device and update the UI."""
        if not msg.startswith("STATUS "):
            return
        kv: dict = {}
        for part in msg[7:].split():
            if "=" in part:
                k, v = part.split("=", 1)
                kv[k] = v
        if "wb_k"     in kv: self.wb_k_var.set(kv["wb_k"])
        if "exp_f"    in kv: self.exp_f_var.set(kv["exp_f"])
        if "exp_s"    in kv: self.exp_s_var.set(kv["exp_s"])
        if "exp_g"    in kv: self.exp_g_var.set(kv["exp_g"])
        if "hostname" in kv: self.hostname_var.set(kv["hostname"])

    def _schedule_status_poll(self) -> None:
        """Periodically request a STATUS update from the device."""
        if self.target:
            self._send("STATUS")
        self.root.after(STATUS_INTERVAL_MS, self._schedule_status_poll)

    def _connect(self) -> None:
        host = self.ip_var.get().strip()
        if not host:
            return
        try:
            ip = socket.gethostbyname(host)
        except socket.gaierror as exc:
            self.conn_label.set(f"DNS error: {exc}")
            return
        port = int(self.port_var.get())
        self.target = (ip, port)
        self.conn_label.set(f"● {ip}:{port}")
        self._send("STATUS")

    def _disconnect(self) -> None:
        self._do_stop_all()
        self.target = None
        self.conn_label.set("Disconnected")

    # =========================================================================
    # Keep-alive machinery
    # =========================================================================

    def _start_action(self, action: str, packet: str) -> None:
        """
        Send `packet` immediately and then every KEEPALIVE_MS while the
        action remains active.  Replaces any existing job for this action.
        """
        self._cancel_keepalive(action)
        self.key_held[action] = True
        self._send(packet)

        def repeat() -> None:
            if self.key_held.get(action, False) and self.target:
                self._send(packet)
                self.keepalive_jobs[action] = self.root.after(KEEPALIVE_MS, repeat)

        self.keepalive_jobs[action] = self.root.after(KEEPALIVE_MS, repeat)

    def _cancel_keepalive(self, action: str) -> None:
        """Stop the keep-alive timer for an action without sending an off packet."""
        job = self.keepalive_jobs.pop(action, None)
        if job:
            self.root.after_cancel(job)
        self.key_held[action] = False

    def _stop_action(self, action: str, off_packet: str) -> None:
        """Cancel the keep-alive and send the off packet 3× for reliability."""
        self._cancel_keepalive(action)
        for _ in range(3):
            self._send(off_packet)

    # =========================================================================
    # Camera mode selection
    # =========================================================================

    def _select_cmd(self, cmd_id: int) -> None:
        self.current_cmd = cmd_id
        self._send(f"CMD {cmd_id}")
        for cid, btn in self.cmd_buttons.items():
            if cid == cmd_id:
                btn.configure(bg=C["blue"], fg=C["bg"], relief="sunken")
            else:
                btn.configure(bg=C["surface"], fg=C["text"], relief="raised")

    # =========================================================================
    # Stop-all
    # =========================================================================

    def _do_stop_all(self) -> None:
        """Cancel all keep-alives and tell the device to stop everything."""
        for action in list(self.key_held):
            self._cancel_keepalive(action)
        for action, tid in list(self.release_timers.items()):
            self.root.after_cancel(tid)
        self.release_timers.clear()
        self._send("STOP")

    # =========================================================================
    # Key event routing
    # =========================================================================

    def _bind_keys(self) -> None:
        # ── Hold keys: arrow + zoom ───────────────────────────────────────────
        held_map = {
            "Up":    ("kup",      "KEY up on",      "KEY up off"),
            "Down":  ("kdown",    "KEY down on",    "KEY down off"),
            "Left":  ("kleft",    "KEY left on",    "KEY left off"),
            "Right": ("kright",   "KEY right on",   "KEY right off"),
            "z":     ("kzoomin",  "KEY zoomin on",  "KEY zoomin off"),
            "x":     ("kzoomout", "KEY zoomout on", "KEY zoomout off"),
        }
        for sym, (action, on_pkt, off_pkt) in held_map.items():
            self.root.bind(f"<KeyPress-{sym}>",
                           lambda e, a=action, p=on_pkt:  self._on_held_press(a, p))
            self.root.bind(f"<KeyRelease-{sym}>",
                           lambda e, a=action, p=off_pkt: self._on_held_release(a, p))

        # ── Instant keys: mode selectors ──────────────────────────────────────
        for sym, cmd_id in HOTKEY_CMD.items():
            self.root.bind(f"<KeyPress-{sym}>",
                           lambda e, c=cmd_id: self._select_cmd(c))

        # ── Safety: stop everything when the window loses focus ───────────────
        self.root.bind("<FocusOut>", lambda e: self._do_stop_all())

    def _on_held_press(self, action: str, packet: str) -> None:
        """
        Handle key press for a held action.
        Cancels any pending release timer first to absorb keyboard auto-repeat
        (which generates a rapid KeyRelease → KeyPress pair).
        """
        pending = self.release_timers.pop(action, None)
        if pending:
            self.root.after_cancel(pending)
        if not self.key_held.get(action, False):
            self._start_action(action, packet)

    def _on_held_release(self, action: str, off_packet: str) -> None:
        """
        Handle key release with a 30 ms confirmation delay to filter
        auto-repeat (system emits Release + Press in ≈1-5 ms for held keys).
        """
        pending = self.release_timers.pop(action, None)
        if pending:
            self.root.after_cancel(pending)
        self.release_timers[action] = self.root.after(
            30,
            lambda a=action, p=off_packet: self._confirm_key_release(a, p),
        )

    def _confirm_key_release(self, action: str, off_packet: str) -> None:
        self.release_timers.pop(action, None)
        self._stop_action(action, off_packet)

    # =========================================================================
    # Button helpers
    # =========================================================================

    def _bind_hold_btn(self, btn: tk.Button, on_pkt: str, off_pkt: str) -> None:
        """
        Bind a button so that pressing sends on_pkt once and releasing
        sends off_pkt three times for reliability.
        Mouse-leave also triggers release (safety net for fast drags).
        """
        def _press(_e=None):  self._send(on_pkt)
        def _release(_e=None):
            for _ in range(3):
                self._send(off_pkt)
        btn.bind("<ButtonPress-1>",   _press)
        btn.bind("<ButtonRelease-1>", _release)
        btn.bind("<Leave>",           _release)

    # =========================================================================
    # UI construction
    # =========================================================================

    def _build_ui(self) -> None:
        # ── Header ────────────────────────────────────────────────────────────
        hdr = tk.Frame(self.root, bg=C["bg"])
        hdr.pack(fill="x", padx=8, pady=(8, 2))
        tk.Label(hdr, text="PTZ Camera Control",
                 bg=C["bg"], fg=C["blue"],
                 font=("Helvetica", 14, "bold")).pack(side="left")
        tk.Label(hdr, textvariable=self.hostname_var,
                 bg=C["bg"], fg=C["subtext"],
                 font=("Helvetica", 11)).pack(side="right")

        # ── Connection bar ────────────────────────────────────────────────────
        cbar = tk.Frame(self.root, bg=C["surface"], padx=6, pady=5)
        cbar.pack(fill="x", padx=8, pady=(0, 6))

        for lbl_txt, var, width in [("Host:", self.ip_var, 22), ("Port:", self.port_var, 6)]:
            tk.Label(cbar, text=lbl_txt, bg=C["surface"], fg=C["text"],
                     font=("Helvetica", 10)).pack(side="left")
            tk.Entry(cbar, textvariable=var, width=width,
                     bg=C["overlay"], fg=C["text"], insertbackground=C["text"],
                     relief="flat", font=("Helvetica", 10)).pack(side="left", padx=(2, 5))

        tk.Button(cbar, text="Connect", command=self._connect,
                  bg=C["green"], fg=C["bg"], relief="flat",
                  font=("Helvetica", 10, "bold"), padx=6).pack(side="left", padx=(0, 3))
        tk.Button(cbar, text="✕", command=self._disconnect,
                  bg=C["red"], fg=C["bg"], relief="flat",
                  font=("Helvetica", 10, "bold"), padx=5).pack(side="left")
        tk.Label(cbar, textvariable=self.conn_label,
                 bg=C["surface"], fg=C["blue"],
                 font=("Helvetica", 9)).pack(side="right", padx=4)

        # ── Camera mode section ───────────────────────────────────────────────
        mode_frame = tk.LabelFrame(self.root, text=" Camera Mode ",
                                   bg=C["bg"], fg=C["blue"],
                                   font=("Helvetica", 10, "bold"), padx=6, pady=4)
        mode_frame.pack(fill="x", padx=8, pady=(0, 6))

        for row_label, modes in [
            ("Zoom",  ZOOM_MODES),
            ("Param", PARAM_MODES),
            ("Speed", SPEED_MODES),
        ]:
            row = tk.Frame(mode_frame, bg=C["bg"])
            row.pack(fill="x", pady=1)
            tk.Label(row, text=row_label, bg=C["bg"], fg=C["muted"],
                     font=("Helvetica", 9), width=5, anchor="e").pack(side="left", padx=(0, 4))
            for cmd_id, label in modes:
                btn = tk.Button(row, text=label, width=5,
                                bg=C["surface"], fg=C["text"], relief="raised",
                                font=("Helvetica", 9), activebackground=C["blue"],
                                command=lambda c=cmd_id: self._select_cmd(c))
                btn.pack(side="left", padx=1)
                self.cmd_buttons[cmd_id] = btn

        # Highlight default selection
        self._select_cmd(ZOOM_1)

        # ── Lower area ────────────────────────────────────────────────────────
        lower = tk.Frame(self.root, bg=C["bg"])
        lower.pack(fill="both", expand=True, padx=8, pady=(0, 6))

        # ── D-pad (left side) ─────────────────────────────────────────────────
        dpad = tk.LabelFrame(lower, text=" Direction ",
                             bg=C["bg"], fg=C["blue"],
                             font=("Helvetica", 10, "bold"), padx=8, pady=6)
        dpad.pack(side="left", fill="y", padx=(0, 6))

        def _arrow_btn(text: str, **grid_kw) -> tk.Button:
            b = tk.Button(dpad, text=text, width=4, height=2,
                          bg=C["surface"], fg=C["text"], relief="raised",
                          font=("Helvetica", 16), activebackground=C["blue"])
            b.grid(**grid_kw, padx=2, pady=2)
            return b

        btn_up    = _arrow_btn("▲", row=0, column=1)
        btn_left  = _arrow_btn("◄", row=1, column=0)
        btn_right = _arrow_btn("►", row=1, column=2)
        btn_down  = _arrow_btn("▼", row=2, column=1)

        btn_stop = tk.Button(dpad, text="■", width=4, height=2,
                             bg=C["red"], fg=C["bg"], relief="raised",
                             font=("Helvetica", 16, "bold"),
                             command=self._do_stop_all,
                             activebackground="#ff6666")
        btn_stop.grid(row=1, column=1, padx=2, pady=2)

        self._bind_hold_btn(btn_up,    "DIR up on",    "DIR up off")
        self._bind_hold_btn(btn_down,  "DIR down on",  "DIR down off")
        self._bind_hold_btn(btn_left,  "DIR left on",  "DIR left off")
        self._bind_hold_btn(btn_right, "DIR right on", "DIR right off")

        # Roll row
        tk.Label(dpad, text="Roll", bg=C["bg"], fg=C["muted"],
                 font=("Helvetica", 9)).grid(row=3, column=0, columnspan=3, pady=(8, 0))
        btn_ccw = tk.Button(dpad, text="↺ CCW", width=6, height=2,
                            bg=C["surface"], fg=C["text"], relief="raised",
                            font=("Helvetica", 11), activebackground=C["blue"])
        btn_ccw.grid(row=4, column=0, columnspan=2, padx=2, pady=2, sticky="ew")
        btn_cw  = tk.Button(dpad, text="↻ CW", width=6, height=2,
                            bg=C["surface"], fg=C["text"], relief="raised",
                            font=("Helvetica", 11), activebackground=C["blue"])
        btn_cw.grid(row=4, column=2, columnspan=1, padx=2, pady=2, sticky="ew")

        self._bind_hold_btn(btn_ccw, "ROLL ccw on", "ROLL ccw off")
        self._bind_hold_btn(btn_cw,  "ROLL cw on",  "ROLL cw off")

        # ── Right panel: status + hotkeys + action buttons ────────────────────
        right = tk.Frame(lower, bg=C["bg"])
        right.pack(side="left", fill="both", expand=True)

        # Status panel
        stat = tk.LabelFrame(right, text=" Camera Status ",
                             bg=C["bg"], fg=C["blue"],
                             font=("Helvetica", 10, "bold"), padx=8, pady=6)
        stat.pack(fill="x", pady=(0, 6))

        status_items = [
            ("WB K:",  self.wb_k_var),
            ("Exp F:", self.exp_f_var),
            ("Exp S:", self.exp_s_var),
            ("Gain:",  self.exp_g_var),
        ]
        for i, (lbl, var) in enumerate(status_items):
            r, c = divmod(i, 2)
            tk.Label(stat, text=lbl, bg=C["bg"], fg=C["subtext"],
                     font=("Helvetica", 10), anchor="e", width=6
                     ).grid(row=r, column=c * 2, sticky="e", padx=(0, 2))
            tk.Label(stat, textvariable=var, bg=C["bg"], fg=C["yellow"],
                     font=("Helvetica", 10, "bold"), anchor="w", width=8
                     ).grid(row=r, column=c * 2 + 1, sticky="w")

        # Hotkey reference panel
        hint = tk.LabelFrame(right, text=" Hotkeys ",
                             bg=C["bg"], fg=C["blue"],
                             font=("Helvetica", 10, "bold"), padx=6, pady=4)
        hint.pack(fill="x", pady=(0, 6))
        for line in [
            "1-6 : Zoom speed        f : Focus",
            "w : WB    a : Exp F    s : Exp S",
            "g : Gain    ] : Fast    [ : Med    p : Slow",
            "Arrow keys : Pan / Tilt    (keep-alive)",
            "Z : Zoom In    X : Zoom Out    (keep-alive)",
        ]:
            tk.Label(hint, text=line, bg=C["bg"], fg=C["muted"],
                     font=("Courier", 8), anchor="w").pack(fill="x")

        # Action buttons
        actions = tk.Frame(right, bg=C["bg"])
        actions.pack(fill="x")
        for text, cmd, colour, fg in [
            ("Init Camera", self._init_camera,   C["peach"],   C["bg"]),
            ("Stop All",    self._do_stop_all,   C["red"],     C["bg"]),
            ("Reset",       self._reset_device,  C["overlay"], C["text"]),
        ]:
            tk.Button(actions, text=text, command=cmd,
                      bg=colour, fg=fg, relief="flat",
                      font=("Helvetica", 10, "bold"), padx=4, pady=7
                      ).pack(side="left", fill="x", expand=True, padx=2)

    # =========================================================================
    # Action handlers
    # =========================================================================

    def _init_camera(self) -> None:
        """Stop all motion then trigger the camera initialisation sequence."""
        self._do_stop_all()
        self._send("INIT")

    def _reset_device(self) -> None:
        """Stop all motion then reboot the MCU."""
        self._do_stop_all()
        self._send("RESET")


# ── Entry point ───────────────────────────────────────────────────────────────
if __name__ == "__main__":
    root = tk.Tk()
    app = CameraControlApp(root)
    try:
        root.mainloop()
    finally:
        # Best-effort cleanup: stop motors and close socket on exit
        try:
            app._do_stop_all()
        except Exception:
            pass
        app.sock.close()
