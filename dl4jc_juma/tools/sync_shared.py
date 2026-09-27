#!/usr/bin/env python3
"""Keeps the copied sources honest against the ESP32 firmware they come from.

    python3 tools/sync_shared.py [--update] [path/to/esp32-juma]

juma_status.* and bands.* here are copies: the status parser and the band table. Both controllers for this
amplifier need them and they describe the amplifier, not either controller - so
one of the two has to be the original, and that is esp32-juma/src.

They are kept byte for byte identical on purpose: that is what makes this
check mean anything. So nothing may be edited here, not even a comment saying
where it came from. Corrections go into esp32-juma/src and come back with
--update.

Without --update it only reports, and exits non-zero on a difference, which is
what a hook or a CI step wants. tests/run.sh is the other half: this says the
copy is stale, those say it is wrong.
"""

import filecmp
import os
import shutil
import sys

FILES = ("juma_status.h", "juma_status.cpp", "bands.h", "bands.cpp")
HERE = os.path.dirname(os.path.abspath(__file__))
SHARED = os.path.join(HERE, "..")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    update = "--update" in sys.argv[1:]
    origin = args[0] if args else os.path.join(HERE, "..", "..", "..", "esp32-juma")
    src = os.path.join(origin, "src")
    if not os.path.isdir(src):
        raise SystemExit("no src/ under %s - give the path to an esp32-juma "
                         "checkout" % os.path.normpath(origin))

    stale = []
    for f in FILES:
        a, b = os.path.join(src, f), os.path.join(SHARED, f)
        if not os.path.exists(a):
            raise SystemExit("%s is missing from %s" % (f, src))
        if not os.path.exists(b) or not filecmp.cmp(a, b, shallow=False):
            stale.append(f)

    if not stale:
        print("the copies match %s" % os.path.normpath(src))
        return 0
    for f in stale:
        print("differs: %s" % f)
    if not update:
        print("\nrun with --update to take the originals, or correct them there")
        return 1
    for f in stale:
        shutil.copy2(os.path.join(src, f), os.path.join(SHARED, f))
        print("updated: %s" % f)
    print("\nnow run tests/run.sh")
    return 0


if __name__ == "__main__":
    sys.exit(main())
