#!/usr/bin/env python3
"""Assemble a platform-specific Pocket Morse desktop uploader archive."""
from __future__ import annotations

import argparse
import platform
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROJECT_FILES = (
    "lib",
    "src",
    "platformio.ini",
    "partitions_4mb.csv",
    "partitions_16mb.csv",
    "scripts/display-setup.py",
)


def archive_name() -> str:
    system = {"Windows": "windows", "Darwin": "macos"}.get(platform.system())
    if not system:
        raise RuntimeError(f"Unsupported packaging platform: {platform.system()}")
    arch = platform.machine().lower().replace("amd64", "x64").replace("aarch64", "arm64")
    return f"PocketMorseUploader-{system}-{arch}.zip"


def copy_project(destination: Path) -> None:
    destination.mkdir()
    for item in PROJECT_FILES:
        source = ROOT / item
        target = destination / item
        if source.is_dir():
            shutil.copytree(source, target, ignore=shutil.ignore_patterns(".DS_Store", "__pycache__", "*.pyc"))
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)


def zip_tree(root: Path, archive: Path) -> None:
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as output:
        for path in sorted(root.rglob("*")):
            relative = Path(root.name) / path.relative_to(root)
            if path.is_symlink():
                info = zipfile.ZipInfo(str(relative))
                info.create_system = 3
                info.external_attr = (0o120777 << 16)
                output.writestr(info, str(path.readlink()))
            elif path.is_file():
                output.write(path, relative)
            elif path.is_dir():
                output.writestr(str(relative).rstrip("/") + "/", "")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dist", type=Path, default=ROOT / "dist")
    parser.add_argument("--output", type=Path, default=ROOT / "build" / "uploader-release")
    args = parser.parse_args()

    windows = platform.system() == "Windows"
    gui_file = "PocketMorseUploader.exe" if windows else "PocketMorseUploader.app"
    pair_file = "provision_pair.exe" if windows else "provision_pair"
    gui = args.dist / gui_file
    helper = args.dist / pair_file
    if not gui.exists() or not helper.is_file():
        parser.error(f"Expected GUI {gui} and pairing helper {helper}; build both with PyInstaller first")

    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / archive_name()
    with tempfile.TemporaryDirectory(prefix="pocket-morse-package-") as temporary:
        stage = Path(temporary) / archive.stem
        stage.mkdir()
        if gui.is_dir():
            shutil.copytree(gui, stage / gui.name, symlinks=True)
        else:
            shutil.copy2(gui, stage / gui.name)
        shutil.copy2(helper, stage / helper.name)
        copy_project(stage / "project")
        shutil.copy2(ROOT / "LICENSE", stage / "LICENSE")
        zip_tree(stage, archive)

    print(f"Created {archive}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
