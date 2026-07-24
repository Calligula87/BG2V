"""Inventory ZIP-format OBB files embedded as stored APK entries.

The script never extracts payload data. It presents each embedded OBB as a
bounded, seekable view over the APK and reads only ZIP metadata.
"""

from __future__ import annotations

import collections
import io
import struct
import sys
import zipfile
from pathlib import Path, PurePosixPath


OBB_NAMES = (
    "assets/main.5826.com.beamdog.baldursgateIIenhancededition.obb",
    "assets/patch.5826.com.beamdog.baldursgateIIenhancededition.obb",
)


class SliceReader(io.RawIOBase):
    def __init__(self, source: io.BufferedReader, start: int, size: int):
        self.source = source
        self.start = start
        self.size = size
        self.position = 0

    def readable(self) -> bool:
        return True

    def seekable(self) -> bool:
        return True

    def tell(self) -> int:
        return self.position

    def seek(self, offset: int, whence: int = io.SEEK_SET) -> int:
        if whence == io.SEEK_SET:
            position = offset
        elif whence == io.SEEK_CUR:
            position = self.position + offset
        elif whence == io.SEEK_END:
            position = self.size + offset
        else:
            raise ValueError(f"unsupported whence {whence}")
        if position < 0:
            raise ValueError("negative seek")
        self.position = min(position, self.size)
        return self.position

    def read(self, size: int = -1) -> bytes:
        remaining = self.size - self.position
        if size is None or size < 0:
            size = remaining
        size = min(size, remaining)
        self.source.seek(self.start + self.position)
        data = self.source.read(size)
        self.position += len(data)
        return data

    def readinto(self, buffer) -> int:
        data = self.read(len(buffer))
        buffer[:len(data)] = data
        return len(data)


def stored_entry_offset(source: io.BufferedReader, info: zipfile.ZipInfo) -> int:
    source.seek(info.header_offset)
    header = source.read(30)
    signature, *_rest, name_len, extra_len = struct.unpack("<IHHHHHIIIHH", header)
    if signature != 0x04034B50:
        raise ValueError("invalid local ZIP header")
    return info.header_offset + 30 + name_len + extra_len


def summarize_obb(apk_file: io.BufferedReader, outer: zipfile.ZipFile, name: str) -> None:
    info = outer.getinfo(name)
    print(f"\nOBB {name}")
    print(
        f"outer_method={info.compress_type} size={info.file_size} "
        f"compressed={info.compress_size}"
    )
    if info.compress_type != zipfile.ZIP_STORED:
        print("Cannot inspect in place: outer APK entry is compressed.")
        return

    start = stored_entry_offset(apk_file, info)
    view = io.BufferedReader(SliceReader(apk_file, start, info.file_size))
    with zipfile.ZipFile(view) as obb:
        entries = obb.infolist()
        files = [item for item in entries if not item.is_dir()]
        total = sum(item.file_size for item in files)
        extensions = collections.Counter(
            PurePosixPath(item.filename).suffix.lower() or "<none>" for item in files
        )
        top_dirs = collections.Counter(
            PurePosixPath(item.filename).parts[0] if PurePosixPath(item.filename).parts else "<root>"
            for item in files
        )
        markers = (
            "chitin.key", "dialog.tlk", "dialogf.tlk", "baldur.lua",
            "engine.lua", "weidu.log", "lang", "data", "movies", "music",
            "scripts", "override",
        )
        matches = [
            item for item in files
            if any(marker in item.filename.lower() for marker in markers)
        ]
        largest = sorted(files, key=lambda item: item.file_size, reverse=True)[:30]
        languages = sorted({
            PurePosixPath(item.filename).parts[1]
            for item in files
            if len(PurePosixPath(item.filename).parts) > 2
            and PurePosixPath(item.filename).parts[0] == "lang"
        })

        print(f"zip_entries={len(entries)} files={len(files)} total_uncompressed={total}")
        if languages:
            print(f"languages={','.join(languages)}")
        for special in ("chitin.key", "engine.lua"):
            if special in obb.namelist():
                with obb.open(special) as stream:
                    payload = stream.read(min(4096, obb.getinfo(special).file_size))
                print(f"{special}.hex={payload[:32].hex()}")
                if special.endswith(".lua") and obb.getinfo(special).file_size <= 4096:
                    print(f"{special}.text={payload.decode('utf-8', 'replace')!r}")
        print("top_directories:")
        for key, count in top_dirs.most_common(30):
            print(f"  {key}\t{count}")
        print("extensions:")
        for key, count in extensions.most_common(50):
            print(f"  {key}\t{count}")
        print("key_markers:")
        for item in matches[:200]:
            print(f"  {item.file_size}\t{item.compress_type}\t{item.filename}")
        print("largest_files:")
        for item in largest:
            print(f"  {item.file_size}\t{item.compress_type}\t{item.filename}")


def main(apk_path: str) -> None:
    path = Path(apk_path)
    with path.open("rb") as apk_file:
        with zipfile.ZipFile(apk_file) as outer:
            for name in OBB_NAMES:
                summarize_obb(apk_file, outer, name)


if __name__ == "__main__":
    main(sys.argv[1])
