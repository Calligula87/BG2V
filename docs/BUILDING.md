# Building BG2V

BG2V is at the loader proof-of-concept stage. It does not run the game yet.
The first VPK is designed to expose missing imports and prove that the Android
engine can reach `JNI_OnLoad` on real Vita hardware.

## Requirements

- A homebrew-capable PlayStation Vita.
- `kubridge.skprx` installed on the Vita.
- The soft-float VitaSDK toolchain (`VitaSDK-softfp`), not the standard
  hard-float toolchain.
- A legally obtained Android ARMv7 `libBaldursGate.so`.

Do not copy the APK, OBBs, or game data into this Git repository.

## Clone

```sh
git clone --recurse-submodules https://github.com/CalligulaRex/BG2V.git
cd BG2V
```

For an existing clone:

```sh
git submodule update --init --recursive
```

## Build

Set `VITASDK` to the soft-float SDK installation, then:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Debug is intentional for the first run because unresolved imports and JNI
lookups must appear in the log. The output will be `build/BG2V.vpk`.

## Device data

After installing the VPK, create this directory on the Vita:

```text
ux0:data/bg2v/
```

Copy only the 32-bit ARMv7 library from your own APK to:

```text
ux0:data/bg2v/libBaldursGate.so
```

Do not use the ARM64 library. The audited target is ARM EABI5 soft-float.

## Expected first result

Success is a dialog saying that the loader milestone passed. A failure is also
useful: photograph the error and capture the debug console output if available,
because it identifies the next missing symbol or JNI method to implement.

The SDL `nativeInit` entry point is deliberately not called yet. That becomes
safe only after the Java callback surface and initial import set are bridged.

## Repository architecture

- `source/dynlib.c`: Android/Bionic symbol-to-Vita mappings.
- `source/java.c`: fake Java classes, methods, and fields.
- `source/patch.c`: compatibility patches only.
- `source/main.c`: guarded BG2 startup sequence.
- `source/reimpl/`: Android API compatibility implementations.
- `lib/`: pinned upstream loader dependencies.

BG2V will not include purchase-verification bypasses or proprietary game data.
