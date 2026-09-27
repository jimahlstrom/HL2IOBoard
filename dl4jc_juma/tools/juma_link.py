"""Talking to the JUMA firmware on a Hermes Lite 2 IO board.

Two ways in, same interface out:

  Hl2Link     over the HL2's own I2C bridge, on the network. No extra cable,
              but it shares the radio's command port with the SDR software.
  UsbLink     over the Pico's USB port. Needs the cable, conflicts with nothing,
              and carries the PA's status line at full precision.

Nothing here imports tkinter, so it is usable from a script or a notebook.
The HL2 side needs only the standard library; the USB side needs pyserial.
"""

import collections
import errno
import platform
import re
import select
import sys
import socket
import struct
import time

# --- The registers, as hl2io/juma_pa/juma_regs.h defines them --------------
REG_FAULT = 0x08
REG_CONTROL = 0x05

REG_MODE = 0x40
REG_CMD = 0x41
REG_LINK = 0x42
REG_FLAGS = 0x43
REG_BAND = 0x44
REG_WANT_BAND = 0x45
REG_GAIN = 0x46
REG_ALARMS = 0x47
REG_SWR = 0x48
REG_VOLTS = 0x49
REG_AMPS = 0x4A
REG_TEMP = 0x4B
REG_WATTS_MSB = 0x4C
REG_WATTS_LSB = 0x4D
REG_FAN = 0x4E
REG_REPLIES = 0x4F
REG_BADLINES = 0x50
REG_SET_GAIN = 0x51
REG_SET_BAND = 0x52
REG_LOST = 0x53
# the same measurements at the PA's own precision, 16 bit each
REG_JUMA_VOLTS100_MSB = 0x54
REG_JUMA_AMPS100_MSB = 0x56
REG_JUMA_WATTS10_MSB = 0x58
REG_JUMA_SWR100_MSB = 0x5A
REG_JUMA_BANNER_IDX = 0x5C
REG_JUMA_BANNER_CH = 0x5D
REG_SNAP = 0x5F
SNAP_BASE = 0x60
SNAP_GROUPS = 7
# stamp = (generation << 3) | group number. The group number is what catches a
# dropped read that hands back a different group of the SAME snapshot - a shared
# tag alone would let that through.
def snap_gen(stamp):
    return stamp >> 3

def snap_group(stamp):
    return stamp & 0x07

MODE_NO_BAND = 0x01
MODE_HOLD_OPERATE = 0x02
MODE_TELEMETRY = 0x04
MODE_PROXY = 0x08

CMD_OPERATE = 1
CMD_STANDBY = 2
CMD_AUTO_BAND = 3
CMD_CLR_ALARM = 4
CMD_POWER_OFF = 0xA0
CMD_POWER_SAVE = 0xA1

FLAG_OPERATE = 0x01
FLAG_AUTO_SEL = 0x02
FLAG_PA_TX = 0x04
FLAG_CELSIUS = 0x08
FLAG_FOLLOWING = 0x10
FLAG_HL2_TX = 0x20

FAULT_NO_LINK = 0x01
FAULT_ALARM = 0x02
FAULT_BAD_CMD = 0x04

ALARM_NAMES = [
    "high SWR", "over-current", "high temperature",
    "high voltage", "low voltage pre-limit", "low voltage final limit",
]

BAND_NAMES = {
    0: "-", 1: "160m", 2: "80m", 3: "40m", 4: "30m", 5: "20m",
    6: "17m", 7: "15m", 8: "12m", 9: "10m", 10: "?",
}


class Status:
    """What the PA and the firmware say, in one place.

    The measurements are None when nothing has been received yet, so a caller
    can tell 'no reading' from 'zero'.
    """

    def __init__(self):
        self.link = False          # the values below are fresh
        self.operate = False
        self.auto_sel = False      # the PA selects bands itself
        self.pa_tx = False
        self.hl2_tx = False
        self.celsius = True
        self.following = False     # the firmware follows the transmit frequency
        self.band = 0
        self.want_band = 0
        self.gain = 0
        self.alarms = 0
        self.swr = None
        self.volts = None
        self.amps = None
        self.watts = None
        self.temp = None
        self.fan = 0
        self.mode = 0
        self.fault = 0
        self.replies = 0
        self.badlines = 0
        self.lost = 0

    @property
    def band_name(self):
        return BAND_NAMES.get(self.band, "?")

    @property
    def want_band_name(self):
        return BAND_NAMES.get(self.want_band, "?")

    def alarm_list(self):
        return [n for i, n in enumerate(ALARM_NAMES) if self.alarms & (1 << i)]


