#!/usr/bin/env python3
"""
PTZ Camera Control – UDP companion app  (Python 3 / tkinter)
=============================================================
Light-theme UI designed to match the web-based interface.
Sends control packets to the camera controller on UDP port 5355.
Requires only the Python standard library (no pip installs).

Keyboard shortcuts (identical to the original web interface):
  1-6          Select zoom speed 1-6
  f            Focus mode
  w            White balance (K) mode
  a            Exposure F (Aperture) mode
  s            Exposure S (Shutter) mode
  g  / e       Exposure Gain mode
  p            Pan speed – Slow
  [            Pan speed – Medium
  ]  / .       Pan speed – Fast
  Arrow keys   Pan (L/R) and Tilt (U/D) at the current speed  [keep-alive]
  z            Zoom In at the current zoom speed               [keep-alive]
  x            Zoom Out at the current zoom speed              [keep-alive]
"""

import tkinter as tk
import socket
import threading
import queue
import time

# ── Protocol constants ────────────────────────────────────────────────────────
CONTROL_PORT       = 5355
STATUS_INTERVAL_MS = 5000
KEEPALIVE_MS       = 100

# ── Camera command IDs (must match firmware CameraCommands enum) ──────────────
ZOOM_1, ZOOM_2, ZOOM_3 = 1, 2, 3
ZOOM_4, ZOOM_5, ZOOM_6 = 4, 5, 6
FOCUS           = 8
WB_K            = 9
EXP_F           = 10
EXP_S           = 11
EXP_GAIN        = 12
PAN_TILT_FAST   = 13
PAN_TILT_MEDIUM = 14
PAN_TILT_SLOW   = 15

# ── Hotkey → camera command ───────────────────────────────────────────────────
HOTKEY_CMD: dict = {
    "1": ZOOM_1, "2": ZOOM_2, "3": ZOOM_3,
    "4": ZOOM_4, "5": ZOOM_5, "6": ZOOM_6,
    "f": FOCUS,  "w": WB_K,  "a": EXP_F,
    "s": EXP_S,  "g": EXP_GAIN, "e": EXP_GAIN,
    "p": PAN_TILT_SLOW,
    "bracketleft":  PAN_TILT_MEDIUM,
    "bracketright": PAN_TILT_FAST,
    "period":       PAN_TILT_FAST,
}

# ── Value conversion (mirrors JS functions in index.js) ───────────────────────
_EXP_F = [
    "F1.8","F2.0","F2.2","F2.4","F2.6","F2.8","F3.2","F3.4","F3.7","F4.0",
    "F4.0②","F4.0③","F4.0④","ND½①","ND½②","ND½③","ND½④",
    "ND¼①","ND¼②","ND¼③","ND¼④","F4.0 ND⅛","F4.4 ND⅛",
    "F4.8 ND⅛","F5.2 ND⅛","F5.6 ND⅛","F6.2 ND⅛","F6.7 ND⅛","F7.3 ND⅛","F8.0 ND⅛",
]
_EXP_S = ["1/6","1/12","1/25","1/50","1/120","1/250","1/500","1/1000","1/2000"]

def _wb_str(idx)   -> str: return f"{2000 + int(idx) * 100} K"
def _expf_str(idx) -> str:
    try:    return _EXP_F[int(idx)]
    except: return str(idx)
def _exps_str(idx) -> str:
    try:    return _EXP_S[int(idx)]
    except: return str(idx)

# ── Colour palette (light neumorphic theme matching the web UI) ───────────────
C = {
    "bg":       "#edf2fb",   # main window background
    "card":     "#f2f6ff",   # card / status-box background
    "surface":  "#dde5f4",   # seg-track, d-pad circle, roll bg
    "border":   "#c4d0e8",   # subtle outlines
    "seg_sel":  "#8fa8d9",   # selected segment fill
    "seg_txt":  "#ffffff",   # text on a selected segment
    "icon_bg":  "#e8eef8",   # icon-button circle resting fill
    "icon_sel": "#7b9ed9",   # icon-button circle selected fill
    "icon_brd": "#c4d0e8",   # icon-button circle border
    "text":     "#3d4c6e",   # primary text
    "subtext":  "#6e7fa8",   # secondary / label text
    "muted":    "#9aaabf",   # hint text
    "red":      "#c97070",   # stop / danger
    "orange":   "#c98050",   # init / warm
    "neutral":  "#8090b8",   # reset
    "white":    "#ffffff",
}

