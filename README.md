# BG2V

Feasibility research and proof-of-concept work for running Baldur's Gate II:
Enhanced Edition on PlayStation Vita.

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

The first direct-wrapper skeleton is now based on the MIT-licensed
[`soloader-boilerplate`](https://github.com/v-atamanenko/soloader-boilerplate).
It pins FalsoJNI, `so_util`, FalsoNDK, and vitaGL as Git submodules.

The current device milestone is intentionally small:

1. load the user-supplied `ux0:data/bg2v/libBaldursGate.so`;
2. relocate it and resolve its Android imports;
3. initialize the fake JVM and call `JNI_OnLoad`;
4. initialize vitaGL;
5. display an explicit success message instead of entering an unfinished loop.

See [`docs/BUILDING.md`](docs/BUILDING.md) for setup and build instructions.
