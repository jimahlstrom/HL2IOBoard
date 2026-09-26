"""Colours, wording and the two meter widgets for the JUMA window.

All of it is taken from the ESP32 dashboard (src/index_html.h) rather than
invented again: the same palettes, the same zone boundaries, the same wording.
An operator who knows the web interface should not have to learn a second set
of colours for the same amplifier.
"""

import math
import tkinter as tk

# 'TkDefaultFont' is a NAMED font, not a family, so ("TkDefaultFont", 10) asks
# Tk for a family of that name and gets whatever it falls back to. Name a real
# family instead - these three exist on macOS, Windows and Linux alike.
FONT_XS = ("Helvetica", 9)          # scale figures under the bars
FONT_S = ("Helvetica", 11)          # labels, buttons, switches
FONT_M = ("Helvetica", 12, "bold")  # card titles
FONT_BIG = ("Helvetica", 15, "bold")   # the number on a meter
FONT_STATE = ("Helvetica", 22, "bold") # OPERATE / STANDBY

# --- Palettes -------------------------------------------------------------
# Straight out of the dashboard's :root blocks. In the light theme the signal
# colours are darkened, because #ff9900 is barely readable on white.
DARK = {
    "bg": "#111111", "card": "#1e2228", "card2": "#191d23", "line": "#3a4049",
    "fg": "#eeeeee", "dim": "#8b95a3", "ok": "#00b33c", "warn": "#ff9900",
    "bad": "#e60000", "off": "#595959", "acc": "#0eb8c0", "btn": "#272c34",
    "btn2": "#2a2f37", "sw": "#3a4049",
}
LIGHT = {
    "bg": "#eef1f5", "card": "#ffffff", "card2": "#f5f7fa", "line": "#d2d8e0",
    "fg": "#161a1f", "dim": "#5c6674", "ok": "#079c35", "warn": "#c97a00",
    "bad": "#cc1f1a", "off": "#ccd2da", "acc": "#0a7b82", "btn": "#e7ebf0",
    "btn2": "#dfe4ea", "sw": "#b9c1cb",
}

# --- Zones, as the dashboard defines them ---------------------------------
RF_MIN, RF_MAX, RF_WARN, RF_HIGH = 0, 150, 100, 120
SWR_MIN, SWR_MAX, SWR_WARN, SWR_HIGH = 1.0, 3.0, 2.0, 2.5
T_MIN, T_MAX, T_WARN, T_HIGH = 20, 80, 50, 60
V_MIN, V_MAX, V_UV, V_PRE, V_OVA, V_OV = 10.0, 15.5, 11.0, 11.2, 14.0, 14.8
I_TRIP, I_W1, I_W2 = 24.0, 0.8, 0.9
NSEG = 36

