"""Tests for juma_link, against a fake HL2 and a fake Pico.

    python3 hl2io/tools/test_juma_link.py

No hardware needed. The fake HL2 answers command packets the way the gateware
does - a read returns four bytes, the first one read in response_data bits 7:0 -
so the packet format and the decoding are both checked, not just the plumbing.
"""
import os, struct, socket, sys, threading, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import juma_link as jl

fails = checks = 0
def ck(cond, msg):
    global fails, checks
    checks += 1
    if not cond:
        fails += 1
        print("  FAIL", msg)

# ---------------------------------------------------------------- fake HL2
class FakeHL2(threading.Thread):
    """Answers command packets the way the gateware would: a read returns four
    bytes, first byte read in response_data bits 7:0.

    drop_every tells it to behave like the real thing when it is busy: swallow
    the command and answer with the PREVIOUS read's data anyway. That is the
    failure the snapshot tag exists to catch.
    """
    def __init__(self, regs, drop_every=0, snapshot=False):
        super().__init__(daemon=True)
        self.regs = regs
        self.drop_every = drop_every
        self.snapshot = snapshot
        self.n = 0
        self.tag = 0
        self.last_resp = 0
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.port = self.sock.getsockname()[1]
        self.seen = []
        self.stop = False

    def run(self):
        while not self.stop:
            self.sock.settimeout(0.2)
            try:
                data, addr = self.sock.recvfrom(64)
            except socket.timeout:
                continue
            self.seen.append(data)
            resp = bytearray(60)
            resp[0:2] = b"\xef\xfe"
            if len(data) >= 9 and data[2] == 0x05:
                cmd_addr = data[4] >> 1
                op, i2c, reg, val = data[5], data[6], data[7], data[8]
                self.n += 1
                dropped = self.drop_every and (self.n % self.drop_every == 0)
                if dropped:
                    pass                    # "// Missed" - and resp_data stands
                elif cmd_addr == 0x3D and op == 0x07:
                    b = [self.regs.get(reg + i, 0) for i in range(4)]
                    self.last_resp = b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)
                elif cmd_addr == 0x3D and op == 0x06:
                    self.regs[reg] = val
                    if self.snapshot and reg == jl.REG_SNAP:
                        self._snap()
            resp[0x17:0x1B] = struct.pack("!L", self.last_resp)
            self.sock.sendto(bytes(resp), addr)

    def _snap(self):
        """What take_snapshot() does in the firmware."""
        self.tag = self.tag % 31 + 1
        payload = [
            (1, self.regs[jl.REG_FLAGS], self.regs[jl.REG_BAND]),
            (self.regs[jl.REG_WANT_BAND], self.regs[jl.REG_GAIN], self.regs[jl.REG_ALARMS]),
            (1366 >> 8, 1366 & 0xFF, self.regs[jl.REG_TEMP]),
            (810 >> 8, 810 & 0xFF, self.regs[jl.REG_FAN]),
            (272 >> 8, 272 & 0xFF, self.regs[jl.REG_MODE]),
            (140 >> 8, 140 & 0xFF, self.regs[jl.REG_FAULT]),
            (self.regs[jl.REG_REPLIES], self.regs[jl.REG_BADLINES], self.regs[jl.REG_LOST]),
        ]
        for g, three in enumerate(payload):
            base = jl.SNAP_BASE + 4 * g
            self.regs[base] = (self.tag << 3) | g
            for k, v in enumerate(three):
                self.regs[base + 1 + k] = v

regs = {
    jl.REG_LINK: 1, jl.REG_FLAGS: jl.FLAG_OPERATE | jl.FLAG_CELSIUS | jl.FLAG_FOLLOWING,
    jl.REG_BAND: 5, jl.REG_WANT_BAND: 5,
    jl.REG_GAIN: 1, jl.REG_ALARMS: 0x00, jl.REG_SWR: 14, jl.REG_VOLTS: 136,
    jl.REG_AMPS: 81, jl.REG_TEMP: 26, jl.REG_WATTS_MSB: 0, jl.REG_WATTS_LSB: 37,
    jl.REG_FAN: 1, jl.REG_REPLIES: 200, jl.REG_BADLINES: 0, jl.REG_LOST: 0,
    jl.REG_MODE: jl.MODE_HOLD_OPERATE | jl.MODE_TELEMETRY, jl.REG_CMD: 0,
    jl.REG_FAULT: 0,
}
srv = FakeHL2(regs); srv.start(); time.sleep(0.1)

