"""Where the window remembers how it last reached the amplifier.

Started from an icon there is no command line, so the connection has to come
from somewhere: what worked last time, or a broadcast for a Hermes Lite 2.
"""

import json
import os
import platform


def config_dir():
    s = platform.system()
    if s == "Darwin":
        base = os.path.expanduser("~/Library/Application Support")
    elif s == "Windows":
        base = os.environ.get("APPDATA") or os.path.expanduser("~")
    else:
        base = os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config")
    return os.path.join(base, "JUMA PA" if s in ("Darwin", "Windows") else "juma-pa")


def config_file():
    return os.path.join(config_dir(), "config.json")


DEFAULTS = {
    "kind": "hl2",          # "hl2" or "usb"
    "hl2": "",              # last address that answered
    "port": 1024,
    "usb": "",
    "lang": "de",
    "theme": "system",
}


def load():
    cfg = dict(DEFAULTS)
    try:
        with open(config_file(), "r") as f:
            got = json.load(f)
        if isinstance(got, dict):
            for k in DEFAULTS:
                if k in got:
                    cfg[k] = got[k]
    except (OSError, ValueError):
        pass                # no file yet, or someone edited it into nonsense
    return cfg


def save(cfg):
    """Best effort. A window that cannot write its settings still works."""
    try:
        os.makedirs(config_dir(), exist_ok=True)
        tmp = config_file() + ".tmp"
        with open(tmp, "w") as f:
            json.dump({k: cfg.get(k, DEFAULTS[k]) for k in DEFAULTS}, f, indent=1)
        os.replace(tmp, config_file())
        return True
    except OSError:
        return False
