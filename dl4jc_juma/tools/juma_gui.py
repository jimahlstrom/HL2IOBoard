#!/usr/bin/env python3
"""A window for the JUMA PA behind a Hermes Lite 2 IO board.

    python3 juma_gui.py                     find the HL2 on the network
    python3 juma_gui.py --hl2 192.168.1.50  a known HL2
    python3 juma_gui.py --usb /dev/cu.usbmodem11433301

Laid out after the ESP32 firmware's web dashboard, and using its palettes,
zone boundaries and wording (see juma_theme.py) - the same amplifier should not
need a second set of colours learned.

Tkinter comes with Python, so there is nothing to install for the HL2 route.
The USB route wants pyserial.

Which route: USB conflicts with nothing and carries the PA's status line at full
precision. The HL2 route needs no second cable, but its command packets share
the port the SDR software uses.
"""

import argparse
import os
import platform
import subprocess
import sys
import time
import tkinter as tk
import tkinter.font as tkfont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import juma_link as jl          # noqa: E402
import juma_theme as th         # noqa: E402
import juma_config as jc        # noqa: E402


def log_path():
    s = platform.system()
    if s == "Darwin":
        return os.path.expanduser("~/Library/Logs/JUMA-PA.log")
    if s == "Windows":
        base = os.environ.get("LOCALAPPDATA") or os.path.expanduser("~")
        return os.path.join(base, "JUMA PA", "juma-pa.log")
    base = os.environ.get("XDG_STATE_HOME") or os.path.expanduser("~/.local/state")
    return os.path.join(base, "juma-pa.log")


_logfile = None


def start_log():
    """Keep a log when there is no terminal to print to.

    Double-clicked, stdout and stderr go nowhere, and any mistake in here turns
    into "the icon does nothing" with no way to find out why. That is not a
    hypothetical: an argument passed twice to a Label did exactly that, and the
    log is what found it in one go.
    """
    global _logfile
    if sys.stderr is not None and sys.stderr.isatty():
        return                      # a terminal is a better log
    try:
        p = log_path()
        os.makedirs(os.path.dirname(p), exist_ok=True)
        _logfile = open(p, "a", buffering=1)
        _logfile.write("--- %s starting ---\n" % time.strftime("%Y-%m-%d %H:%M:%S"))
        sys.stderr = _logfile
        sys.stdout = _logfile
    except OSError:
        pass                        # a log is nice, not necessary


def say(text):
    """To stderr - a terminal, or the log file when started from an icon."""
    try:
        sys.stderr.write("%s\n" % text)
        sys.stderr.flush()
    except (OSError, ValueError, AttributeError):
        pass


def resource_dir():
    """Where the icons are.

    Frozen by PyInstaller they sit in the unpacked bundle, which is
    sys._MEIPASS; from a checkout they sit next to this file.
    """
    return getattr(sys, "_MEIPASS",
                   os.path.dirname(os.path.abspath(__file__)))


def set_icon(root):
    """The window's own icon. A bundle carries it; a checkout has it too."""
    png = os.path.join(resource_dir(), "icons", "juma-pa-128.png")
    try:
        root._icon = tk.PhotoImage(file=png)
        root.iconphoto(True, root._icon)
    except (tk.TclError, OSError):
        pass            # an icon is nice, not necessary

REFRESH_HL2_MS = 1000
REFRESH_USB_MS = 500
ALARM_REPEAT_MS = 5000


def system_prefers_light(root):
    """Tk knows the desktop's background; a bright one means a light desktop."""
    try:
        bg = root.winfo_rgb(root.cget("background"))
        return sum(bg) / 3 > 32768
    except tk.TclError:
        return False


