#!/usr/bin/env python3
"""Set the public product name/version and ad-hoc sign a PyInstaller macOS app."""
from __future__ import annotations

import json
import plistlib
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
    if len(sys.argv) != 2:
        print(f"Usage: {Path(sys.argv[0]).name} PATH/TO/PocketMorseUploader.app", file=sys.stderr)
        return 2
    app = Path(sys.argv[1]).resolve()
    info = app / "Contents" / "Info.plist"
    if not app.is_dir() or not info.is_file():
        print(f"Not a macOS application bundle: {app}", file=sys.stderr)
        return 2

    version = json.loads((ROOT / "package.json").read_text(encoding="utf-8"))["version"]
    with info.open("rb") as stream:
        metadata = plistlib.load(stream)
    metadata.update({
        "CFBundleDisplayName": "Pocket Morse Communicators",
        "CFBundleName": "Pocket Morse Communicators",
        "CFBundleIdentifier": "com.github.pilafdob.pocketmorse",
        "CFBundleShortVersionString": version,
        "CFBundleVersion": version,
        "NSHighResolutionCapable": True,
    })
    with info.open("wb") as stream:
        plistlib.dump(metadata, stream, sort_keys=True)

    subprocess.run(["codesign", "--force", "--deep", "--sign", "-", str(app)], check=True)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True)
    print(f"Prepared {metadata['CFBundleDisplayName']} {version}: {app}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