FF = "Helvetica Neue"   # closest macOS match to the web UI's Poppins font


# ─────────────────────────────────────────────────────────────────────────────
class CameraControlApp:
    """Main application window for UDP-based PTZ camera control."""

    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("PTZ Camera Control")
        self.root.resizable(True, True)
        self.root.minsize(920, 580)
        self.root.configure(bg=C["bg"])

        # ── UDP socket ────────────────────────────────────────────────────────
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(0.05)
        self.target: tuple = None

        # ── UI state ──────────────────────────────────────────────────────────
        self.ip_var       = tk.StringVar(value="camera.local")
        self.port_var     = tk.StringVar(value=str(CONTROL_PORT))
        self.conn_label   = tk.StringVar(value="Not connected")
        self.hostname_var = tk.StringVar(value="Unnamed Camera")
        self.wb_k_var     = tk.StringVar(value="—")
        self.exp_f_var    = tk.StringVar(value="—")
        self.exp_s_var    = tk.StringVar(value="—")
        self.exp_g_var    = tk.StringVar(value="—")

        self.current_cmd: int = ZOOM_1
        self.cmd_buttons:  dict = {}   # cmd_id → tk.Button  (segmented pill rows)
        self.cmd_canvases: dict = {}   # cmd_id → (Canvas, oval_id, text_id)

        # Key / button hold-state
        self.key_held:       dict = {}
        self.release_timers: dict = {}
        self.keepalive_jobs: dict = {}

        # Receive thread
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
        if not self.target:
            return
        try:
            self.sock.sendto(msg.encode(), self.target)
        except OSError:
            pass

    def _recv_loop(self) -> None:
        while True:
            try:
                data, _ = self.sock.recvfrom(512)
                self.recv_q.put(data.decode(errors="replace").strip())
            except socket.timeout:
                pass
            except OSError:
                time.sleep(0.1)

    def _poll_recv_queue(self) -> None:
        while not self.recv_q.empty():
            try:
                self._handle_incoming(self.recv_q.get_nowait())
            except Exception:
                pass
        self.root.after(100, self._poll_recv_queue)

    def _handle_incoming(self, msg: str) -> None:
        if not msg.startswith("STATUS "):
            return
        kv: dict = {}
        for part in msg[7:].split():
            if "=" in part:
                k, v = part.split("=", 1)
                kv[k] = v
        if "wb_k"     in kv: self.wb_k_var.set(_wb_str(kv["wb_k"]))
        if "exp_f"    in kv: self.exp_f_var.set(_expf_str(kv["exp_f"]))
        if "exp_s"    in kv: self.exp_s_var.set(_exps_str(kv["exp_s"]))
        if "exp_g"    in kv: self.exp_g_var.set(kv["exp_g"])
        if "hostname" in kv: self.hostname_var.set(kv["hostname"])

    def _schedule_status_poll(self) -> None:
        if self.target:
            self._send("STATUS")
        self.root.after(STATUS_INTERVAL_MS, self._schedule_status_poll)

    # ── Connection ────────────────────────────────────────────────────────────

    def _is_typing_focus(self) -> bool:
        return isinstance(self.root.focus_get(), tk.Entry)

    def _connect(self) -> None:
        host = self.ip_var.get().strip()
        if not host:
            return
        self.conn_label.set("Resolving\u2026")
        threading.Thread(target=self._connect_worker, args=(host,), daemon=True).start()

    def _connect_worker(self, host: str) -> None:
        try:
            ip   = socket.gethostbyname(host)
            port = int(self.port_var.get())
            self.root.after(0, lambda: self._connect_done(ip, port))
        except socket.gaierror as exc:
            self.root.after(0, lambda: self.conn_label.set(f"DNS error: {exc}"))

    def _connect_done(self, ip: str, port: int) -> None:
        self.target = (ip, port)
        self.conn_label.set(f"\u25cf {ip}:{port}")
        self._send("STATUS")
        self.root.focus_set()

    def _disconnect(self) -> None:
        self._do_stop_all()
        self.target = None
        self.conn_label.set("Disconnected")

    # =========================================================================
    # Keep-alive machinery
    # =========================================================================

    def _start_action(self, action: str, packet: str) -> None:
        self._cancel_keepalive(action)
        self.key_held[action] = True
        self._send(packet)

        def repeat() -> None:
            if self.key_held.get(action, False) and self.target:
                self._send(packet)
                self.keepalive_jobs[action] = self.root.after(KEEPALIVE_MS, repeat)

        self.keepalive_jobs[action] = self.root.after(KEEPALIVE_MS, repeat)

    def _cancel_keepalive(self, action: str) -> None:
        job = self.keepalive_jobs.pop(action, None)
        if job:
            self.root.after_cancel(job)
        self.key_held[action] = False

    def _stop_action(self, action: str, off_packet: str) -> None:
        self._cancel_keepalive(action)
        for _ in range(3):
            self._send(off_packet)

    # =========================================================================
    # Camera command selection
    # =========================================================================

    def _select_cmd(self, cmd_id: int) -> None:
        self.current_cmd = cmd_id
        self._send(f"CMD {cmd_id}")
        for cid, btn in self.cmd_buttons.items():
            if cid == cmd_id:
                btn.configure(bg=C["seg_sel"], fg=C["seg_txt"])
            else:
                btn.configure(bg=C["surface"], fg=C["text"])
        for cid, (canvas, oval_id, txt_id) in self.cmd_canvases.items():
            if cid == cmd_id:
                canvas.itemconfigure(oval_id, fill=C["icon_sel"])
                canvas.itemconfigure(txt_id,  fill=C["white"])
            else:
                canvas.itemconfigure(oval_id, fill=C["icon_bg"])
                canvas.itemconfigure(txt_id,  fill=C["text"])

    # =========================================================================
    # Stop-all / actions
    # =========================================================================

    def _do_stop_all(self) -> None:
        for action in list(self.key_held):
            self._cancel_keepalive(action)
        for tid in list(self.release_timers.values()):
            self.root.after_cancel(tid)
        self.release_timers.clear()
        self._send("STOP")

    def _init_camera(self) -> None:
        self._do_stop_all()
        self._send("INIT")

    def _reset_device(self) -> None:
        self._do_stop_all()
        self._send("RESET")

    # =========================================================================
    # Key event routing
    # =========================================================================

    def _bind_keys(self) -> None:
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

        for sym, cmd_id in HOTKEY_CMD.items():
            self.root.bind(f"<KeyPress-{sym}>",
                           lambda e, c=cmd_id: (
                               None if self._is_typing_focus() else self._select_cmd(c)
                           ))

        self.root.bind("<Deactivate>", lambda e: self._do_stop_all())

    def _on_held_press(self, action: str, packet: str) -> None:
        if self._is_typing_focus():
            return
        pending = self.release_timers.pop(action, None)
        if pending:
            self.root.after_cancel(pending)
        if not self.key_held.get(action, False):
            self._start_action(action, packet)

    def _on_held_release(self, action: str, off_packet: str) -> None:
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
    # Button hold binding
    # =========================================================================

    def _bind_hold_btn(self, btn: tk.Button, on_pkt: str, off_pkt: str) -> None:
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

        # ── Main 3-col × 3-row grid ───────────────────────────────────────────
        # col 0 (left)  : mode controls + d-pad
        # col 1 (centre): camera name + status + actions + hotkeys
        # col 2 (right) : roll visualisation
        self.root.columnconfigure(0, weight=2)
        self.root.columnconfigure(1, weight=3)
        self.root.columnconfigure(2, weight=2)
        self.root.rowconfigure(0, weight=0)   # connection bar  (fixed)
        self.root.rowconfigure(1, weight=2)   # top row
        self.root.rowconfigure(2, weight=3)   # bottom row

        # ─── ROW 0 · Connection bar (full width) ─────────────────────────────
        cbar = tk.Frame(self.root, bg=C["card"], pady=5, padx=10)
        cbar.grid(row=0, column=0, columnspan=3, sticky="ew")
        cbar.columnconfigure(9, weight=1)

        for idx, (lbl_txt, var, w) in enumerate([
            ("Host :", self.ip_var,   18),
            ("Port :", self.port_var,  5),
        ]):
            base = idx * 2
            tk.Label(cbar, text=lbl_txt, bg=C["card"], fg=C["subtext"],
                     font=(FF, 9)).grid(row=0, column=base,   padx=(0, 2))
            tk.Entry(cbar, textvariable=var, width=w,
                     bg=C["white"], fg=C["text"], insertbackground=C["text"],
                     relief="solid", bd=1,
                     font=(FF, 9)).grid(row=0, column=base+1, padx=(0, 6))

        tk.Button(cbar, text="Connect", command=self._connect,
                  bg=C["seg_sel"], fg=C["white"], relief="flat",
                  font=(FF, 9, "bold"), padx=10, pady=3
                  ).grid(row=0, column=4, padx=(0, 3))
        tk.Button(cbar, text="\u2715", command=self._disconnect,
                  bg=C["red"], fg=C["white"], relief="flat",
                  font=(FF, 9, "bold"), padx=7, pady=3
                  ).grid(row=0, column=5)
        tk.Label(cbar, textvariable=self.conn_label,
                 bg=C["card"], fg=C["subtext"],
                 font=(FF, 9)).grid(row=0, column=9, sticky="e", padx=(0, 4))

        # ─── ROW 1 / COL 0 · Camera-mode selector ────────────────────────────
        mode_panel = tk.Frame(self.root, bg=C["bg"], padx=14, pady=12)
        mode_panel.grid(row=1, column=0, sticky="nsew", padx=4, pady=4)

        def _pill_row(items: list) -> None:
            """Row of flat buttons that behave like a segmented control."""
            track = tk.Frame(mode_panel, bg=C["surface"], padx=4, pady=4)
            track.pack(fill="x", pady=(0, 6))
            for cmd_id, label in items:
                btn = tk.Button(
                    track, text=label,
                    bg=C["surface"], fg=C["text"],
                    relief="flat", bd=0, font=(FF, 9),
                    padx=6, pady=6,
                    activebackground=C["seg_sel"],
                    activeforeground=C["white"],
                    cursor="hand2",
                    command=lambda c=cmd_id: self._select_cmd(c),
                )
                btn.pack(side="left", fill="x", expand=True)
                self.cmd_buttons[cmd_id] = btn

        _pill_row([(1,"Zoom 1"),(2,"Zoom 2"),(3,"Zoom 3"),
                   (4,"Zoom 4"),(5,"Zoom 5"),(6,"Zoom 6")])
        _pill_row([(PAN_TILT_SLOW,"Pan/Tilt Slow"),
                   (PAN_TILT_MEDIUM,"Pan/Tilt Medium"),
                   (PAN_TILT_FAST,"Pan/Tilt Fast")])

        # Circular icon buttons
        icon_row = tk.Frame(mode_panel, bg=C["bg"])
        icon_row.pack(fill="x", pady=(4, 0))

        for cmd_id, symbol, label in [
            (FOCUS,    "\u2299", "Focus"),
            (EXP_F,    "\u25c9", "Aperture"),
            (EXP_S,    "\u25f7", "Shutter"),
            (EXP_GAIN, "\u2733", "Exp-Gain"),
            (WB_K,     "\u25d1", "White Bal"),
        ]:
            cell = tk.Frame(icon_row, bg=C["bg"])
            cell.pack(side="left", expand=True)

            IC, M = 52, 6
            cv = tk.Canvas(cell, width=IC, height=IC,
                           bg=C["bg"], highlightthickness=0, cursor="hand2")
            cv.pack()
            ov_id = cv.create_oval(M, M, IC-M, IC-M,
                                   fill=C["icon_bg"], outline=C["icon_brd"], width=1.5)
            tx_id = cv.create_text(IC//2, IC//2, text=symbol,
                                   fill=C["text"], font=(FF, 17))

            lbl = tk.Label(cell, text=label, bg=C["bg"], fg=C["subtext"],
                           font=(FF, 8), cursor="hand2")
            lbl.pack(pady=(0, 2))

            handler = (lambda cid=cmd_id: lambda _e=None: self._select_cmd(cid))()
            cv.bind("<Button-1>",  handler)
            lbl.bind("<Button-1>", handler)
            self.cmd_canvases[cmd_id] = (cv, ov_id, tx_id)

        # ─── ROW 1 / COL 2 · Roll controls ───────────────────────────────────
        roll_panel = tk.Frame(self.root, bg=C["bg"], padx=14, pady=12)
        roll_panel.grid(row=1, column=2, sticky="nsew", padx=4, pady=4)
        roll_panel.columnconfigure(0, weight=1)

        RW, RH = 200, 160
        rcv = tk.Canvas(roll_panel, width=RW, height=RH,
                        bg=C["bg"], highlightthickness=0)
        rcv.pack()

        cx, cy, R = RW//2, RH//2 - 6, 62

        # CCW arc + arrowhead
        rcv.create_arc(cx-R, cy-R, cx+R, cy+R,
                       start=110, extent=120,
                       style="arc", outline=C["subtext"], width=3)
        rcv.create_polygon(cx-R+14, cy-20, cx-R+4,  cy-28, cx-R+24, cy-30,
                           fill=C["subtext"], outline="")
        # CW arc + arrowhead
        rcv.create_arc(cx-R, cy-R, cx+R, cy+R,
                       start=-50, extent=120,
                       style="arc", outline=C["subtext"], width=3)
        rcv.create_polygon(cx+R-14, cy-20, cx+R-4, cy-28, cx+R-24, cy-30,
                           fill=C["subtext"], outline="")

        # Camera-body sketch
        BW, BH = 54, 40
        bx1, by1 = cx-BW//2, cy-BH//2
        bx2, by2 = cx+BW//2, cy+BH//2
        rcv.create_rectangle(bx1, by1, bx2, by2,
                             fill=C["surface"], outline=C["border"], width=1.5)
        rcv.create_oval(cx-12, cy-12, cx+12, cy+12,
                        fill=C["bg"], outline=C["border"], width=1.5)
        rcv.create_oval(cx-5,  cy-5,  cx+5,  cy+5,
                        fill=C["subtext"], outline="")
        rcv.create_rectangle(cx-9, by1-9, cx+9, by1,
                             fill=C["surface"], outline=C["border"], width=1)

        rbtn_row = tk.Frame(roll_panel, bg=C["bg"])
        rbtn_row.pack(pady=(8, 0))

        for text, pkt_on, pkt_off in [
            ("\u21ba  CCW", "ROLL ccw on", "ROLL ccw off"),
            ("CW  \u21bb",  "ROLL cw on",  "ROLL cw off"),
        ]:
            b = tk.Button(rbtn_row, text=text,
                          bg=C["surface"], fg=C["text"], relief="flat",
                          font=(FF, 11), padx=10, pady=6,
                          activebackground=C["seg_sel"],
                          activeforeground=C["white"],
                          cursor="hand2")
            b.pack(side="left", padx=5)
            self._bind_hold_btn(b, pkt_on, pkt_off)

        # ─── ROW 2 / COL 0 · D-pad (circular, matches web UI) ────────────────
        dpad_panel = tk.Frame(self.root, bg=C["bg"])
        dpad_panel.grid(row=2, column=0, sticky="nsew", padx=4, pady=4)
        dpad_panel.columnconfigure(0, weight=1)
        dpad_panel.rowconfigure(0, weight=1)

        dpad_wrap = tk.Frame(dpad_panel, bg=C["bg"])
        dpad_wrap.grid(row=0, column=0)

        DP = 210
        dpcv = tk.Canvas(dpad_wrap, width=DP, height=DP,
                         bg=C["bg"], highlightthickness=0)
        dpcv.pack()
        dpcv.create_oval(8, 8, DP-8, DP-8,
                         fill=C["surface"], outline=C["border"], width=2)

        arrow_kw = dict(
            bg=C["surface"], fg=C["text"],
            relief="flat", bd=0, font=(FF, 20),
            padx=5, pady=3,
            activebackground=C["seg_sel"],
            activeforeground=C["white"],
            cursor="hand2",
        )
        btn_up    = tk.Button(dpad_wrap, text="\u25b2", **arrow_kw)
        btn_down  = tk.Button(dpad_wrap, text="\u25bc", **arrow_kw)
        btn_left  = tk.Button(dpad_wrap, text="\u25c4", **arrow_kw)
        btn_right = tk.Button(dpad_wrap, text="\u25ba", **arrow_kw)
        btn_cstop = tk.Button(dpad_wrap, text="\u25a0",
                              bg=C["surface"], fg=C["red"],
                              relief="flat", bd=0, font=(FF, 20),
                              padx=5, pady=3,
                              activebackground=C["red"],
                              activeforeground=C["white"],
                              command=self._do_stop_all,
                              cursor="hand2")

        ctr = DP // 2
        dpcv.create_window(ctr,   26,      window=btn_up)
        dpcv.create_window(ctr,   DP-26,   window=btn_down)
        dpcv.create_window(26,    ctr,     window=btn_left)
        dpcv.create_window(DP-26, ctr,     window=btn_right)
        dpcv.create_window(ctr,   ctr,     window=btn_cstop)

        self._bind_hold_btn(btn_up,    "DIR up on",    "DIR up off")
        self._bind_hold_btn(btn_down,  "DIR down on",  "DIR down off")
        self._bind_hold_btn(btn_left,  "DIR left on",  "DIR left off")
        self._bind_hold_btn(btn_right, "DIR right on", "DIR right off")

        # ─── ROW 2 / COL 1 · Camera name + status + actions + hotkeys ─────────
        centre = tk.Frame(self.root, bg=C["bg"])
        centre.grid(row=2, column=1, sticky="nsew", padx=8, pady=8)
        centre.columnconfigure(0, weight=1)

        # Camera name
        tk.Label(centre, textvariable=self.hostname_var,
                 bg=C["bg"], fg=C["text"],
                 font=(FF, 22, "bold"), anchor="center"
                 ).pack(fill="x", pady=(8, 4))

        # Status card
        stat_card = tk.Frame(centre, bg=C["card"], padx=16, pady=10)
        stat_card.pack(fill="x", pady=(0, 10))
        tk.Label(stat_card, text="Status",
                 bg=C["card"], fg=C["text"],
                 font=(FF, 12, "bold"), anchor="center").pack(fill="x", pady=(0, 6))

        for row_lbl, var in [
            ("White Balance :", self.wb_k_var),
            ("Exposure :",      self.exp_f_var),
            ("Shutter Speed :", self.exp_s_var),
            ("Exposure Gain :", self.exp_g_var),
        ]:
            r = tk.Frame(stat_card, bg=C["card"])
            r.pack(fill="x", pady=1)
            tk.Label(r, text=row_lbl, bg=C["card"], fg=C["subtext"],
                     font=(FF, 10), anchor="e", width=16).pack(side="left")
            tk.Label(r, textvariable=var, bg=C["card"], fg=C["text"],
                     font=(FF, 10, "bold"), anchor="w").pack(side="left", padx=6)

        # Action buttons (Stop / Reset / Init – circular canvas style)
        act_row = tk.Frame(centre, bg=C["bg"])
        act_row.pack(pady=(0, 10))

        for symbol, label, command, color in [
            ("\u25a0", "Stop",  self._do_stop_all, C["red"]),
            ("\u23fb", "Reset", self._reset_device, C["neutral"]),
            ("\u2691", "Init",  self._init_camera,  C["orange"]),
        ]:
            cell = tk.Frame(act_row, bg=C["bg"])
            cell.pack(side="left", padx=14)
            AC, AM = 54, 5
            acv = tk.Canvas(cell, width=AC, height=AC,
                            bg=C["bg"], highlightthickness=0, cursor="hand2")
            acv.pack()
            acv.create_oval(AM, AM, AC-AM, AC-AM,
                            fill=C["icon_bg"], outline=C["icon_brd"], width=1.5)
            acv.create_text(AC//2, AC//2, text=symbol,
                            fill=color, font=(FF, 17))
            acv.bind("<Button-1>", lambda e, cmd=command: cmd())
            tk.Label(cell, text=label, bg=C["bg"], fg=C["subtext"],
                     font=(FF, 9)).pack(pady=(0, 2))

        # Hotkeys reference
        hint_card = tk.Frame(centre, bg=C["card"], padx=14, pady=8)
        hint_card.pack(fill="x")
        tk.Label(hint_card, text="Keyboard Shortcuts",
                 bg=C["card"], fg=C["subtext"],
                 font=(FF, 9, "bold"), anchor="center").pack(fill="x", pady=(0, 4))
        for line in [
            "1 \u2013 6  :  Zoom speed",
            "f  : Focus       w  : White Balance",
            "a  : Aperture    s  : Shutter    g  : Gain",
            "]  : Pan Fast    [  : Pan Med    p  : Pan Slow",
            "Arrow keys  :  Pan / Tilt               (hold)",
            "Z  :  Zoom In        X  :  Zoom Out      (hold)",
        ]:
            tk.Label(hint_card, text=line,
                     bg=C["card"], fg=C["muted"],
                     font=("Courier", 8), anchor="center").pack(fill="x")

        # Initial highlight
        self._select_cmd(ZOOM_1)


# ── Entry point ───────────────────────────────────────────────────────────────
if __name__ == "__main__":
    root = tk.Tk()
    app = CameraControlApp(root)
    try:
        root.mainloop()
    finally:
        try:
            app._do_stop_all()
        except Exception:
            pass
        app.sock.close()
