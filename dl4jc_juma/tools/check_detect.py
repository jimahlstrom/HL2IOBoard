#!/usr/bin/env python3
"""Does the SDR software see the board, and does the band follow?

    python3 check_detect.py [ip] [port]

Close the JUMA window first: the HL2's I2C bridge serves one caller, and two
of them just take turns failing.
"""

import sys
import time

sys.path.insert(0, __file__.rsplit("/", 1)[0])
import juma_link as jl          # noqa: E402

REG_LPF_DETECT = 33
MAGIC = 0xEF


def main():
    ip = sys.argv[1] if len(sys.argv) > 1 else None
    port = int(sys.argv[2]) if len(sys.argv) > 2 else jl.CMD_PORT
    if not ip:
        found = jl.Hl2Link.discover(timeout=1.5)
        if not found:
            raise SystemExit("no Hermes Lite 2 answered")
        ip, port = found[0].ip, found[0].port
        print("found %s:%d" % (ip, port))

    link = jl.Hl2Link(ip, port)

    got = link.read4(REG_LPF_DETECT)[0]
    print("register %d reads 0x%02X — %s"
          % (REG_LPF_DETECT, got,
             "the software will recognise the board" if got == MAGIC
             else "NOT 0x%02X, so deskHPSDR will send nothing" % MAGIC))
    if got != MAGIC:
        print("  either flash the firmware that answers it, or tick")
        print("  Radio > HL2 Force IO Board in deskHPSDR")

    print()
    print("Now change band in the SDR software. 120 s.")
    print("  time   TX frequency   wanted  PA reports")
    last = None
    t0 = time.time()
    while time.time() - t0 < 120:
        try:
            # Two reads, believed only when they agree: the five frequency
            # bytes need two transactions and can tear between them.
            a1, b1 = link.read4(0x00), link.read4(0x04)
            a2, b2 = link.read4(0x00), link.read4(0x04)
            if a1 != a2 or b1[0] != b2[0]:
                continue
            hz = (a1[0] << 32) | (a1[1] << 24) | (a1[2] << 16) | (a1[3] << 8) | b1[0]
            st = link.read_status()
            now = (hz, st.want_band, st.band)
            if now != last:
                print("  %5.1fs  %11d   %-6s  %s"
                      % (time.time() - t0, hz, st.want_band_name, st.band_name))
                last = now
        except jl.LinkError:
            pass
        time.sleep(0.8)
    link.close()
    print("done")


if __name__ == "__main__":
    main()
