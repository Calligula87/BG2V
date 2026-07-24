"""Decode the Android manifest/DEX interface and correlate it with native strings."""

from __future__ import annotations

import re
import sys
from pathlib import Path

from loguru import logger

logger.remove()

from androguard.core.axml import AXMLPrinter
from androguard.core.dex import DEX


TARGET_PREFIXES = (
    "Lcom/beamdog/",
    "Lorg/libsdl/app/",
)


def decode_manifest(path: Path) -> None:
    printer = AXMLPrinter(path.read_bytes())
    root = printer.get_xml_obj()
    android = "{http://schemas.android.com/apk/res/android}"
    print("MANIFEST")
    print(f"  package={root.get('package')}")
    print(f"  versionCode={root.get(android + 'versionCode')}")
    print(f"  versionName={root.get(android + 'versionName')}")
    sdk = root.find("uses-sdk")
    if sdk is not None:
        print(f"  minSdk={sdk.get(android + 'minSdkVersion')}")
        print(f"  targetSdk={sdk.get(android + 'targetSdkVersion')}")
    for feature in root.findall("uses-feature"):
        print(f"  feature.glEsVersion={feature.get(android + 'glEsVersion')}")
    for permission in root.findall("uses-permission"):
        print(f"  permission={permission.get(android + 'name')}")
    application = root.find("application")
    if application is not None:
        for activity in application.findall("activity"):
            print(f"  activity={activity.get(android + 'name')}")
        for service in application.findall("service"):
            print(f"  service={service.get(android + 'name')}")
        for provider in application.findall("provider"):
            print(f"  provider={provider.get(android + 'name')}")


def load_dexes(paths: list[Path]) -> list[DEX]:
    return [DEX(path.read_bytes()) for path in paths]


def all_target_classes(dexes: list[DEX]):
    for dex in dexes:
        for cls in dex.get_classes():
            if cls.get_name().startswith(TARGET_PREFIXES):
                yield cls


def inspect_dex(dexes: list[DEX], native_blob: bytes) -> None:
    target_classes = sorted(all_target_classes(dexes), key=lambda cls: cls.get_name())
    functional_classes = [
        cls for cls in target_classes
        if "/R" not in cls.get_name() and "BuildConfig" not in cls.get_name()
    ]
    native_methods = []
    correlated_methods = []

    print("\nDEX_SUMMARY")
    print(f"  dex_files={len(dexes)}")
    print(f"  total_classes={sum(len(dex.get_classes()) for dex in dexes)}")
    print(f"  target_classes={len(target_classes)}")
    print(f"  functional_target_classes={len(functional_classes)}")
    for cls in functional_classes:
        print(f"  class={cls.get_name()} super={cls.get_superclassname()}")
        for method in cls.get_methods():
            flags = method.get_access_flags_string()
            row = (
                cls.get_name(),
                method.get_name(),
                method.get_descriptor(),
                flags,
            )
            if "native" in flags.split():
                native_methods.append(row)
            name = method.get_name().encode()
            descriptor = method.get_descriptor().encode()
            if name in native_blob and descriptor in native_blob:
                correlated_methods.append(row)

    print(f"\nJAVA_NATIVE_DECLARATIONS count={len(native_methods)}")
    for cls, name, descriptor, flags in sorted(native_methods):
        print(f"  {cls}->{name}{descriptor} [{flags}]")

    print(f"\nMETHODS_CORRELATED_IN_NATIVE_STRINGS count={len(correlated_methods)}")
    for cls, name, descriptor, flags in sorted(correlated_methods):
        print(f"  {cls}->{name}{descriptor} [{flags}]")

    printable = {
        item.decode("ascii", "ignore")
        for item in re.findall(rb"[\x20-\x7e]{4,}", native_blob)
    }
    java_descriptors = sorted({
        token for token in printable
        if (
            token.startswith("Landroid/")
            or token.startswith("Lorg/libsdl/")
            or token.startswith("Lcom/beamdog/")
            or token.startswith("()Landroid/")
            or token.startswith("(Landroid/")
        )
        and len(token) < 240
    })
    print(f"\nNATIVE_JAVA_DESCRIPTOR_STRINGS count={len(java_descriptors)}")
    for token in java_descriptors:
        print(f"  {token}")


def main(directory: str) -> None:
    root = Path(directory)
    manifest = root / "AndroidManifest.xml"
    dex_paths = [root / "classes.dex", root / "classes2.dex"]
    native_path = root / "lib" / "armeabi-v7a" / "libBaldursGate.so"
    decode_manifest(manifest)
    dexes = load_dexes(dex_paths)
    inspect_dex(dexes, native_path.read_bytes())


if __name__ == "__main__":
    main(sys.argv[1])