# --- Wording --------------------------------------------------------------
L = {
    "de": {
        "title": "JUMA PA",
        "operate": "BETRIEB", "standby": "STANDBY", "tx": "SENDEN",
        "silent": "PA antwortet nicht", "nolink": "keine Verbindung",
        "lvlHdr": "Pegel", "thHdr": "Temperatur, Lüfter, Versorgung",
        "bandHdr": "Band — PA meldet", "attHdr": "Abschwächer",
        "ctlHdr": "Steuerung", "alHdr": "Alarme",
        "tTemp": "PA Temp", "tFan": "Lüfter", "tVolt": "Spannung", "tAmp": "Strom",
        "tSel": "Bandwahl der PA",
        "bOperate": "BETRIEB", "bStandby": "STANDBY", "bClear": "Alarm quittieren",
        "bAuto": "PA wählt selbst", "bMute": "Stummschalten", "bMuted": "Stumm",
        "follow": "Band der Sendefrequenz folgen",
        "hold": "BETRIEB halten — PA zurückholen, wenn sie auf STANDBY fällt",
        "fans": ["Aus", "Langsam", "Mittel", "Schnell"],
        "alarms": ["SWR zu hoch", "Überstrom", "Übertemperatur", "Überspannung",
                   "Unterspannung Vorwarnung", "Unterspannung Abschaltung"],
        "auto": "Automatik", "manual": "Manuell",
        "lang": "Sprache", "theme": "Darstellung", "pico": "Pico",
        "connTitle": "Verbindung zur PA", "connHl2": "Hermes Lite 2, Adresse",
        "connPort": "Port", "connUsb": "oder Pico direkt über USB",
        "connSearch": "Im Netz suchen", "connOk": "Verbinden",
        "connQuit": "Beenden", "connBusy": "suche …",
        "connNone": "Keine Hermes Lite 2 geantwortet.",
        "connMany": "%d Geräte geantwortet — bitte auswählen:",
        "busy": "Die HL2 funkt gerade für ein anderes Programm",
        "busyHint": "Sie antwortet auf die Suche, aber nicht auf Registerzugriffe.\nSolange eine SDR-Software den Datenstrom hält, kommt kein zweites\nProgramm an die I2C-Brücke. Die PA wird trotzdem weiter gesteuert —\ndas macht die Firmware allein. Zum Zuschauen den Pico per USB.",
        "connFail": "Keine Verbindung: %s",
        "connHint": "Leer lassen und suchen, oder die Adresse eintragen.\nÜber USB stattdessen den Gerätepfad des Pico angeben.",
        "mProxy": "USB-Proxy — USB-Port ist die serielle Schnittstelle der PA",
        "mTelem": "Telemetriezeile auf USB",
        "mProxyHint": "Im Proxy-Betrieb schweigt die Telemetrie: was dann über\nUSB läuft, ist ausschließlich der Verkehr der PA.",
        "thSystem": "System", "thLight": "Hell", "thDark": "Dunkel",
        "noBand": "Bandfolge ist aus — die PA behält ihr Band",
        "unsupported": "%s — dafür hat die PA kein Filter",
        "bandok": "Band %s", "paoff": "Kein Status von der PA",
        "nofreq": "Die HL2 hat noch keine Sendefrequenz geschickt",
        "sound": "Akustischer Alarm",
        "soundHint": "Die PA piepst nur vor Ort. Hier wiederholt sich der Ton "
                     "alle 5 s, bis er quittiert ist oder der Alarm weg ist.",
    },
    "en": {
        "title": "JUMA PA",
        "operate": "OPERATE", "standby": "STANDBY", "tx": "TRANSMIT",
        "silent": "PA is not answering", "nolink": "no link",
        "lvlHdr": "Levels", "thHdr": "Temperature, fan, supply",
        "bandHdr": "Band — as the PA reports it", "attHdr": "Attenuator",
        "ctlHdr": "Control", "alHdr": "Alarms",
        "tTemp": "PA temp", "tFan": "Fan", "tVolt": "Supply", "tAmp": "Current",
        "tSel": "The PA's band select",
        "bOperate": "OPERATE", "bStandby": "STANDBY", "bClear": "Clear alarm",
        "bAuto": "PA selects band", "bMute": "Mute", "bMuted": "Muted",
        "follow": "Follow the transmit frequency",
        "hold": "Hold OPERATE — put the PA back when it drops to STANDBY",
        "fans": ["Off", "Slow", "Medium", "Fast"],
        "alarms": ["High SWR", "Over-current", "High temperature", "High voltage",
                   "Low voltage pre-limit", "Low voltage final limit"],
        "auto": "automatic", "manual": "manual",
        "lang": "Language", "theme": "Appearance", "pico": "Pico",
        "connTitle": "Connection to the PA", "connHl2": "Hermes Lite 2, address",
        "connPort": "Port", "connUsb": "or the Pico straight over USB",
        "connSearch": "Search the network", "connOk": "Connect",
        "connQuit": "Quit", "connBusy": "searching …",
        "connNone": "No Hermes Lite 2 answered.",
        "connMany": "%d radios answered — pick one:",
        "busy": "The HL2 is busy with another program",
        "busyHint": "It answers discovery but not register reads. While SDR\nsoftware holds the stream, no second program reaches the I2C\nbridge. The PA is still being controlled — the firmware does\nthat on its own. To watch it, reach the Pico over USB.",
        "connFail": "No connection: %s",
        "connHint": "Leave empty and search, or type the address.\nOver USB give the Pico's device path instead.",
        "mProxy": "USB proxy — the USB port is the PA's serial port",
        "mTelem": "Telemetry line on USB",
        "mProxyHint": "With the proxy on, telemetry stays quiet: what goes\nover USB is then the PA's traffic and nothing else.",
        "thSystem": "System", "thLight": "Light", "thDark": "Dark",
        "noBand": "Band following is off — the PA keeps its band",
        "unsupported": "%s — the PA has no filter for that",
        "bandok": "Band %s", "paoff": "No status from the PA",
        "nofreq": "The HL2 has not sent a transmit frequency yet",
        "sound": "Audible alarm",
        "soundHint": "The PA only beeps locally. Here the tone repeats every 5 s "
                     "until it is acknowledged or the alarm goes away.",
    },
}


