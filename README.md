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

Next target: load the ARMv7 engine far enough to reach `JNI_OnLoad` and the SDL
native initialization entry point on Vita.
