import re
import struct
import sys
from pathlib import Path


def cstring(blob, offset):
    end = blob.find(b"\0", offset)
    if end < 0:
        end = len(blob)
    return blob[offset:end].decode("utf-8", "replace")


def elf_report(path):
    blob = Path(path).read_bytes()
    if blob[:4] != b"\x7fELF" or blob[4] != 1:
        raise ValueError("Expected a 32-bit ELF")
    endian = "<" if blob[5] == 1 else ">"
    header = struct.unpack_from(endian + "16sHHIIIIIHHHHHH", blob, 0)
    machine = header[2]
    flags = header[7]
    shoff, shentsize, shnum, shstrndx = header[6], header[11], header[12], header[13]
    sections = []
    for index in range(shnum):
        off = shoff + index * shentsize
        values = struct.unpack_from(endian + "IIIIIIIIII", blob, off)
        sections.append({
            "name_off": values[0], "type": values[1], "flags": values[2],
            "addr": values[3], "off": values[4], "size": values[5],
            "link": values[6], "info": values[7], "align": values[8],
            "entsize": values[9],
        })
    shstr = sections[shstrndx]
    names = blob[shstr["off"]:shstr["off"] + shstr["size"]]
    for section in sections:
        section["name"] = cstring(names, section["name_off"])

    needed = []
    dynamic = next((s for s in sections if s["type"] == 6), None)
    if dynamic:
        dynstr_section = sections[dynamic["link"]]
        dynstr = blob[dynstr_section["off"]:dynstr_section["off"] + dynstr_section["size"]]
        entry_size = dynamic["entsize"] or 8
        for off in range(dynamic["off"], dynamic["off"] + dynamic["size"], entry_size):
            tag, value = struct.unpack_from(endian + "II", blob, off)
            if tag == 0:
                break
            if tag == 1:
                needed.append(cstring(dynstr, value))

    undefined = []
    exported = []
    dynsym = next((s for s in sections if s["type"] == 11), None)
    if dynsym:
        strsec = sections[dynsym["link"]]
        strtab = blob[strsec["off"]:strsec["off"] + strsec["size"]]
        entry_size = dynsym["entsize"] or 16
        for off in range(dynsym["off"], dynsym["off"] + dynsym["size"], entry_size):
            name_off, value, size, info, other, shndx = struct.unpack_from(
                endian + "IIIBBH", blob, off
            )
            name = cstring(strtab, name_off) if name_off else ""
            if not name:
                continue
            bind = info >> 4
            if shndx == 0:
                undefined.append(name)
            elif bind in (1, 2):
                exported.append(name)

    printable = [
        item.decode("ascii", "ignore")
        for item in re.findall(rb"[\x20-\x7e]{5,}", blob)
    ]
    interesting_patterns = (
        "JNI", "Java_", "android/", "android.", "GLES", "gl", "EGL",
        "OpenAL", "alc", "AL_", "SDL", "fmod", "Bink", "pthread",
        "dlopen", "dlsym", "AAsset", "ANative", "Media", "http",
        "billing", "license", "Beamdog",
    )
    interesting = sorted({
        value for value in printable
        if any(pattern.lower() in value.lower() for pattern in interesting_patterns)
        and len(value) < 240
    })

    print(f"ELF: {path}")
    print(f"machine={machine} flags=0x{flags:08x} sections={shnum}")
    print("NEEDED:")
    for value in needed:
        print(f"  {value}")
    print(f"UNDEFINED ({len(undefined)}):")
    for value in sorted(undefined):
        print(f"  {value}")
    key_exports = sorted(
        value for value in exported
        if value == "JNI_OnLoad" or value.startswith("Java_")
    )
    print(f"KEY_EXPORTED ({len(key_exports)} of {len(exported)} total):")
    for value in key_exports:
        print(f"  {value}")
    print(f"INTERESTING_STRINGS ({len(interesting)}):")
    for value in interesting:
        print(f"  {value}")


def dex_report(path):
    blob = Path(path).read_bytes()
    tokens = {
        item.decode("utf-8", "ignore")
        for item in re.findall(rb"[\x20-\x7e]{6,}", blob)
    }
    patterns = (
        "beamdog", "baldur", "Activity", "Native", "billing", "license",
        "obb", "Google", "Firebase", "Asset", "Surface", "OpenGL", "audio",
        "com/android", "android/",
    )
    values = sorted(
        item for item in tokens
        if any(pattern.lower() in item.lower() for pattern in patterns)
        and len(item) < 300
    )
    print(f"DEX: {path}")
    print(f"INTERESTING_STRINGS ({len(values)}):")
    for value in values:
        print(f"  {value}")


if __name__ == "__main__":
    if sys.argv[1].lower().endswith(".dex"):
        dex_report(sys.argv[1])
    else:
        elf_report(sys.argv[1])