def beep():
    """One alarm tone, on whichever of the three systems this is."""
    s = platform.system()
    try:
        if s == "Darwin":
            subprocess.Popen(["afplay", "/System/Library/Sounds/Sosumi.aiff"],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        elif s == "Windows":
            import winsound
            winsound.Beep(880, 400)
        else:
            for cmd in (["paplay", "/usr/share/sounds/freedesktop/stereo/bell.oga"],
                        ["aplay", "-q", "/usr/share/sounds/alsa/Front_Center.wav"]):
                try:
                    subprocess.Popen(cmd, stdout=subprocess.DEVNULL,
                                     stderr=subprocess.DEVNULL)
                    return
                except OSError:
                    continue
            print("\a", end="", flush=True)
    except Exception:
        pass            # a missing sound file must never take the window down


class App:
    def __init__(self, root, link, lang="de", theme="system"):
        self.root, self.link = root, link
        self.lang, self.theme_choice = lang, theme
        self.is_hl2 = isinstance(link, jl.Hl2Link)
        self.refresh = REFRESH_HL2_MS if self.is_hl2 else REFRESH_USB_MS
        self.pal = th.DARK
        self.mode = 0
        self.alarm_on = False
        self.muted = False
        self.flash = False
        self.cards = []         # widgets to recolour: (widget, bg key, fg key)
        self.meters = []
        self.buttons = []
        self.fails = 0          # consecutive failed reads
        self.busy = False       # the radio is streaming for somebody else
        self._link_ok = True    # last state written to the log, see _note_link

        self._build()
        self._apply_theme()
        self.root.after(50, self._tick)
        self.root.after(ALARM_REPEAT_MS, self._alarm_tick)

    def t(self, key):
        return th.L[self.lang][key]

    # --- building ---------------------------------------------------------
    def _card(self, parent, title=None):
        f = tk.Frame(parent, padx=12, pady=10)
        self.cards.append((f, "card", None))
        if title is not None:
            lab = tk.Label(f, text=title, anchor="w", font=th.FONT_M)
            lab.pack(fill="x", pady=(0, 8))
            self.cards.append((lab, "card", "dim"))
            self._titles = getattr(self, "_titles", [])
            self._titles.append((lab, title))
        return f

    def _button(self, parent, text, cmd, width=None):
        b = th.Btn(parent, text, cmd, pal=self.pal, width=width, font=th.FONT_S)
        self.buttons.append(b)
        return b

    def _build(self):
        r = self.root
        outer = tk.Frame(r, padx=14, pady=12)
        outer.pack(fill="both", expand=True)
        self.cards.append((outer, "bg", None))
        self.outer = outer

        # header -----------------------------------------------------------
        head = tk.Frame(outer)
        head.pack(fill="x")
        self.cards.append((head, "bg", None))
        self.state_lab = tk.Label(head, text="…", anchor="w",
                                  font=th.FONT_STATE)
        self.state_lab.pack(side="left")
        self.cards.append((self.state_lab, "bg", "fg"))
        self.link_lab = tk.Label(head, text=link_name(self.link), anchor="e",
                                 font=th.FONT_S)
        self.link_lab.pack(side="right")
        self.cards.append((self.link_lab, "bg", "dim"))

        self.note_lab = tk.Label(outer, text="", anchor="w", font=th.FONT_S)
        self.note_lab.pack(fill="x", pady=(2, 8))
        self.cards.append((self.note_lab, "bg", "dim"))

        self.banner = tk.Label(outer, text="", anchor="center", pady=8,
                               font=th.FONT_M)
        self.cards.append((self.banner, "bg", "bad"))

        # levels -------------------------------------------------------------
        lv = self._card(outer, self.t("lvlHdr"))
        lv.pack(fill="x", pady=(0, 10))
        self.rf = th.Bar(lv, "RF", th.RF_MIN, th.RF_MAX, th.RF_WARN, th.RF_HIGH,
                         unit=" W", dec=1)
        self.rf.pack(fill="x", pady=(0, 6))
        self.swr = th.Bar(lv, "VSWR", th.SWR_MIN, th.SWR_MAX, th.SWR_WARN,
                          th.SWR_HIGH, dec=2)
        self.swr.pack(fill="x")
        self.meters += [self.rf, self.swr]

        # dials --------------------------------------------------------------
        dl = self._card(outer, self.t("thHdr"))
        dl.pack(fill="x", pady=(0, 10))
        row = tk.Frame(dl)
        row.pack()
        self.cards.append((row, "card", None))

        fw = (th.T_WARN - th.T_MIN) / float(th.T_MAX - th.T_MIN)
        fh = (th.T_HIGH - th.T_MIN) / float(th.T_MAX - th.T_MIN)
        self.g_temp = th.Dial(row, self.t("tTemp"),
                              [(fw, "ok"), (fh, "warn"), (1.0, "bad")], "°")
        self.g_fan = th.Dial(row, self.t("tFan"),
                             [(.25, "off"), (.5, "ok"), (.75, "warn"), (1.0, "bad")])
        vf = lambda v: (v - th.V_MIN) / (th.V_MAX - th.V_MIN)
        self.g_volt = th.Dial(row, self.t("tVolt"),
                              [(vf(th.V_UV), "bad"), (vf(th.V_PRE), "warn"),
                               (vf(th.V_OVA), "ok"), (vf(th.V_OV), "warn"),
                               (1.0, "bad")], " V")
        self.g_amp = th.Dial(row, self.t("tAmp"),
                             [(th.I_W1, "ok"), (th.I_W2, "warn"), (1.0, "bad")], " A")
        for g in (self.g_temp, self.g_fan, self.g_volt, self.g_amp):
            g.pack(side="left", padx=4)
            self.meters.append(g)

        # band ---------------------------------------------------------------
        bd = self._card(outer, self.t("bandHdr"))
        bd.pack(fill="x", pady=(0, 10))
        brow = tk.Frame(bd)
        brow.pack(fill="x")
        self.cards.append((brow, "card", None))
        self.band_btns = {}
        for n in range(1, 10):
            b = self._button(brow, jl.BAND_NAMES[n],
                             lambda n=n: self._set_band(n), width=5)
            b.pack(side="left", padx=2)
            self.band_btns[n] = b

        self.follow = tk.BooleanVar(value=True)
        self.follow_cb = tk.Checkbutton(bd, text=self.t("follow"),
                                        variable=self.follow, anchor="w",
                                        command=self._apply_mode, font=th.FONT_S,
                                        highlightthickness=0, borderwidth=0)
        self.follow_cb.pack(fill="x", pady=(8, 0))
        self.cards.append((self.follow_cb, "card", "fg"))

        self.sel_lab = tk.Label(bd, text="", anchor="w", font=th.FONT_XS)
        self.sel_lab.pack(fill="x")
        self.cards.append((self.sel_lab, "card", "dim"))

        # attenuator ---------------------------------------------------------
        at = self._card(outer, self.t("attHdr"))
        at.pack(fill="x", pady=(0, 10))
        arow = tk.Frame(at)
        arow.pack(fill="x")
        self.cards.append((arow, "card", None))
        self.gain_btns = {}
        for n in (1, 2, 3, 4):
            b = self._button(arow, "G%d" % n, lambda n=n: self._set_gain(n), width=4)
            b.pack(side="left", padx=2)
            self.gain_btns[n] = b
        gl = tk.Label(arow, text="G1 = 6 dB · G2 = 4 dB · G3 = 2 dB · G4 = 0 dB",
                      font=th.FONT_S)
        gl.pack(side="left", padx=(12, 0))
        self.cards.append((gl, "card", "dim"))

        # control ------------------------------------------------------------
        ct = self._card(outer, self.t("ctlHdr"))
        ct.pack(fill="x", pady=(0, 10))
        crow = tk.Frame(ct)
        crow.pack(fill="x")
        self.cards.append((crow, "card", None))
        self.b_op = self._button(crow, self.t("bOperate"),
                                 lambda: self._cmd(jl.CMD_OPERATE))
        self.b_sb = self._button(crow, self.t("bStandby"),
                                 lambda: self._cmd(jl.CMD_STANDBY))
        self.b_clr = self._button(crow, self.t("bClear"),
                                  lambda: self._cmd(jl.CMD_CLR_ALARM))
        self.b_auto = self._button(crow, self.t("bAuto"),
                                   lambda: self._cmd(jl.CMD_AUTO_BAND))
        # Silencing the tone and clearing the alarm are two different acts: one
        # is about the room, the other tells the amplifier to let go of a
        # protective shutdown. The dashboard keeps them apart and so does this.
        self.b_mute = self._button(crow, self.t("bMute"), self._toggle_mute)
        for b in (self.b_op, self.b_sb, self.b_clr, self.b_auto, self.b_mute):
            b.pack(side="left", padx=4)

        self.hold = tk.BooleanVar()
        self.hold_cb = tk.Checkbutton(ct, text=self.t("hold"), variable=self.hold,
                                      anchor="w", command=self._apply_mode,
                                      font=th.FONT_S,
                                      highlightthickness=0, borderwidth=0)
        self.hold_cb.pack(fill="x", pady=(8, 0))
        self.cards.append((self.hold_cb, "card", "fg"))

        self.sound = tk.BooleanVar(value=True)
        self.sound_cb = tk.Checkbutton(ct, text=self.t("sound"), variable=self.sound,
                                       anchor="w", font=th.FONT_S,
                                       highlightthickness=0, borderwidth=0)
        self.sound_cb.pack(fill="x")
        self.cards.append((self.sound_cb, "card", "fg"))

        # footer -------------------------------------------------------------
        ft = tk.Frame(outer)
        ft.pack(fill="x")
        self.cards.append((ft, "bg", None))
        self.diag = tk.Label(ft, text="", anchor="w", font=th.FONT_XS)
        self.diag.pack(side="left")
        self.cards.append((self.diag, "bg", "dim"))
        self.err = tk.Label(ft, text="", anchor="e", font=th.FONT_XS)
        self.err.pack(side="right")
        self.cards.append((self.err, "bg", "bad"))

        self._build_menu()

    def _build_menu(self):
        m = tk.Menu(self.root)

        lang = tk.Menu(m, tearoff=0)
        self.lang_var = tk.StringVar(value=self.lang)
        for code, name in (("de", "Deutsch"), ("en", "English")):
            lang.add_radiobutton(label=name, value=code, variable=self.lang_var,
                                 command=self._apply_lang)
        m.add_cascade(label=self.t("lang"), menu=lang)

        theme = tk.Menu(m, tearoff=0)
        self.theme_var = tk.StringVar(value=self.theme_choice)
        for code in ("system", "light", "dark"):
            theme.add_radiobutton(label=self.t("th" + code.capitalize()),
                                  value=code, variable=self.theme_var,
                                  command=self._apply_theme)
        m.add_cascade(label=self.t("theme"), menu=theme)

        # The two bits that only matter on the Pico's USB port. Rarely touched,
        # so they live here rather than taking room in the window - but they do
        # belong somewhere: without them the only way to switch the proxy is a
        # terminal on the USB console, and the mode is kept in flash, so the
        # choice outlives the power.
        pico = tk.Menu(m, tearoff=0)
        self.proxy_var = tk.BooleanVar(value=bool(self.mode & jl.MODE_PROXY))
        self.telem_var = tk.BooleanVar(value=bool(self.mode & jl.MODE_TELEMETRY))
        pico.add_checkbutton(label=self.t("mProxy"), variable=self.proxy_var,
                             command=lambda: self._set_mode_bit(jl.MODE_PROXY,
                                                                self.proxy_var))
        pico.add_checkbutton(label=self.t("mTelem"), variable=self.telem_var,
                             command=lambda: self._set_mode_bit(jl.MODE_TELEMETRY,
                                                                self.telem_var))
        if not self.is_hl2:
            # Reached over USB, this window IS the telemetry reader. Letting it
            # switch off its own supply would be a trap.
            pico.entryconfigure(1, state="disabled")
        pico.add_separator()
        for line in self.t("mProxyHint").split("\n"):
            pico.add_command(label=line, state="disabled")
        m.add_cascade(label=self.t("pico"), menu=pico)

        self.root.config(menu=m)
        self.menu = m

    # --- theme and language ----------------------------------------------
    def _apply_theme(self):
        choice = self.theme_var.get() if hasattr(self, "theme_var") else self.theme_choice
        if choice == "system":
            self.pal = th.LIGHT if system_prefers_light(self.root) else th.DARK
        else:
            self.pal = th.LIGHT if choice == "light" else th.DARK
        p = self.pal
        self.root.configure(bg=p["bg"])
        for w, bgk, fgk in self.cards:
            try:
                w.configure(bg=p[bgk])
                if fgk:
                    w.configure(fg=p[fgk])
                if isinstance(w, tk.Button):
                    w.configure(activebackground=p["btn2"], activeforeground=p["fg"])
                if isinstance(w, tk.Checkbutton):
                    w.configure(selectcolor=p["card2"], activebackground=p[bgk],
                                activeforeground=p["fg"])
            except tk.TclError:
                pass
        for meter in self.meters:
            meter.theme(p)
        for b in self.buttons:
            b.theme(p)

    def _apply_lang(self):
        self.lang = self.lang_var.get()
        self.b_op.configure(text=self.t("bOperate"))
        self.b_sb.configure(text=self.t("bStandby"))
        self.b_clr.configure(text=self.t("bClear"))
        self.b_auto.configure(text=self.t("bAuto"))
        self.follow_cb.configure(text=self.t("follow"))
        self.hold_cb.configure(text=self.t("hold"))
        self.sound_cb.configure(text=self.t("sound"))
        self.b_mute.configure(text=self.t("bMuted") if self.muted else self.t("bMute"))
        self.g_temp.label = self.t("tTemp")
        self.g_fan.label = self.t("tFan")
        self.g_volt.label = self.t("tVolt")
        self.g_amp.label = self.t("tAmp")
        titles = [self.t("lvlHdr"), self.t("thHdr"), self.t("bandHdr"),
                  self.t("attHdr"), self.t("ctlHdr")]
        for (lab, _), new in zip(getattr(self, "_titles", []), titles):
            lab.configure(text=new)
        self.root.config(menu="")
        self._build_menu()
        for meter in self.meters:
            meter.redraw()

    # --- actions ----------------------------------------------------------
    def _guard(self, fn):
        try:
            fn()
            self.err.configure(text="")
        except jl.LinkError as e:
            self.err.configure(text=str(e))

    def _cmd(self, code):
        self._guard(lambda: self.link.write(jl.REG_CMD, code))

    def _set_gain(self, n):
        self._guard(lambda: self.link.write(jl.REG_SET_GAIN, n))

    def _set_band(self, n):
        def go():
            if self.follow.get():
                self.follow.set(False)
                self._push_mode()
            self.link.write(jl.REG_SET_BAND, n)
        self._guard(go)

    def _set_mode_bit(self, bit, var):
        def go():
            self.mode = (self.mode | bit) if var.get() else (self.mode & ~bit)
            self.link.write(jl.REG_MODE, self.mode)
        self._guard(go)

    def _apply_mode(self):
        self._guard(self._push_mode)

    def _push_mode(self):
        mode = self.mode
        mode = (mode | jl.MODE_HOLD_OPERATE) if self.hold.get() else (mode & ~jl.MODE_HOLD_OPERATE)
        mode = (mode & ~jl.MODE_NO_BAND) if self.follow.get() else (mode | jl.MODE_NO_BAND)
        self.mode = mode
        self.link.write(jl.REG_MODE, mode)

    # --- alarm ------------------------------------------------------------
    def _toggle_mute(self):
        self.muted = not self.muted
        self.b_mute.configure(text=self.t("bMuted") if self.muted
                              else self.t("bMute"))
        self.b_mute.state(active=self.muted)

    def _alarm_tick(self):
        if self.alarm_on and self.sound.get() and not self.muted:
            beep()
        if self.alarm_on:
            self.flash = not self.flash
            self.root.title(("!!! " if self.flash else "") + self.t("title"))
        self.root.after(ALARM_REPEAT_MS, self._alarm_tick)

    # --- refresh ----------------------------------------------------------
    def _tick(self):
        try:
            st = self.link.read_status()
            self.err.configure(text="")
            self._show(st)
            self._note_link(True, "")
        except jl.LinkError as e:
            self.err.configure(text=str(e))
            self.state_lab.configure(text=self.t("nolink"), fg=self.pal["bad"])
            self._note_link(False, str(e))
        self.root.after(self.refresh, self._tick)

    def _note_link(self, ok, why):
        """Put the coming and going of the link in the log, with the time.

        The label in the window says what is wrong now; nobody watches a label
        for six hours. A run that has to answer "when did it stop, and did it
        come back" needs the wall clock, and only the two changes - not one line
        per second saying the same thing.
        """
        if ok == self._link_ok:
            return
        self._link_ok = ok
        say("%s  %s%s" % (time.strftime("%Y-%m-%d %H:%M:%S"),
                          "link back" if ok else "link lost",
                          "" if ok else ": " + why.splitlines()[0]))

    def _show(self, st):
        p, was = self.pal, self.alarm_on
        self.mode = st.mode
        self.hold.set(bool(st.mode & jl.MODE_HOLD_OPERATE))
        self.follow.set(not (st.mode & jl.MODE_NO_BAND))
        if hasattr(self, "proxy_var"):
            self.proxy_var.set(bool(st.mode & jl.MODE_PROXY))
            self.telem_var.set(bool(st.mode & jl.MODE_TELEMETRY))

        if not st.link:
            self.state_lab.configure(text=self.t("silent"), fg=p["bad"])
        elif st.pa_tx or st.hl2_tx:
            self.state_lab.configure(text=self.t("tx"),
                                     fg=p["warn"] if st.operate else p["dim"])
        else:
            self.state_lab.configure(text=self.t("operate") if st.operate
                                     else self.t("standby"),
                                     fg=p["ok"] if st.operate else p["dim"])

        names = self.t("alarms")
        alarms = [names[i] for i in range(6) if st.alarms & (1 << i)]
        self.alarm_on = bool(alarms)
        if alarms:
            self.banner.configure(text=" · ".join(alarms), fg=p["bad"])
            self.banner.pack(fill="x", pady=(0, 8), before=self.note_lab)
        else:
            self.banner.pack_forget()
            self.root.title(self.t("title"))
        if not was and self.alarm_on:
            # A fresh alarm un-mutes: silence was granted for the last one.
            self.muted = False
            self.b_mute.configure(text=self.t("bMute"))
            if self.sound.get():
                beep()

        if not st.link:
            note = self.t("paoff")
        elif not st.following:
            note = self.t("noBand")
        elif st.want_band == 0:
            note = self.t("nofreq")
        else:
            note = self.t("bandok") % jl.BAND_NAMES.get(st.want_band, "?")
        self.note_lab.configure(text=note)

        self.rf.set(st.watts)
        self.swr.set(st.swr if (st.swr or 0) >= 1.0 else None)
        tc = "bad" if st.temp >= th.T_HIGH else "warn" if st.temp >= th.T_WARN else "fg"
        self.g_temp.set((st.temp - th.T_MIN) / float(th.T_MAX - th.T_MIN),
                        str(st.temp), tc)
        self.g_fan.set((st.fan + 1) / 4.0, self.t("fans")[st.fan] if st.fan < 4
                       else str(st.fan), "fg")
        v = st.volts or 0.0
        vc = "bad" if (v < th.V_UV or v > th.V_OV) else \
             "warn" if (v < th.V_PRE or v > th.V_OVA) else "fg"
        self.g_volt.set((v - th.V_MIN) / (th.V_MAX - th.V_MIN), "%.2f" % v, vc)
        a = st.amps or 0.0
        ac = "bad" if a >= th.I_TRIP * th.I_W2 else \
             "warn" if a >= th.I_TRIP * th.I_W1 else "fg"
        self.g_amp.set(a / th.I_TRIP, "%.1f" % a, ac)

        # The band in use stays highlighted even while the buttons are locked,
        # which is the whole point of showing it.
        for n, b in self.band_btns.items():
            b.state(enabled=not st.following, active=(n == st.band))
        for n, b in self.gain_btns.items():
            b.state(active=(n == st.gain))
        self.b_clr.state(enabled=bool(alarms))
        self.b_mute.state(enabled=bool(alarms), active=self.muted)
        self.sel_lab.configure(text="%s: %s" % (
            self.t("tSel"), self.t("auto") if st.auto_sel else self.t("manual")))

        extra = ""
        if self.is_hl2 and getattr(self.link, "rounds", 0):
            extra = " · %d/%d" % (self.link.retries, self.link.rounds)
        self.diag.configure(text="replies %d · bad %d · lost %d · mode %02X · fault %02X%s"
                                 % (st.replies, st.badlines, st.lost, st.mode,
                                    st.fault, extra))


def link_name(link):
    return link.describe()


class Connect(tk.Frame):
    """Asked for the connection when there is nobody to type a command line.

    Started from an icon there are no arguments, so the window tries what worked
    last time, then a broadcast, and only then asks. Whatever answers is written
    back to the settings, so the asking happens once.
    """

    def __init__(self, master, cfg, lang):
        super().__init__(master, padx=16, pady=14)
        self.cfg, self.lang, self.link = cfg, lang, None
        pal = th.DARK
        master.configure(bg=pal["bg"])
        self.configure(bg=pal["bg"])
        self.pack(fill="both", expand=True)

        def lab(text, font=th.FONT_S, **kw):
            return tk.Label(self, text=text, anchor="w", bg=pal["bg"],
                            fg=pal["fg"], font=font, **kw)

        lab(th.L[lang]["connTitle"], font=th.FONT_M).grid(
            row=0, column=0, columnspan=3, sticky="w", pady=(0, 10))

        lab(th.L[lang]["connHl2"]).grid(row=1, column=0, sticky="w")
        self.e_ip = tk.Entry(self, width=18, font=th.FONT_S)
        self.e_ip.insert(0, cfg.get("hl2") or "")
        self.e_ip.grid(row=1, column=1, sticky="w", padx=6)

        lab(th.L[lang]["connPort"]).grid(row=2, column=0, sticky="w")
        self.e_port = tk.Entry(self, width=8, font=th.FONT_S)
        self.e_port.insert(0, str(cfg.get("port") or jl.CMD_PORT))
        self.e_port.grid(row=2, column=1, sticky="w", padx=6)

        lab(th.L[lang]["connUsb"]).grid(row=3, column=0, sticky="w", pady=(8, 0))
        self.e_usb = tk.Entry(self, width=28, font=th.FONT_S)
        self.e_usb.insert(0, cfg.get("usb") or "")
        self.e_usb.grid(row=3, column=1, columnspan=2, sticky="w",
                        padx=6, pady=(8, 0))

        # Filled by a search. One radio connects straight away; several are
        # put here to choose from, because picking the first of two would be
        # picking at random.
        self.listbox = tk.Listbox(self, height=4, width=52, font=th.FONT_XS,
                                  bg=pal["card"], fg=pal["fg"],
                                  selectbackground=pal["acc"],
                                  highlightthickness=0, borderwidth=0,
                                  exportselection=False)
        self.listbox.grid(row=4, column=0, columnspan=3, sticky="ew",
                          pady=(10, 4))
        self.listbox.bind("<Double-Button-1>", lambda e: self._take_selected())
        self.listbox.grid_remove()          # only shown when there is a choice
        self.radios = []

        self.msg = tk.Label(self, text=th.L[lang]["connHint"], justify="left",
                            anchor="w", bg=pal["bg"], fg=pal["dim"],
                            font=th.FONT_XS)
        self.msg.grid(row=5, column=0, columnspan=3, sticky="w", pady=(4, 10))

        row = tk.Frame(self, bg=pal["bg"])
        row.grid(row=6, column=0, columnspan=3, sticky="w")
        th.Btn(row, th.L[lang]["connSearch"], self._search, pal=pal,
               font=th.FONT_S).pack(side="left", padx=(0, 6))
        th.Btn(row, th.L[lang]["connOk"], self._connect, pal=pal,
               font=th.FONT_S).pack(side="left", padx=6)
        th.Btn(row, th.L[lang]["connQuit"], master.destroy, pal=pal,
               font=th.FONT_S).pack(side="left", padx=6)

    def _say(self, text):
        self.msg.configure(text=text)
        self.update_idletasks()

    def _search(self):
        self._say(th.L[self.lang]["connBusy"])
        self.update_idletasks()
        self.show(jl.Hl2Link.discover())

    def show(self, found):
        """What a search turned up. One connects, several are offered."""
        self.radios = list(found)
        if not self.radios:
            self.listbox.grid_remove()
            self._say(th.L[self.lang]["connNone"] + jl.local_network_hint())
            return
        if len(self.radios) == 1:
            self._fill(self.radios[0])
            self._connect()
            return
        self.listbox.grid()
        self.listbox.delete(0, "end")
        for r in self.radios:
            self.listbox.insert("end", str(r))
        self.listbox.selection_set(0)
        self._say(th.L[self.lang]["connMany"] % len(self.radios))

    def _fill(self, r):
        self.e_ip.delete(0, "end"); self.e_ip.insert(0, r.ip)
        self.e_port.delete(0, "end"); self.e_port.insert(0, str(r.port))

    def _take_selected(self):
        sel = self.listbox.curselection()
        if sel:
            self._fill(self.radios[sel[0]])
            self._connect()

    def _connect(self):
        sel = self.listbox.curselection()
        if sel and self.listbox.winfo_ismapped():
            self._fill(self.radios[sel[0]])
        usb = self.e_usb.get().strip()
        ip = self.e_ip.get().strip()
        try:
            if usb:
                link = jl.UsbLink(usb)
                remember(self.cfg, "usb", usb)
            elif ip:
                port = int(self.e_port.get().strip() or jl.CMD_PORT)
                link = jl.Hl2Link(ip, port)
                link.read_status()          # prove it before closing the dialog
                remember(self.cfg, "hl2", ip, port)
            else:
                self._search()
                return
        except (jl.LinkError, ValueError, OSError) as e:
            self._say(th.L[self.lang]["connFail"] % e)
            return
        self.link = link
        self.master.quit()


def try_saved(cfg, settle=None):
    """What worked last time, if it still does."""
    try:
        if cfg.get("kind") == "usb" and cfg.get("usb"):
            return jl.UsbLink(cfg["usb"])
        if cfg.get("hl2"):
            link = jl.Hl2Link(cfg["hl2"], cfg.get("port") or jl.CMD_PORT, settle=settle)
            link.read_status()
            return link
    except (jl.LinkError, OSError):
        pass
    return None


def open_link(args, cfg):
    """Command line first, then a USB cable, then the remembered address,
    then a broadcast.

    Says out loud what it tried. Started from an icon this goes to the
    launcher's log, and it is the only way to tell a radio that is switched off
    from a system that will not let the program onto the network at all.
    """
    if args.usb:
        return jl.UsbLink(args.usb)
    if args.hl2:
        return jl.Hl2Link(args.hl2, args.port, settle=args.settle)

    # Before the remembered address, not after it: the point of preferring the
    # cable is to keep the HL2's I2C bridge free, and a remembered address would
    # otherwise win every time the cable happens to be plugged in.
    if not args.no_usb:
        link = jl.UsbLink.find()
        if link:
            say("connected over USB: %s" % link.port)
            remember(cfg, "usb", link.port)
            return link
        if jl.UsbLink.ports():
            say("a Pico is on USB but did not answer - trying the network")

    say("local addresses: %s" % (jl.Hl2Link.local_addresses() or "none"))
    if cfg.get("hl2") or cfg.get("usb"):
        say("trying the remembered %s" % (cfg.get("usb") or
                                          "%s:%s" % (cfg.get("hl2"), cfg.get("port"))))
    link = try_saved(cfg, args.settle)
    if link:
        say("connected to %s" % link.describe())
        return link
    if cfg.get("hl2") or cfg.get("usb"):
        say("  that did not answer")

    say("searching the network …")
    found = jl.Hl2Link.discover()
    for r in found:
        # str(), not "%s" % r: Radio is a namedtuple, and a tuple on the right
        # of % is taken as the argument list - four values for one placeholder.
        say("  " + str(r))
    if len(found) == 1:
        r = found[0]
        say("connecting to %s:%d" % (r.ip, r.port))
        remember(cfg, "hl2", r.ip, r.port)
        return jl.Hl2Link(r.ip, r.port, settle=args.settle)
    if len(found) > 1:
        say("%d radios answered - asking which one" % len(found))
        return found                # the dialog turns this into a choice
    say("nothing answered" + jl.local_network_hint())
    return None


def remember(cfg, kind, addr, port=None):
    """Write the connection down at once.

    Not on the way out: a window that is force-quit, or a search that had to
    fight the system for permission, would otherwise lose the address it just
    took trouble to find.
    """
    cfg["kind"] = kind
    if kind == "usb":
        cfg["usb"] = addr
    else:
        cfg["hl2"], cfg["port"] = addr, port
    jc.save(cfg)


def check_tk():
    """Apple's /usr/bin/python3 still carries Tk 8.5, which on current macOS
    lays this window out correctly and then paints neither the labels nor the
    meters. Better to say so than to hand over an empty window."""
    if tk.TkVersion < 8.6:
        sys.stderr.write(
            "This is Tk %s. It cannot draw this window - 8.6 or newer is "
            "needed.\n" % tk.TkVersion)
        if sys.platform == "darwin":
            sys.stderr.write("  brew install python3 python-tk\n"
                             "  and start it with that python3.\n")
        else:
            sys.stderr.write("  Debian/Ubuntu: sudo apt install python3-tk\n")
        return False
    return True


def main():
    start_log()
    say("JUMA PA, Python %s, Tk %s, %s"
        % (platform.python_version(), tk.TkVersion, platform.platform()))
    if not check_tk():
        return 1
    ap = argparse.ArgumentParser(description="JUMA PA control via the HL2 IO board")
    ap.add_argument("--hl2", metavar="IP", help="the Hermes Lite 2's address")
    ap.add_argument("--port", type=int, default=jl.CMD_PORT,
                    help="its command port")
    ap.add_argument("--no-usb", action="store_true",
                    help="do not look for a Pico on USB, go over the HL2")
    ap.add_argument("--usb", metavar="DEV", help="the Pico's serial port instead")
    ap.add_argument("--settle", type=float, default=None,
                    help="seconds between commands to the HL2 (default %.3f)"
                         % jl.Hl2Link.SETTLE)
    ap.add_argument("--lang", choices=("de", "en"), default=None)
    ap.add_argument("--theme", choices=("system", "light", "dark"), default=None)
    args = ap.parse_args()

    cfg = jc.load()
    lang = args.lang or cfg.get("lang") or "de"
    theme = args.theme or cfg.get("theme") or "system"

    link = open_link(args, cfg)
    choices = []
    if isinstance(link, list):
        choices, link = link, None
    if link is None:
        # Nothing answered and nobody passed an address: ask, in a window,
        # because there may well be no terminal to print to.
        ask = tk.Tk()
        ask.title(th.L[lang]["connTitle"])
        dlg = Connect(ask, cfg, lang)
        if choices:
            dlg.show(choices)
        ask.mainloop()
        link = dlg.link
        try:
            ask.destroy()
        except tk.TclError:
            pass
        if link is None:
            return

    root = tk.Tk()
    root.title("JUMA PA")
    root.minsize(620, 200)
    set_icon(root)
    app = App(root, link, lang=lang, theme=theme)
    try:
        root.mainloop()
    finally:
        cfg["lang"] = app.lang
        cfg["theme"] = app.theme_var.get()
        jc.save(cfg)            # the address is already written; this is taste
        link.close()
        del app


if __name__ == "__main__":
    try:
        sys.exit(main() or 0)
    except SystemExit:
        raise
    except BaseException:
        import traceback
        traceback.print_exc()       # into the log, where it can be read
        raise
