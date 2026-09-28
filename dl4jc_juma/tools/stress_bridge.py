#!/usr/bin/env python3
"""Push the HL2's I2C bridge until it deadlocks, and time how long that took.

    python3 tools/stress_bridge.py [--rate 40] [--burst 1] [--minutes 60]
                                   [--usb /dev/cu.usbmodem1101] [--hl2 IP]

The expansion bus has deadlocked three times here: 31 minutes with the window
polling in bursts of eight, 6 h 33 with one command per 150 ms, and once
overnight before any of this was measured. Waiting hours for the next one is no
way to find out whether a change helped, so provoke it instead.

What it varies, because these are the two things that plausibly matter:

  --rate    commands per second. The window does about 5; the bridge will take a
            great deal more.
  --burst   how many go out back to back before the next pause. 1 is the shape
            N2ADR's tool uses and the shape the window uses now; 8 was the old
            snapshot round, and 24 was a snapshot round that had to be repeated.

Nothing here writes to the radio. Every command is a four byte read of the
Pico's registers, which is what the window does all day - this only does it
faster and in a chosen rhythm.

The Pico's USB port is the witness, and it is the only one that still works when
the bus is gone: the firmware reports `wr` (register writes it has seen, frozen
when nothing arrives), `idle`, the `sda`/`scl` pad levels and `rec`, the number
of times it has tried to clock the bus free. Running that in parallel costs the
bridge nothing - different wire, different protocol.

Close the JUMA PA window first. The bridge serves one caller, and two of them
just take turns failing.
"""

import argparse
import os
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import juma_link as jl          # noqa: E402

REG_LINK = 0x42                 # any register will do; this one is cheap to read


class UsbWitness(threading.Thread):
    """Reads the firmware's telemetry line in the background.

    A thread rather than an occasional read, because the line arrives once a
    second whether anybody is listening or not and the interesting moment is
    whichever one happens to be last before the bus goes.
    """

    daemon = True

    def __init__(self, port):
        super().__init__()
        self.port = port
        self.fields = {}
        self.error = None
        self._stop = threading.Event()

    def run(self):
        try:
            import serial
        except ImportError:
            self.error = "pyserial is missing: pip install pyserial"
            return
        try:
            s = serial.Serial(self.port, 115200, timeout=0.5)
            s.write(b"mode+04\r\n")     # make sure telemetry is on
            s.flush()
        except Exception as e:          # noqa: BLE001 - any failure is the same here
            self.error = str(e)
            return
        while not self._stop.is_set():
            try:
                line = s.readline()
            except Exception as e:      # noqa: BLE001
                self.error = str(e)
                break
            if not line.startswith(b"JUMA "):
                continue
            f = {}
            for kv in line.split()[1:]:
                if b"=" in kv:
                    k, v = kv.split(b"=", 1)
                    f[k.decode()] = v.decode()
            self.fields = f
        try:
            s.close()
        except Exception:               # noqa: BLE001
            pass

    def stop(self):
        self._stop.set()

    def summary(self):
        f = self.fields
        if self.error:
            return "USB: %s" % self.error
        if not f:
            return "USB: nothing yet"
        return ("wr=%s idle=%ss sda=%s scl=%s slow=%s rec=%s"
                % (f.get("wr", "?"), f.get("idle", "?"), f.get("sda", "?"),
                   f.get("scl", "?"), f.get("slow", "?"), f.get("rec", "?")))

    def bus_is_dead(self):
        """No write has arrived for a while - the firmware's own verdict."""
        try:
            return int(self.fields.get("idle", "0")) >= 30
        except ValueError:
            return False

    def bus_is_back(self):
        try:
            return int(self.fields.get("idle", "999")) < 5
        except ValueError:
            return False


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--rate", type=float, default=40.0,
                    help="commands per second (default 40)")
    ap.add_argument("--burst", type=int, default=1,
                    help="commands back to back before the next pause")
    ap.add_argument("--minutes", type=float, default=60.0,
                    help="give up after this long (default 60)")
    ap.add_argument("--usb", default="/dev/cu.usbmodem1101",
                    help="the Pico's serial port, or '' for none")
    ap.add_argument("--hl2", default=None, help="the radio's address")
    ap.add_argument("--port", type=int, default=jl.CMD_PORT)
    args = ap.parse_args()

    ip = args.hl2
    if not ip:
        found = jl.Hl2Link.discover(timeout=1.5)
        if not found:
            raise SystemExit("no Hermes Lite 2 answered")
        ip, args.port = found[0].ip, found[0].port
    print("radio %s:%d, %.0f commands/s in bursts of %d, up to %.0f minutes"
          % (ip, args.port, args.rate, args.burst, args.minutes))

    witness = None
    if args.usb:
        witness = UsbWitness(args.usb)
        witness.start()
        time.sleep(1.5)
        print("witness: %s" % witness.summary())

    # settle=0: the pacing here is the point, so it must not be done twice.
    link = jl.Hl2Link(ip, args.port, settle=0.0)
    gap = args.burst / args.rate

    t0 = time.time()
    deadline = t0 + args.minutes * 60
    sent = errors = 0
    last_report = t0
    dead_since = None

    try:
        while time.time() < deadline:
            for _ in range(args.burst):
                try:
                    link.read4(REG_LINK)
                    sent += 1
                except jl.LinkError:
                    errors += 1
            time.sleep(gap)

            now = time.time()
            if now - last_report >= 10:
                last_report = now
                print("%6.1f min  sent %-7d errors %-6d %s"
                      % ((now - t0) / 60, sent, errors,
                         witness.summary() if witness else ""))

            if witness and witness.bus_is_dead():
                if dead_since is None:
                    dead_since = now
                    print("\n*** bus gone after %.1f minutes, %d commands ***"
                          % ((now - t0) / 60, sent))
                    print("    %s" % witness.summary())
                    print("    watching whether the firmware gets it back ...")
                elif witness.bus_is_back():
                    print("\n*** back after %.0f s - the recovery worked ***"
                          % (now - dead_since))
                    print("    %s" % witness.summary())
                    break
                elif now - dead_since > 300:
                    print("\n*** still gone after 5 minutes ***")
                    print("    %s" % witness.summary())
                    break
    except KeyboardInterrupt:
        print("\nstopped by hand")

    dt = (time.time() - t0) / 60
    print("\n%.1f minutes, %d commands, %d errors, %d torn sets of %d rounds"
          % (dt, sent, errors, link.retries, link.rounds))
    if witness:
        print("witness: %s" % witness.summary())
        witness.stop()
    link.close()


if __name__ == "__main__":
    main()
