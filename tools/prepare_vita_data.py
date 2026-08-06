"""Prepare BG2V's Vita data folder from a legally obtained Android APK.

The APK is a ZIP archive. This script copies only the files BG2V needs into
an output directory; it never downloads or distributes game data.
"""

from __future__ import annotations

import shutil
import sys
import zipfile
from pathlib import Path


LIBRARY = "lib/armeabi-v7a/libBaldursGate.so"
OBBS = (
    "assets/main.5826.com.beamdog.baldursgateIIenhancededition.obb",
    "assets/patch.5826.com.beamdog.baldursgateIIenhancededition.obb",
)


def copy_entry(apk: zipfile.ZipFile, entry: str, destination: Path) -> None:
    try:
        info = apk.getinfo(entry)
    except KeyError as exc:
        raise RuntimeError(f"APK is missing required entry: {entry}") from exc
    destination.parent.mkdir(parents=True, exist_ok=True)
    with apk.open(info) as source, destination.open("wb") as target:
        shutil.copyfileobj(source, target, length=1024 * 1024)


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: python tools/prepare_vita_data.py GAME.apk OUTPUT_DIR")
        return 2

    apk_path = Path(sys.argv[1]).expanduser().resolve()
    output = Path(sys.argv[2]).expanduser().resolve()
    if not apk_path.is_file():
        print(f"APK not found: {apk_path}", file=sys.stderr)
        return 1

    output.mkdir(parents=True, exist_ok=True)
    try:
        with zipfile.ZipFile(apk_path) as apk:
            copy_entry(apk, LIBRARY, output / "libBaldursGate.so")
            for entry in OBBS:
                copy_entry(apk, entry, output / Path(entry).name)

            # The compatibility layer reports this package path to the engine.
            shutil.copy2(apk_path, output / "game.apk")

            # Preserve any ordinary APK assets (the OBBs themselves are placed
            # at the data root above). Ignore directories and native libraries.
            for info in apk.infolist():
                if not info.filename.startswith("assets/") or info.is_dir():
                    continue
                if Path(info.filename).name in {Path(item).name for item in OBBS}:
                    continue
                relative = Path(info.filename).relative_to("assets")
                copy_entry(apk, info.filename, output / "assets" / relative)
    except zipfile.BadZipFile:
        print("The input file is not a valid APK/ZIP archive.", file=sys.stderr)
        return 1
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    print(f"Prepared Vita data in: {output}")
    print("Copy the contents of this directory to ux0:data/bg2v/")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