class Bar(tk.Canvas):
    """The dashboard's segmented bar.

    Each segment is coloured by its OWN position, not by the current value, so
    the bar shows where the zones are instead of flipping over entirely once a
    threshold is passed.
    """

    H = 26          # height of the bar itself
    TOP = 26        # where it starts, below the label row
    FOOT = 20       # room under it for the scale

    def __init__(self, master, label, lo, hi, warn, high, unit="", dec=1, **kw):
        super().__init__(master, height=self.TOP + self.H + self.FOOT,
                         highlightthickness=0, **kw)
        self.label, self.lo, self.hi = label, lo, hi
        self.warn, self.high, self.unit, self.dec = warn, high, unit, dec
        self.value = None
        self.pal = DARK
        self._last_w = 0      # NOT _w: that name belongs to tkinter.Misc
        # Only on a real width change. Redrawing on every Configure would be
        # wasteful; worse, anything in redraw() that reconfigures the widget
        # produces another Configure, and the two feed each other until the
        # event loop has no time left for the rest of the window.
        self.bind("<Configure>", self._on_configure)

    def _on_configure(self, event):
        if event.width != self._last_w:
            self._last_w = event.width
            self.redraw()

    def set(self, value):
        self.value = value
        self.redraw()

    def seg_colour(self, i):
        v = self.lo + (i + 0.5) * (self.hi - self.lo) / NSEG
        if v >= self.high:
            return self.pal["bad"]
        if v >= self.warn:
            return self.pal["warn"]
        return self.pal["ok"]

    def redraw(self):
        self.delete("all")
        # winfo_width() is 1 until the widget has been mapped, and '1 or 320'
        # is 1 - so ask for a real number, not a truthy one.
        w = self.winfo_width()
        if w <= 1:
            w = self._last_w or 320
        p = self.pal

        ok = self.value is not None
        txt = ("%." + str(self.dec) + "f") % self.value if ok else "—"
        col = p["fg"]
        if ok and self.value >= self.high:
            col = p["bad"]
        elif ok and self.value >= self.warn:
            col = p["warn"]
        elif not ok:
            col = p["dim"]

        self.create_text(2, 12, text=self.label, anchor="w",
                         fill=p["dim"], font=FONT_S)
        self.create_text(w - 2, 12, text=txt + self.unit, anchor="e",
                         fill=col, font=FONT_BIG)

        gap, top = 2, self.TOP
        sw = max(1.0, (w - (NSEG - 1) * gap) / NSEG)
        lit = 0
        if ok:
            frac = (self.value - self.lo) / float(self.hi - self.lo)
            lit = int(round(max(0.0, min(1.0, frac)) * NSEG))
        for i in range(NSEG):
            x = i * (sw + gap)
            self.create_rectangle(x, top, x + sw, top + self.H, width=0,
                                  fill=self.seg_colour(i) if i < lit else p["off"])

        y = top + self.H + 10
        for frac, val in ((0.0, self.lo), ((self.warn - self.lo) / (self.hi - self.lo), self.warn),
                          ((self.high - self.lo) / (self.hi - self.lo), self.high), (1.0, self.hi)):
            anchor = "w" if frac == 0.0 else "e" if frac == 1.0 else "center"
            self.create_text(frac * w, y, text=("%g" % val), anchor=anchor,
                             fill=p["dim"], font=FONT_XS)

    def theme(self, pal):
        self.pal = pal
        self.configure(bg=pal["card"])      # here, never inside redraw()
        self.redraw()