def parse_pa_line(line, st):
    """The PA's own status reply, 13 fields separated by ':'.

    The same rules as juma_status.cpp: field 1 is O or S, field 13 is
    hexadecimal. Returns True when the line was a usable status reply.
    """
    f = line.strip().split(":")
    if len(f) < 13:
        return False
    if f[0].strip()[:1] not in ("O", "S"):
        return False
    try:
        st.operate = f[0].strip()[0] == "O"
        st.auto_sel = f[1].strip()[:1] == "A"
        st.pa_tx = f[2].strip()[:1] == "T"
        st.celsius = f[3].strip()[:1] == "C"
        st.band = int(f[4])
        st.gain = int(f[5])
        st.swr = float(f[6])
        st.volts = float(f[7])
        st.amps = float(f[8])
        st.watts = float(f[9])
        st.temp = int(f[10])
        st.fan = int(f[11])
        st.alarms = int(f[12].strip(), 16)      # HEX, not decimal
    except ValueError:
        return False
    return True


# Port 1025, and that is not a guess: the maintainer's own tool goes out of its
# way to avoid 1024. n2adr_ioboard.pyw replaces the discovery function wholesale
#
#     def no_port_1024_discover(ifaddr=None, verbose=2):
#       return hermeslite.discover_by_port(ifaddr, 1025, verbose)
#     hermeslite.discover = no_port_1024_discover
#
# and connects to a known address the same way, (ip, 1025). The name says the
# intent. 1024 is where the SDR software's data stream lives, and a second
# program on it is a second program in the way.
#
# Both ports answer discovery - measured on a gateware 74.2, a datagram sent to
# 1024 is answered FROM 1025 - so the reply's source port cannot settle it
# either. 1025 is tried first and 1024 after it, and whichever answers is kept
# for the session, so a radio that wants the other one still works.
CMD_PORT = 1025          # what a link uses when nobody says otherwise
PORTS = (1025, 1024)     # everything worth trying, in the order to try it


class Radio(collections.namedtuple("Radio", "ip port mac gateware")):
    """One Hermes Lite 2 that answered.

    The address alone is no use when there are two of them on the bench, so the
    discovery reply's MAC and gateware version come along: they are in the
    packet anyway.
    """

    def __str__(self):
        return "%s:%d  MAC %s  gateware %s" % (self.ip, self.port, self.mac,
                                               self.gateware)


def decode_discovery(data, addr):
    """A 60 byte reply to the discovery datagram, or None."""
    if len(data) != 60 or data[0:2] != b"\xef\xfe":
        return None
    mac = "%02x:%02x:%02x:%02x:%02x:%02x" % struct.unpack("BBBBBB", data[3:9])
    gateware = "%d.%d" % (data[0x09], data[0x15])
    # addr[1] is the port the reply came from, which is where this radio has a
    # socket open - the first thing worth trying, not the last word. Hl2Link
    # falls back to the others if it stays silent.
    return Radio(addr[0], addr[1], mac, gateware)


class LinkError(Exception):
    pass


def trace(text):
    """Into the log the window keeps, so a search that finds nothing can say
    what it actually tried."""
    try:
        sys.stderr.write("  %s\n" % text)
        sys.stderr.flush()
    except (OSError, ValueError, AttributeError):
        pass


# Set when the system refuses to let datagrams onto the local network, which on
# macOS is how a missing local-network permission shows itself: EHOSTUNREACH,
# "No route to host", on address after address. Worth telling apart from a radio
# that is simply switched off, because the two need completely different things
# from whoever is reading.
blocked_locally = False


def local_network_hint():
    """macOS asks an application for permission before it may speak to the
    local network, and refuses in a way that reads like a routing fault. Worth
    naming, because nothing about the message points at a checkbox."""
    if platform.system() != "Darwin":
        return ""
    if blocked_locally:
        return ("\nThe system refused these packets: 'no route to host' on "
                "address after address.\nThat is the local network permission, "
                "not the network. Open System Settings >\nPrivacy & Security > "
                "Local Network and switch this program on.")
    return ("\nOn macOS, check System Settings > Privacy & Security > "
            "Local Network and allow this program.")