print("HL2 route")
link = jl.Hl2Link("127.0.0.1", srv.port, settle=0.0)
four = link.read4(jl.REG_LINK)
ck(four == [1, regs[jl.REG_FLAGS], 5, 5], "read4 gives the register and the next three: %r" % four)

pkt = srv.seen[-1]
ck(pkt[0:3] == b"\xef\xfe\x05", "packet cookie and type")
ck(pkt[3] == 0x7F, "sequence byte 0x7f")
ck(pkt[4] >> 1 == 0x3D, "command address 0x3d = I2C bus 2, got 0x%02X" % (pkt[4] >> 1))
ck(pkt[5] == 0x07, "read opcode 0x07")
ck(pkt[6] == 0x9D, "device byte 0x80|0x1D, got 0x%02X" % pkt[6])
ck(len(pkt) == 60, "packet is 60 bytes, got %d" % len(pkt))

st = link.read_status()
ck(st.link and st.operate and st.celsius and st.following, "flags decoded")
ck(st.band == 5 and st.want_band == 5, "band")
ck(abs(st.swr - 1.4) < 1e-9, "swr 14 -> 1.4, got %r" % st.swr)
ck(abs(st.volts - 13.6) < 1e-9, "volts 136 -> 13.6, got %r" % st.volts)
ck(abs(st.amps - 8.1) < 1e-9, "amps 81 -> 8.1, got %r" % st.amps)
ck(st.watts == 37.0, "watts msb/lsb -> 37, got %r" % st.watts)
ck(st.temp == 26, "temp")
ck(st.mode == (jl.MODE_HOLD_OPERATE | jl.MODE_TELEMETRY), "mode")

# negative temperature, two's complement
regs[jl.REG_TEMP] = 0xF6          # -10
ck(link.read_status().temp == -10, "temp 0xF6 -> -10, got %r" % link.read_status().temp)

link.write(jl.REG_CMD, jl.CMD_OPERATE)
ck(regs[jl.REG_CMD] == jl.CMD_OPERATE, "write reached the register")
w = srv.seen[-1]
ck(w[5] == 0x06 and w[7] == jl.REG_CMD and w[8] == jl.CMD_OPERATE, "write packet")

# timeout must raise, not hang
dead = jl.Hl2Link("127.0.0.1", 1, timeout=0.05)
try:
    dead.read4(0x40); ck(False, "a dead HL2 should raise")
except jl.LinkError:
    ck(True, "")
except OSError:
    ck(True, "")
link.close(); dead.close(); srv.stop = True

# ---------------------------------------------- the snapshot, and dropped reads
print("Snapshot, and a bridge that drops commands")
regs2 = dict(regs)
regs2[jl.REG_TEMP] = 20
srv2 = FakeHL2(regs2, snapshot=True); srv2.start(); time.sleep(0.1)
link2 = jl.Hl2Link("127.0.0.1", srv2.port, settle=0.0)

st = link2.read_status()
ck(st.link and st.operate, "snapshot: flags")
ck(st.temp == 20 and st.band == 5, "snapshot: temp %s band %s" % (st.temp, st.band))
ck(abs(st.volts - 13.66) < 1e-9, "snapshot keeps full precision: %r V" % st.volts)
ck(abs(st.amps - 8.10) < 1e-9, "amps %r" % st.amps)
ck(abs(st.watts - 27.2) < 1e-9, "watts %r" % st.watts)
ck(abs(st.swr - 1.40) < 1e-9, "swr %r" % st.swr)
ck(st.mode == regs2[jl.REG_MODE], "mode through the snapshot")

# The tag must move, or a whole stale set would pass as fresh
t1 = link2._last_gen
link2.read_status()
ck(link2._last_gen != t1, "the generation advances between snapshots")

# Now make it drop commands. Measured on the real bridge: about 4 % of rounds
# came back shifted, and a round is 8 commands, so roughly 1 in 100 commands.
# Every 40th is harsher than that and still has to be survivable.
srv2.drop_every = 40
good = 0
wrong = 0
for _ in range(25):
    try:
        st = link2.read_status()
        good += 1
        # any of these would be a shifted read getting through
        if st.temp != 20 or st.band != 5 or st.alarms != 0 or st.volts < 8:
            wrong += 1
            print("      DURCHGERUTSCHT: temp=%s band=%s alarm=%02X %.2fV"
                  % (st.temp, st.band, st.alarms, st.volts))
    except jl.LinkError:
        pass            # detected and given up on - the honest outcome
