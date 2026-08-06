# BG2VBETA

Beta port of Baldur's Gate II: Enhanced Edition for PlayStation Vita, built as
a direct wrapper around the user's legally supplied Android ARMv7 game engine.

The initial APK audit found a 32-bit ARMv7, soft-float native engine using SDL2,
OpenGL ES 2, and OpenAL. This makes an Android-native-library wrapper technically
plausible, subject to JNI, graphics, memory, and data-loading prototypes.

See [`analysis/BG2_APK_FEASIBILITY.md`](analysis/BG2_APK_FEASIBILITY.md) for the
initial findings and [`analysis/ROUTE_DECISION.md`](analysis/ROUTE_DECISION.md)
for the wrapper-versus-GemRB decision.

## Legal and repository policy

This repository does not contain the game, APKs, OBBs, proprietary native
libraries, copyrighted game data, or tools for bypassing purchase verification.
Users and developers must supply files from their own legitimate installation.

The `input/` directory and extracted proprietary analysis material are excluded
from Git.

## Current status

Selected route: **bounded direct Android ARMv7 wrapper proof of concept**, with
GemRB retained as the fallback.

Device milestone W1/W2 bootstrap passed on 24 July 2026:

- the original ARMv7 `libBaldursGate.so` loaded and relocated on Vita;
- the initial import table resolved;
- `JNI_OnLoad` accepted the fake VM and returned JNI 1.4 (`0x00010004`);
- vitaGL initialized successfully.

The first direct-wrapper skeleton is now based on the MIT-licensed
[`soloader-boilerplate`](https://github.com/v-atamanenko/soloader-boilerplate).
It pins FalsoJNI, `so_util`, FalsoNDK, and vitaGL as Git submodules.

The completed first device milestone was intentionally small:

1. load the user-supplied `ux0:data/bg2v/libBaldursGate.so`;
2. relocate it and resolve its Android imports;
3. initialize the fake JVM and call `JNI_OnLoad`;
4. initialize vitaGL;
5. display an explicit success message instead of entering an unfinished loop.

See [`docs/INSTALL.md`](docs/INSTALL.md) for the end-user installation guide.
See [`docs/BUILDING.md`](docs/BUILDING.md) for development setup and build
instructions.

## Installing the Vita build

The VPK contains the Vita wrapper, not the game data. Users supply their own
Android APK, run the included preparation script, copy the generated folder to
the Vita with VitaShell FTP, and then install the VPK. The complete numbered
checklist is in [`docs/INSTALL.md`](docs/INSTALL.md).