def note_send_error(e):
    global blocked_locally
    if getattr(e, "errno", None) in (errno.EHOSTUNREACH, errno.EPERM,
                                     errno.EACCES):
        blocked_locally = True


# --- Over the HL2's I2C bridge ---------------------------------------------
class Hl2Link:
    """Reach the Pico through the Hermes Lite 2.

    The HL2 accepts a command packet that it turns into an I2C transaction on
    its expansion bus, which is where the IO board sits. Command address 0x3d
    selects that bus (gateware, i2c_bus2.v: en_i2c2_next = cmd_addr == 6'h3d);
    0x3c would be the internal clock bus.

    A read returns four bytes, the register addressed and the three after it -
    which is why the firmware keeps values that belong together adjacent.
    """

    I2C_ADDR = 0x1D          # the Pico, per the IO board documentation
    CMD_BUS2 = 0x3D
    OP_WRITE = 0x06
    OP_READ = 0x07

    # How long to leave the bridge alone after a command.
    #
    # hermeslite.py uses 0.2 s, and it is tempting to read that as the time the
    # I2C transaction needs. Measured against a real HL2, it is not: the drops
    # are random, not a timing threshold. 0.2 s gave 1 bad round in 25, 0.35 s
    # gave none, 0.5 s gave 1 again - and with the snapshot stamps doing the
    # detecting, even 5 ms came through 30 rounds with one retry and no failure.
    #
    # So this is chosen for throughput, not for safety: what makes a reading
    # trustworthy is the stamp check in _read_snapshot(), not this number. 20 ms
    # puts a round at about 0.2 s while leaving the radio's command path mostly
    # to itself - going faster gains little and means hammering an HL2 that has
    # a transmitter to run.
    SETTLE = 0.02

    # After a command goes unanswered, leave the bridge alone for a while
    # instead of coming straight back. A silent bridge means it is busy with
    # the radio's own traffic, and the gateware drops what arrives while it is
    # busy - including its own write to the filter board. Asking harder at that
    # moment is the one thing that cannot help.
    #
    # It also shortens a failed round from seconds to nothing: the first
    # command pays the retries, the rest of the round is refused without a
    # packet, and the window says so instead of freezing while it waits.
    COOL_BASE = 1.0                 # doubles per consecutive failure
    COOL_MAX  = 8.0

    def __init__(self, ip, port=CMD_PORT, settle=None, timeout=1.0):
        self.ip = ip
        self.port = port
        self.settle = self.SETTLE if settle is None else settle
        self.timeout = timeout
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.setblocking(False)
        self._fan_seen = 0
        self._mode_seen = 0
        self._fault_seen = 0
        self._last_gen = None
        self.rounds = 0          # snapshots asked for
        self.retries = 0         # times a set had to be read again
        self._cool_until = 0.0   # no packets before this
        self._fails = 0          # consecutive unanswered commands
        # Ports still worth trying, the one asked for first. Emptied by the
        # first answer, so the search happens once per link and not per command.
        self._untried = [p for p in PORTS if p != self.port]

    @staticmethod
    def local_addresses():
        """This machine's IPv4 addresses, best effort and without extra modules."""
        addrs = set()
        try:
            for r in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
                addrs.add(r[4][0])
        except (socket.gaierror, OSError):
            pass
        # The address the default route would use. connect() on a datagram
        # socket sends nothing, it only picks the route.
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            s.connect(("8.8.8.8", 1))
            addrs.add(s.getsockname()[0])
        except OSError:
            pass
        finally:
            s.close()
        return sorted(a for a in addrs if not a.startswith("127."))

    @staticmethod
    def discover(timeout=1.0):
        """Broadcast for HL2s. Returns a list of (ip, port).

        Sent once per local address, not just once. A machine with a dock, a
        WLAN and a couple of virtual-machine bridges has several interfaces, and
        a datagram to 255.255.255.255 leaves through exactly one of them -
        whichever the routing table fancies, which on this sort of machine is
        rarely the one the radio is on. Binding the socket to an address first
        is what decides the way out.
        """
        found = []
        sources = [None] + Hl2Link.local_addresses()
        for port in PORTS:
            for src in sources:
                s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
                s.setblocking(False)
                try:
                    if src:
                        s.bind((src, 0))
                    s.sendto(bytes([0xEF, 0xFE, 0x02] + [0] * 57),
                             ("255.255.255.255", port))
                    end = time.time() + timeout
                    while time.time() < end:
                        left = max(0.0, end - time.time())
                        if not select.select([s], [], [], left)[0]:
                            break
                        r = decode_discovery(*s.recvfrom(60))
                        if r and r not in found:
                            found.append(r)
                except OSError as e:
                    note_send_error(e)
                    trace("broadcast from %s port %d: %s" % (src or "any", port, e))
                finally:
                    s.close()
            if found:
                break
        trace("broadcast found %d" % len(found))
        if not found:
            trace("no broadcast answer, knocking on every address instead")
            found = Hl2Link.sweep(timeout=timeout)
            trace("sweep found %d" % len(found))
        return Hl2Link._unique(found)

    @staticmethod
    def _unique(found):
        """One entry per radio. The same unit answers on 1024 and on 1025, and
        a machine with two interfaces on one subnet hears it twice."""
        out = []
        seen = set()
        for r in found:
            if r.mac in seen:
                continue
            seen.add(r.mac)
            out.append(r)
        return out

    @staticmethod
    def sweep(timeout=1.0):
        """Ask every address on the local /24 directly.

        macOS gates broadcast and multicast behind the local-network
        permission, and does not always get round to asking for it - measured
        here: the broadcast came back empty from the application while a
        unicast to the same radio connected at once. So when nobody answers the
        broadcast, knock on every door instead. It is 254 small datagrams per
        interface and it needs no permission.

        A /24 is an assumption. It is the usual thing on a home network, and
        the address can always be typed in.
        """
        found = []
        for src in Hl2Link.local_addresses():
            net = src.rsplit(".", 1)[0]
            for port in PORTS:
                s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                s.setblocking(False)
                msg = bytes([0xEF, 0xFE, 0x02] + [0] * 57)
                sent = errs = 0
                try:
                    s.bind((src, 0))
                    for host in range(1, 255):
                        ip = "%s.%d" % (net, host)
                        if ip == src:
                            continue
                        try:
                            s.sendto(msg, (ip, port))
                            sent += 1
                        except OSError as e:
                            note_send_error(e)
                            if not errs:
                                trace("sweep send to %s: %s" % (ip, e))
                            errs += 1
                    trace("sweep %s.x port %d: %d sent, %d refused"
                          % (net, port, sent, errs))
                    end = time.time() + timeout
                    while time.time() < end:
                        left = max(0.0, end - time.time())
                        if not select.select([s], [], [], left)[0]:
                            break
                        r = decode_discovery(*s.recvfrom(60))
                        if r and r not in found:
                            found.append(r)
                except OSError as e:
                    trace("sweep from %s port %d: %s" % (src, port, e))
                finally:
                    s.close()
                if found:
                    return found
        return found

    def _drain(self):
        """Throw away anything already queued.

        Those packets answer an earlier command, and taking one for the answer
        to this one shifts every value by a whole read.
        """
        while select.select([self.sock], [], [], 0)[0]:
            try:
                self.sock.recvfrom(60)
            except OSError:
                break

    def _command(self, cmd4):
        """Send one command packet and return its 60-byte response."""
        left = self._cool_until - time.time()
        if left > 0:
            raise LinkError("the HL2 did not answer - leaving the bridge alone "
                            "for another %.1f s" % left)

        msg = bytes([0xEF, 0xFE, 0x05, 0x7F, self.CMD_BUS2 << 1]) + cmd4 + bytes(51)

        # The port asked for first, then the ones not tried yet. Only the first
        # pass pays for this: an answer empties the list.
        while True:
            data = self._attempt(msg)
            if data is not None:
                self._fails = 0
                self._cool_until = 0.0
                self._untried = []
                time.sleep(self.settle)
                return data
            if not self._untried:
                break
            self.port = self._untried.pop(0)
            trace("no answer - trying port %d" % self.port)

        self._fails += 1
        self._cool_until = time.time() + min(
            self.COOL_MAX, self.COOL_BASE * (2 ** (self._fails - 1)))
        raise LinkError("no answer from the HL2 at %s:%d%s"
                        % (self.ip, self.port, local_network_hint()))

    def _attempt(self, msg):
        """Send to self.port up to three times. The 60-byte reply, or None."""
        self._drain()
        for _ in range(3):
            try:
                self.sock.sendto(msg, (self.ip, self.port))
            except OSError as e:
                # EHOSTUNREACH and EPERM are what macOS hands back when an app
                # has not been allowed onto the local network, and the wording
                # ("no route to host") sends people looking at their router.
                note_send_error(e)
                raise LinkError("%s%s" % (e, local_network_hint()))
            if select.select([self.sock], [], [], self.timeout)[0]:
                data, addr = self.sock.recvfrom(60)
                if len(data) == 60 and data[0:2] == b"\xef\xfe":
                    return data
        return None

    def write(self, reg, value):
        self._command(bytes([self.OP_WRITE, 0x80 | self.I2C_ADDR,
                             reg & 0xFF, value & 0xFF]))

    def read4(self, reg):
        """The register and the three after it."""
        r = self._command(bytes([self.OP_READ, 0x80 | self.I2C_ADDR,
                                 reg & 0xFF, 0x00]))
        # response_data sits at 0x17..0x1b, big endian. The gateware fills it
        # first byte read in bits 7:0, then 15:8, 23:16, 31:24.
        d = struct.unpack("!L", r[0x17:0x1B])[0]
        return [d & 0xFF, (d >> 8) & 0xFF, (d >> 16) & 0xFF, (d >> 24) & 0xFF]

    def read_status(self, slow_fields=True):
        """One picture of the PA, proved coherent.

        Ask the firmware for a snapshot, then read the seven groups. Every group
        carries the same tag, so a read the bridge dropped - which comes back as
        an older group - shows up as a tag that does not match, and we try again.
        Without this about one round in 25 is silently shifted; measured, not
        feared.

        slow_fields is ignored here: a snapshot costs the same either way.
        """
        self.rounds += 1
        for attempt in range(3):
            st = self._read_snapshot()
            if st is not None:
                return st
            self.retries += 1
        raise LinkError("the HL2 keeps dropping reads - no coherent snapshot")

    def _read_snapshot(self):
        """One attempt. None when the groups did not agree."""
        self.write(REG_SNAP, 1)
        groups = [self.read4(SNAP_BASE + 4 * i) for i in range(SNAP_GROUPS)]
        stamps = [g[0] for g in groups]
        if all(s == 0 for s in stamps):    # firmware without the snapshot block
            return self._read_direct()
        # every group has to be the group we asked for ...
        if any(snap_group(s) != i for i, s in enumerate(stamps)):
            return None
        # ... out of one and the same snapshot ...
        gens = set(snap_gen(s) for s in stamps)
        if len(gens) != 1:
            return None
        gen = gens.pop()
        if gen == 0:
            return None
        # ... and not the snapshot we already had.
        if gen == self._last_gen:
            return None
        self._last_gen = gen

        st = Status()
        (_, link, flags, band) = groups[0]
        (_, want, gain, alarms) = groups[1]
        (_, v_hi, v_lo, temp) = groups[2]
        (_, a_hi, a_lo, fan) = groups[3]
        (_, w_hi, w_lo, mode) = groups[4]
        (_, s_hi, s_lo, fault) = groups[5]
        (_, replies, bad, lost) = groups[6]

        st.link = link == 1
        st.operate = bool(flags & FLAG_OPERATE)
        st.auto_sel = bool(flags & FLAG_AUTO_SEL)
        st.pa_tx = bool(flags & FLAG_PA_TX)
        st.celsius = bool(flags & FLAG_CELSIUS)
        st.following = bool(flags & FLAG_FOLLOWING)
        st.hl2_tx = bool(flags & FLAG_HL2_TX)
        st.band, st.want_band, st.gain, st.alarms = band, want, gain, alarms
        st.volts = ((v_hi << 8) | v_lo) / 100.0
        st.amps = ((a_hi << 8) | a_lo) / 100.0
        st.watts = ((w_hi << 8) | w_lo) / 10.0
        st.swr = ((s_hi << 8) | s_lo) / 100.0
        st.temp = temp - 256 if temp > 127 else temp
        st.fan, st.mode, st.fault = fan, mode, fault
        st.replies, st.badlines, st.lost = replies, bad, lost
        return st

    def _read_direct(self, slow_fields=True):
        """The old way, register by register. Only for firmware without the
        snapshot block - it cannot tell a dropped read from a good one."""
        st = Status()
        link, flags, band, want = self.read4(REG_LINK)
        gain, alarms, swr, volts = self.read4(REG_GAIN)
        amps, temp, w_msb, w_lsb = self.read4(REG_AMPS)
        # volts x 100 and amps x 100, the PA's own precision
        v_hi, v_lo, a_hi, a_lo = self.read4(REG_JUMA_VOLTS100_MSB)
        w_hi, w_lo, s_hi, s_lo = self.read4(REG_JUMA_WATTS10_MSB)
        if slow_fields:
            fan, replies, bad, lost = self.read4(REG_FAN)
            mode, cmd, _l2, _f2 = self.read4(REG_MODE)
            fault = self.read4(REG_FAULT)[0]
            st.fan, st.replies, st.badlines, st.lost = fan, replies, bad, lost
            st.mode, st.fault = mode, fault
        else:
            st.fan = self._fan_seen
            st.mode, st.fault = self._mode_seen, self._fault_seen

        st.link = link == 1
        st.operate = bool(flags & FLAG_OPERATE)
        st.auto_sel = bool(flags & FLAG_AUTO_SEL)
        st.pa_tx = bool(flags & FLAG_PA_TX)
        st.celsius = bool(flags & FLAG_CELSIUS)
        st.following = bool(flags & FLAG_FOLLOWING)
        st.hl2_tx = bool(flags & FLAG_HL2_TX)
        st.band, st.want_band, st.gain, st.alarms = band, want, gain, alarms
        st.temp = temp - 256 if temp > 127 else temp

        # The fine registers when they carry something, the tenths otherwise -
        # an older image would leave the fine ones at zero.
        fine_v = (v_hi << 8) | v_lo
        fine_a = (a_hi << 8) | a_lo
        fine_w = (w_hi << 8) | w_lo
        fine_s = (s_hi << 8) | s_lo
        st.volts = fine_v / 100.0 if fine_v else volts / 10.0
        st.amps = fine_a / 100.0 if fine_a else amps / 10.0
        st.watts = fine_w / 10.0 if fine_w else float((w_msb << 8) | w_lsb)
        st.swr = fine_s / 100.0 if fine_s else swr / 10.0

        self._fan_seen, self._mode_seen, self._fault_seen = st.fan, st.mode, st.fault
        return st

    def read_banner(self):
        """What the PA called itself at power-up, a character at a time."""
        self.write(REG_JUMA_BANNER_IDX, 0)
        n = self.read4(REG_JUMA_BANNER_IDX)[1]
        out = []
        for i in range(1, min(n, 47) + 1):
            self.write(REG_JUMA_BANNER_IDX, i)
            out.append(chr(self.read4(REG_JUMA_BANNER_IDX)[1]))
        return "".join(out)

    def close(self):
        self.sock.close()

    def describe(self):
        return "HL2 %s:%d" % (self.ip, self.port)