print("      %d von 25 Runden geliefert, %d davon falsch" % (good, wrong))
ck(wrong == 0, "no shifted read reaches the caller: %d slipped through" % wrong)
ck(good >= 20, "and the retries keep it usable: only %d of 25 rounds got through" % good)

# The harsh case: it must give up honestly rather than invent a reading.
srv2.drop_every = 2
raised = delivered = 0
for _ in range(6):
    try:
        st = link2.read_status()
        delivered += 1
        if st.temp != 20 or st.alarms != 0:
            ck(False, "a shifted read got through under heavy loss")
    except jl.LinkError:
        raised += 1
print("      bei jedem 2. Abwurf: %d Fehler gemeldet, %d Runden geliefert"
      % (raised, delivered))
ck(raised > 0, "under heavy loss it reports instead of guessing")
link2.close(); srv2.stop = True

# ------------------------------------------------------- fake Pico on USB
print("USB route")
class FakeSerial:
    def __init__(self, lines): self.lines = list(lines); self.written = []
    def readline(self): return self.lines.pop(0) if self.lines else b""
    def write(self, b): self.written.append(b)
    def flush(self): pass
    def close(self): pass

u = jl.UsbLink.__new__(jl.UsbLink)          # bypass opening a real port
u.ser = FakeSerial([
    b"JUMA up=312 link=1 mode=06 fault=00 want=5 rep=624 bad=0 lost=0 "
    b"raw=O:A:T:C:5:1:1.0:14.09:8.1:27.2:26:0:0\r\n",
])
u.port = "fake"
u._st = jl.Status()
st = u.read_status()
ck(st.link and st.operate and st.auto_sel and st.pa_tx and st.celsius, "telemetry flags")
ck(st.band == 5 and st.gain == 1, "telemetry band/gain")
ck(abs(st.volts - 14.09) < 1e-6, "full precision volts 14.09, got %r" % st.volts)
ck(abs(st.watts - 27.2) < 1e-6, "full precision watts 27.2, got %r" % st.watts)
ck(st.mode == 0x06 and st.want_band == 5 and st.replies == 624, "firmware fields")
ck(st.following is True, "mode 06 has no NO_BAND -> following")

# hex alarm field, the trap
u.ser = FakeSerial([b"JUMA up=1 link=1 mode=00 fault=02 want=0 rep=2 bad=0 lost=0 "
                    b"raw=S:M:R:C:10:4:2.9:13.10:10.4:96.0:64:3:A\r\n"])
st = u.read_status()
ck(st.alarms == 0x0A, "alarm 'A' is hex 0x0A, got 0x%02X" % st.alarms)
ck(st.alarm_list() == ["over-current", "high voltage"], "alarm names: %r" % st.alarm_list())

# a bare PA line, as proxy mode delivers it
u.ser = FakeSerial([b"O:M:R:C: 3:2:1.4:13.66: 0.0:  0.0: 24:1: 0\n\r"])
st = u.read_status()
ck(st.band == 3 and st.gain == 2 and not st.auto_sel, "bare status line parsed")

# writes: mode goes as 'mode=', anything else as 'reg='
u.ser = FakeSerial([])
u.write(jl.REG_MODE, 0x06)
u.write(jl.REG_SET_GAIN, 3)
u.send_pa("=C")
u.set_mode_bits(jl.MODE_TELEMETRY, True)
u.set_mode_bits(jl.MODE_PROXY, False)
ck(u.ser.written == [b"mode=06\r\n", b"reg=51:03\r\n", b"=C\r\n",
                     b"mode+04\r\n", b"mode-08\r\n"],
   "console lines: %r" % u.ser.written)

# rubbish must not raise
for junk in (b"\x00\xff garbage\n", b"JUMA raw=\n", b"O:A\n", b""):
    u.ser = FakeSerial([junk] if junk else [])
    try:
        u.read_status(); ck(True, "")
    except Exception as e:
        ck(False, "junk %r raised %s" % (junk, e))

print("\n%d checks, %d failures" % (checks, fails))
sys.exit(1 if fails else 0)