class Btn(tk.Label):
    """A button that takes the colours it is given.

    macOS draws tk.Button in its own style and quietly ignores bg and fg, which
    leaves white-on-white text and no way to show which band is selected. A
    Label with a click binding looks the same on all three systems and is ours
    to colour.
    """

    def __init__(self, master, text, command, pal=None, width=None, **kw):
        super().__init__(master, text=text, padx=10, pady=6, borderwidth=0,
                         highlightthickness=0, **kw)
        if width:
            self.configure(width=width)
        self.command = command
        self.pal = pal or DARK
        self.enabled = True
        self.active = False       # 'this is the current band / attenuator'
        self.bind("<Button-1>", self._click)
        self.bind("<Enter>", lambda e: self._paint(hover=True))
        self.bind("<Leave>", lambda e: self._paint())
        self._paint()

    def _click(self, _event):
        if self.enabled and self.command:
            self.command()

    def _paint(self, hover=False):
        p = self.pal
        if not self.enabled:
            self.configure(bg=p["card2"], fg=p["off"])
        elif self.active:
            self.configure(bg=p["acc"], fg=p["bg"])
        else:
            self.configure(bg=p["btn2"] if hover else p["btn"], fg=p["fg"])

    def state(self, enabled=None, active=None):
        if enabled is not None:
            self.enabled = enabled
        if active is not None:
            self.active = active
        self._paint()

    def theme(self, pal):
        self.pal = pal
        self._paint()


class Dial(tk.Canvas):
    """The dashboard's 180 degree dial: a coloured arc, a needle, a number.

    zones is a list of (fraction, colour) running left to right, the same shape
    the dashboard passes to mkGauge.
    """

    W, H = 120, 100

    def __init__(self, master, label, zones, unit="", **kw):
        super().__init__(master, width=self.W, height=self.H,
                         highlightthickness=0, **kw)
        self.label, self.zone_spec, self.unit = label, zones, unit
        self.frac, self.text, self.colour_key = None, "—", "fg"
        self.pal = DARK

    def set(self, frac, text, colour_key="fg"):
        self.frac = None if frac is None else max(0.0, min(1.0, frac))
        self.text, self.colour_key = text, colour_key
        self.redraw()

    def redraw(self):
        self.delete("all")
        p = self.pal
        cx, cy, r = self.W / 2.0, 66.0, 40.0
        box = (cx - r, cy - r, cx + r, cy + r)

        prev = 0.0
        for frac, key in self.zone_spec:
            start = 180.0 - prev * 180.0
            extent = -(frac - prev) * 180.0
            if abs(extent) > 0.01:
                self.create_arc(*box, start=start, extent=extent, style="arc",
                                width=9, outline=p[key])
            prev = frac

        if self.frac is not None:
            a = math.radians(180.0 - self.frac * 180.0)
            self.create_line(cx, cy, cx + (r - 13) * math.cos(a),
                             cy - (r - 13) * math.sin(a),
                             width=3, fill=p["fg"], capstyle="round")
        self.create_oval(cx - 3, cy - 3, cx + 3, cy + 3, width=0, fill=p["fg"])
        self.create_text(cx, 12, text=self.label, fill=p["dim"],
                         font=FONT_S)
        self.create_text(cx, cy + 20, text=self.text + self.unit,
                         fill=p[self.colour_key],
                         font=FONT_BIG)

    def theme(self, pal):
        self.pal = pal
        self.configure(bg=pal["card"])      # here, never inside redraw()
        self.redraw()