# --- Over the Pico's USB port ---------------------------------------------
_TEL = re.compile(rb"^JUMA ")


class UsbLink:
    """Reach the Pico over USB.

    Reads the telemetry line the firmware emits once a second, whose 'raw='
    field is the PA's status line unchanged - so the readings keep the PA's own
    precision instead of the registers' tenths. Writes go through the same
    console: '=O' and friends for the PA, 'mode=' and 'reg=' for the registers.

    In an image with JUMA_MODE_PROXY set, the PA's lines arrive bare instead;
    those are read too, and then the firmware's own fields stay at their
    defaults.
    """

    # The Pico's USB identity. Raspberry Pi's vendor id; the product id is
    # whatever the SDK's stdio_usb hands out, so only the vendor is matched.
    # Matching it is what keeps a probe off a GPS or a modem: those get neither
    # an open nor a written line.
    PICO_VID = 0x2E8A

    @staticmethod
    def ports():
        """Serial ports that could be a Pico running this firmware.

        Nothing here may raise. Enumerating ports is a convenience on the way
        to the network route, and on macOS it goes through IOKit by ctypes -
        which PyInstaller says out loud it cannot follow into a bundle
        ("only basenames are supported with ctypes imports"). So a frozen app
        can fail here in ways a script never does, and the answer to all of
        them is the same: no ports, take the network.
        """
        try:
            from serial.tools import list_ports
            out = []
            for p in list_ports.comports():
                if getattr(p, "vid", None) == UsbLink.PICO_VID:
                    out.append(p.device)
            return out
        except Exception as e:
            trace("USB: cannot list ports (%s)" % e)
            return []

    @staticmethod
    def find(timeout=2.5):
        """The first port that answers as this firmware, or None.

        Worth preferring over the HL2's I2C bridge whenever the cable is there:
        the bridge is shared with the radio's own traffic, and every command
        that goes over it is a command the gateware's filter board write can be
        dropped behind. Over USB the watching costs the bus nothing.

        The proof asked for is a telemetry line. An open alone proves nothing -
        any CDC device opens - and the firmware may have come up in proxy mode,
        which the constructor steps out of.
        """
        for dev in UsbLink.ports():
            link = None
            try:
                link = UsbLink(dev)
                end = time.time() + timeout
                while time.time() < end:
                    link.pump()
                    if link.saw_firmware:
                        trace("USB: %s answered" % dev)
                        return link
                trace("USB: %s opened but said nothing in %.1f s" % (dev, timeout))
            except (LinkError, OSError) as e:
                trace("USB: %s - %s" % (dev, e))
            if link:
                link.close()
        return None

    def __init__(self, port, baud=115200, timeout=0.2):
        try:
            import serial
        except ImportError:
            raise LinkError("pyserial is missing: pip install pyserial")
        self.ser = serial.Serial(port, baud, timeout=timeout)
        self.port = port
        self._st = Status()
        # Set by _take_telemetry(): a line only this firmware sends. find()
        # waits for it rather than trusting that the port opened.
        self.saw_firmware = False
        # Ask for the telemetry feed, and step out of proxy mode if the image
        # came up in it - but with '+' and '-', so that whatever else the mode
        # holds, the OPERATE hold in particular, is left alone.
        self._write_line("mode-%02X" % MODE_PROXY)
        self._write_line("mode+%02X" % MODE_TELEMETRY)

    def _write_line(self, s):
        self.ser.write((s + "\r\n").encode("ascii"))
        self.ser.flush()

    def pump(self):
        """Read whatever has arrived. Returns True if the status changed."""
        changed = False
        while True:
            try:
                line = self.ser.readline()
            except Exception as e:
                raise LinkError(str(e))
            if not line:
                break
            if _TEL.match(line):
                changed |= self._take_telemetry(line)
            else:
                # A bare status line: proxy mode, or the trace of a debug image.
                if parse_pa_line(line.decode("ascii", "replace"), self._st):
                    self._st.link = True
                    changed = True
        return changed

    def _take_telemetry(self, line):
        self.saw_firmware = True
        fields = {}
        for kv in line.split()[1:]:
            if b"=" in kv:
                k, v = kv.split(b"=", 1)
                fields[k.decode()] = v
        st = self._st
        raw = fields.get("raw", b"").decode("ascii", "replace")
        if raw:
            parse_pa_line(raw, st)
        st.link = fields.get("link") == b"1"
        st.mode = int(fields.get("mode", b"0"), 16)
        st.fault = int(fields.get("fault", b"0"), 16)
        st.want_band = int(fields.get("want", b"0"))
        st.replies = int(fields.get("rep", b"0"))
        st.badlines = int(fields.get("bad", b"0"))
        st.lost = int(fields.get("lost", b"0"))
        st.following = not (st.mode & MODE_NO_BAND)
        return True

    def read_status(self):
        self.pump()
        return self._st

    # The register writes the firmware's console understands.
    def write(self, reg, value):
        if reg == REG_MODE:
            self._write_line("mode=%02X" % (value & 0xFF))
            self._st.mode = value & 0xFF
        else:
            self._write_line("reg=%02X:%02X" % (reg & 0xFF, value & 0xFF))

    def set_mode_bits(self, bits, on):
        """Set or clear bits without having to know the rest of the byte."""
        self._write_line("mode%s%02X" % ("+" if on else "-", bits & 0xFF))

    def send_pa(self, cmd):
        """A JUMA command straight at the amplifier."""
        self._write_line(cmd)

    def close(self):
        self.ser.close()

    def describe(self):
        return "USB %s" % self.port
