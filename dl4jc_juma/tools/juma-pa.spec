# -*- mode: python ; coding: utf-8 -*-
"""PyInstaller recipe for the JUMA window.

    .venv/bin/pyinstaller --clean --noconfirm juma-pa.spec

Built for the system it runs on - PyInstaller does not cross-compile, so the
Windows and Linux packages have to be built there (or in CI, see the README).

Why a bundle with its own interpreter at all, when a shell script was 316 kB:
macOS hangs the local-network permission on the executable that carries the
signature. In a script bundle that is python3, not the app, so the app never
appears in System Settings and never gets asked for. Measured on macOS 27:
ad-hoc signing did not help, keeping the app as the parent process did not
help, resetting TCC did not even produce a prompt. An embedded interpreter
gives the bundle an identity of its own - and it settles which Tk is used,
which matters because Apple still ships 8.5 and it cannot draw this window.
"""

import os
import sys

HERE = os.path.abspath(os.getcwd())
NAME = "JUMA PA"

a = Analysis(
    ["juma_gui.py"],
    pathex=[HERE],
    binaries=[],
    datas=[("icons", "icons")],
    # Reached through a sys.path insert rather than a package import, so name
    # them: the analysis cannot see round that.
    hiddenimports=["juma_link", "juma_theme", "juma_config", "serial"],
    hookspath=[],
    runtime_hooks=[],
    excludes=["numpy", "PIL", "pytest", "setuptools"],
    noarchive=False,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz, a.scripts, [],
    exclude_binaries=True,
    name=NAME if sys.platform == "darwin" else "juma-pa",
    debug=False,
    strip=False,
    upx=False,
    console=False,          # no terminal window behind it
    icon=("icons/juma-pa.icns" if sys.platform == "darwin"
          else "icons/juma-pa.ico" if os.name == "nt" else None),
)
coll = COLLECT(
    exe, a.binaries, a.datas,
    strip=False, upx=False,
    name="juma-pa",
)

if sys.platform == "darwin":
    app = BUNDLE(
        coll,
        name=NAME + ".app",
        icon="icons/juma-pa.icns",
        bundle_identifier="de.dl4jc.jumapa",
        info_plist={
            "CFBundleShortVersionString": "1.0",
            "CFBundleVersion": "1",
            "NSHighResolutionCapable": True,
            # Without this the system refuses the broadcast that looks for the
            # Hermes Lite 2, and says "no route to host" while doing it.
            "NSLocalNetworkUsageDescription":
                "Finds the Hermes Lite 2 the amplifier is connected to, and "
                "talks to the IO board through it.",
            "LSApplicationCategoryType": "public.app-category.utilities",
            "LSMinimumSystemVersion": "11.0",
        },
    )
