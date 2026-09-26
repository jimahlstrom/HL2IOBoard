#!/usr/bin/env python3
"""Packs the JUMA window into something you can double-click.

    python3 -m venv .venv
    .venv/bin/pip install pyinstaller pyserial
    .venv/bin/python make_app.py

Builds for the system it runs on. PyInstaller does not cross-compile, so the
Windows package has to be built on Windows and the Linux one on Linux - there is
a GitHub Actions workflow in .github/ that does all three if you would rather
not keep three machines.

Why an embedded interpreter, when a shell script was 316 kB: macOS ties the
local-network permission to the signed executable. In a script bundle that is
python3, not the app, so the app never appears under Privacy & Security and is
never asked about - measured on macOS 27, where ad-hoc signing, keeping the app
as the parent process and resetting TCC all changed nothing. It also settles
which Tk gets used, and that matters: Apple still ships 8.5 with
/usr/bin/python3, and 8.5 draws neither the labels nor the meters.
"""

import os
import platform
import shutil
import stat
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DIST = os.path.join(HERE, "dist")


def run(cmd, **kw):
    print("+", " ".join(cmd))
    return subprocess.run(cmd, check=True, **kw)


def build():
    py = sys.executable
    try:
        import PyInstaller           # noqa: F401
    except ImportError:
        raise SystemExit(
            "PyInstaller is missing. In a virtual environment:\n"
            "  python3 -m venv .venv && .venv/bin/pip install pyinstaller pyserial\n"
            "  .venv/bin/python make_app.py")
    run([py, "-m", "PyInstaller", "--clean", "--noconfirm", "juma-pa.spec"],
        cwd=HERE)


# --- macOS ----------------------------------------------------------------
def finish_macos():
    app = os.path.join(DIST, "JUMA PA.app")
    if not os.path.isdir(app):
        raise SystemExit("no bundle at %s" % app)
    # PyInstaller signs it already; do it again so the result is known, and
    # verify, because an unsigned bundle is refused by the Finder in silence.
    run(["codesign", "--force", "--deep", "--sign", "-", app])
    run(["codesign", "--verify", "--verbose", app])
    print("\n%s is ready. Drag it to /Applications." % app)
    print("On first use macOS asks to let it onto the local network - say yes,")
    print("or the search for the Hermes Lite 2 comes back empty.")
    print()
    print("Note for whoever is rebuilding this: an ad-hoc signature carries the")
    print("binary's own hash, so every build is a different application as far")
    print("as the system is concerned and the permission does not carry over.")
    print("The switch in System Settings stays on and stops matching. Clear it")
    print("and let it ask again:")
    print("    tccutil reset All de.dl4jc.jumapa")
    print("Someone who installs a finished build once never meets this.")
    return app


# --- Linux ----------------------------------------------------------------
DESKTOP = """[Desktop Entry]
Type=Application
Name=JUMA PA
Comment=Control a JUMA amplifier through a Hermes Lite 2 IO board
Exec=@EXEC@
Icon=juma-pa
Terminal=false
Categories=HamRadio;Network;Utility;
"""

INSTALL_SH = """#!/bin/sh
# Puts JUMA PA in this user's application menu.
set -e
here=$(cd "$(dirname "$0")" && pwd)
apps="$HOME/.local/share/applications"
icons="$HOME/.local/share/icons/hicolor"
mkdir -p "$apps" "$icons/128x128/apps" "$icons/256x256/apps"
sed "s|@EXEC@|$here/juma-pa/juma-pa|" "$here/juma-pa.desktop.in" \\
    > "$apps/juma-pa.desktop"
cp "$here/juma-pa/_internal/icons/juma-pa-128.png" \\
   "$icons/128x128/apps/juma-pa.png" 2>/dev/null || \\
cp "$here/juma-pa/icons/juma-pa-128.png" "$icons/128x128/apps/juma-pa.png"
cp "$here/juma-pa/_internal/icons/juma-pa-256.png" \\
   "$icons/256x256/apps/juma-pa.png" 2>/dev/null || \\
cp "$here/juma-pa/icons/juma-pa-256.png" "$icons/256x256/apps/juma-pa.png"
command -v update-desktop-database >/dev/null && \\
    update-desktop-database "$apps" || true
command -v gtk-update-icon-cache >/dev/null && \\
    gtk-update-icon-cache -f -t "$icons" || true
echo "JUMA PA is in the application menu."
"""


def finish_linux():
    with open(os.path.join(DIST, "juma-pa.desktop.in"), "w") as f:
        f.write(DESKTOP)
    p = os.path.join(DIST, "install.sh")
    with open(p, "w") as f:
        f.write(INSTALL_SH)
    os.chmod(p, os.stat(p).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    print("\nBuilt %s/juma-pa/. Run %s to put it in the menu." % (DIST, p))
    return DIST


# --- Windows --------------------------------------------------------------
SHORTCUT_PS1 = r"""# Puts JUMA PA on the Desktop and in the Start Menu.
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$target = Join-Path $here 'juma-pa\juma-pa.exe'
$ws = New-Object -ComObject WScript.Shell
foreach ($dir in @([Environment]::GetFolderPath('Desktop'),
                   (Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs'))) {
    $lnk = $ws.CreateShortcut((Join-Path $dir 'JUMA PA.lnk'))
    $lnk.TargetPath = $target
    $lnk.WorkingDirectory = Join-Path $here 'juma-pa'
    $lnk.Description = 'Control a JUMA amplifier through a Hermes Lite 2 IO board'
    $lnk.Save()
}
Write-Host 'JUMA PA is on the Desktop and in the Start Menu.'
"""


def finish_windows():
    p = os.path.join(DIST, "make-shortcuts.ps1")
    with open(p, "w") as f:
        f.write(SHORTCUT_PS1)
    print("\nBuilt %s\\juma-pa\\. Run make-shortcuts.ps1 for the shortcuts." % DIST)
    return DIST


def main():
    if not os.path.exists(os.path.join(HERE, "icons", "juma-pa-128.png")):
        print("icons are missing, drawing them first")
        run([sys.executable, os.path.join(HERE, "make_icon.py")])
    build()
    s = platform.system()
    if s == "Darwin":
        finish_macos()
    elif s == "Windows":
        finish_windows()
    else:
        finish_linux()


if __name__ == "__main__":
    main()
