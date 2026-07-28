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

On Windows, run these commands inside Ubuntu WSL:

```sh
./tools/setup_wsl_softfp.sh
./tools/build_wsl.sh
```

The setup script installs the SDK separately at `~/vitasdk-softfp` and will not
overwrite another VitaSDK. The build script refuses to use a hard-float
compiler. Debug is intentional for the first run because unresolved imports and
JNI lookups must appear in the log. The script stages the build on WSL's native
filesystem because Vita's VPK tool cannot handle spaces in Windows paths. It
copies the results back to `artifacts/BG2V-debug.vpk` and
`artifacts/eboot-debug.bin`; this directory is intentionally ignored by Git.

On Linux, either use the same scripts or set `VITASDK` to an existing soft-float
SDK and run CMake manually.

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

## Vita movie overrides

The original 1280x720 VP8 startup movies exceed the Vita's practical software
decoding budget. BG2V can load optimized copies from
`ux0:data/bg2v/movies/`, while all original media remains in the user's own
installation.

With `ffmpeg` and `unzip` installed, prepare the confirmed 640x360, 15 fps
startup set from your legally obtained patch OBB:

```sh
./tools/prepare_vita_movies.sh \
  /path/to/patch.5826.com.beamdog.baldursgateIIenhancededition.obb \
  ./artifacts/optimized-movies
```

Copy the generated `logo.wbm`, `intro.wbm`, and `intro15f.wbm` to:

```text
ux0:data/bg2v/movies/
```

The generated files contain copyrighted game media and must not be committed
or redistributed. Additional movie filenames can be supplied after the output
directory to optimize later cinematics with the same profile.

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
