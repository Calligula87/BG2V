# BG2V route decision

Date: 2026-07-24

## Decision

**Attempt a bounded direct Android ARMv7 wrapper proof of concept first.**

Keep GemRB as the planned fallback if the direct wrapper fails one of the early
graphics, memory, or initialization gates.

This is not yet a commitment to finish the entire direct wrapper. It is a
fail-fast experiment whose endpoint is either:

1. `libBaldursGate.so` reaches its main loop and displays a first frame on Vita;
   or
2. a documented blocker triggers a pivot to a native GemRB Vita port.

## Why the direct wrapper gets the first attempt

The inspected package is unusually favorable for a Vita Android wrapper:

- a 32-bit ARMv7 engine is present;
- the engine is ARM EABI5 and marked soft-float;
- the engine uses the SDL Android interface;
- it links to GLES2, EGL, OpenAL, and ordinary Android/bionic libraries;
- SDL2, Lua, codecs, ZIP support, and most engine code appear statically bundled;
- the game library has only eight direct shared-library dependencies;
- there are 417 undefined imports, mostly from already-understood compatibility
  categories;
- the Android native-window surface is only three directly imported functions;
- the complete game data is present in ordinary unencrypted ZIP-format OBBs;
- the OBB contents are stored, so they can be copied or loaded without an
  expensive decompression layer;
- the mobile engine already has a touch-oriented BG2EE interface.

The library's load segments reserve about 56 MiB before runtime heap and asset
caches. Roughly 47 MiB of that is `.bss`. This is meaningful but does not reject
the Vita route by itself.

## OBB/data findings

### Main OBB

- 2,004,576,250 bytes
- 215 files
- `KEY V1` `chitin.key`
- `engine.lua` explicitly identifies BG2EE mode
- 185 BIFF archives
- 28 PVRZ override textures
- all entries stored without ZIP compression

### Patch OBB

- 1,801,925,724 bytes
- 2,125 files
- language TLKs and voice data
- music, scripts, and WebM movies
- languages: German, English, Spanish, French, Italian, Korean, Polish, Russian,
  and Simplified Chinese
- all entries stored without ZIP compression

This is a recognizable Infinity Engine installation rather than an encrypted or
proprietary opaque data container.

## Java/JNI findings

The decoded package contains:

- 8,848 Java classes total, mostly AndroidX, Google Play, Material, Firebase,
  billing, and downloader libraries;
- 90 Beamdog/SDL namespace classes;
- 52 functional Beamdog/SDL classes after generated resources are excluded;
- 37 Java methods declared `native`.

Most native declarations are the normal SDL lifecycle/input surface:

- initialize, pause, resume, quit, and low-memory;
- surface created/changed/destroyed;
- resize;
- keyboard and composing text;
- mouse, touch, tap, pan, fling, and magnify;
- joystick, hat, and gamepad;
- message and import notifications.

The Beamdog Java layer additionally exposes services visible in the native
library:

- OBB/APK path lookup and expansion-file streams;
- language lookup;
- video play/stop/status;
- DLC state and purchase calls;
- logging;
- save importing;
- Android audio callbacks;
- Wi-Fi and storage dialogs;
- activity title and native surface access.

For the offline proof of concept:

- OBB path and language lookup must work;
- lifecycle, surface, input, text, logging, and audio must work or have native
  Vita replacements;
- movies may initially be skipped;
- Wi-Fi UI, save importing, and Android dialogs can initially be stubbed;
- Google Play, Firebase, billing UI, telemetry, and online multiplayer do not
  need to be recreated.

No purchase-verification bypass should be implemented or distributed. The
public loader must require user-supplied data from a legitimate installation.

## Important risks

### GLES version ambiguity

The manifest advertises OpenGL ES 3.0, while `libBaldursGate.so` directly links
to `libGLESv2.so` and imports a GLES2-style function set. It also contains
runtime GL capability/version checks and can resolve extra functions dynamically.

The first-frame prototype must determine whether a GLES2-compatible path is
actually selected. A mandatory GLES3-only shader or texture path is a pivot
condition.

### Memory

Static mapping is approximately 56 MiB, but the real risk is runtime heap,
decoded area art, PVRZ textures, audio, video, and caches. Memory instrumentation
must be added before testing large areas.

### Third-party repack

The inspected filename identifies an "unlocked-apkvision" repack. Its
`libstub.so` exports only `JNI_OnLoad` and contains a large Android UI/hook
surface. `libBaldursGate.so` does not list `libstub.so` as a direct dependency,
so the wrapper prototype should ignore `libstub.so`.

A clean official APK/OBB installation must be compared before release work.

## Why GemRB remains the fallback

GemRB 0.9.5 is open source and its March 2026 release notes say BG2EE Shadows of
Amn is known to be completable, though BG2EE support remains experimental.
The inspected data has the expected KEY/BIFF/TLK structure, so a GemRB data path
is credible.

GemRB is not selected first because:

- BG2EE remains experimental rather than polished;
- the inspected data also includes Throne of Bhaal, whose complete BG2EE path is
  not established by the 0.9.5 release claim;
- a new Vita platform port and dependency build would still be required;
- the original Android engine already contains the mobile UI and exact game
  behavior.

GemRB becomes the preferred route if the closed native engine cannot reach a
first frame without extensive binary patching.

## Direct-wrapper proof-of-concept gates

### Gate W1 — loader

- Load the ARMv7 ELF.
- Map all load segments and relocations.
- Resolve or intentionally stub all 417 imports.
- Run constructors without a crash.

### Gate W2 — JNI startup

- Provide a minimal JavaVM/JNIEnv table.
- Complete `JNI_OnLoad`.
- Supply SDL/Engine method IDs used during startup.
- Enter `SDLActivity_nativeInit`.

### Gate W3 — data

- Provide deterministic Vita paths for main data, language data, saves, cache,
  and configuration.
- Read `chitin.key`, `engine.lua`, and the selected `dialog.tlk`.
- Open BIFF data successfully.

### Gate W4 — first frame

- Replace native-window/EGL setup.
- Create a vitaGL context.
- Select the engine's GLES2-compatible path.
- Compile startup shaders.
- Render a recognizable main-menu frame.

### Gate W5 — memory

- Record mapped, heap, and graphics memory.
- Load the main menu and first playable area.
- Stay within safe Vita memory limits without progressive leakage.

## Automatic pivot conditions

Pivot to GemRB if any of these is demonstrated:

- the engine requires ARM instructions not supportable on Vita;
- startup requires an essential Java framework too large to reproduce;
- the rendering path requires unsupported GLES3 behavior with no fallback;
- unavoidable runtime memory exceeds Vita limits before gameplay;
- the library requires invasive version-specific binary patches before reaching
  the main menu;
- clean official files materially differ from the inspected native engine;
- lawful offline use cannot be separated from unavailable store services.

## Immediate implementation target

Create the Vita project skeleton and a host-side ELF audit:

1. define the import-resolution table by subsystem;
2. reuse an established Vita Android `.so` loader architecture;
3. implement logging and loader diagnostics;
4. map `libBaldursGate.so`;
5. stop at unresolved imports rather than silently stubbing them;
6. add minimal FalsoJNI-style SDL/Engine definitions;
7. target `JNI_OnLoad` as the first executable milestone.
