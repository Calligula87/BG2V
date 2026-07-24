# Baldur's Gate II Android APK — preliminary Vita feasibility

Date: 2026-07-24

## Sample

- File: `Baldurs-Gate-II-v2.6.6.13-unlocked-apkvision.apk`
- Size: 3,823,610,529 bytes
- SHA-256: `EF1A19DF0A81D4E0FDD1FA8AF509B805DE6496625594A6CA6F382B50948E5373`
- Claimed package version in filename: 2.6.6.13
- Embedded OBB version in filenames: 5826

The filename indicates a third-party "unlocked" repack. This audit does not establish
the file's provenance or whether its executable matches Beamdog's official package.
A clean APK and OBBs extracted from a legally purchased installation should be used
for development and release validation.

## Package inventory

| Entry | ABI | Uncompressed size |
|---|---:|---:|
| `libBaldursGate.so` | armeabi-v7a | 9,656,544 |
| `libBaldursGate.so` | arm64-v8a | 13,169,032 |
| `libopenal.so` | armeabi-v7a | 1,050,568 |
| `libyuv.so` | armeabi-v7a | 263,780 |
| `libstub.so` | armeabi-v7a | 531,884 |
| `classes.dex` | — | 8,747,216 |
| `classes2.dex` | — | 1,595,832 |
| main OBB | — | 2,004,576,250 |
| patch OBB | — | 1,801,925,724 |

The complete data payload is embedded in the APK as:

- `assets/main.5826.com.beamdog.baldursgateIIenhancededition.obb`
- `assets/patch.5826.com.beamdog.baldursgateIIenhancededition.obb`

## Native engine findings

`libBaldursGate.so` is:

- ELF32 for ARM (`EM_ARM`, machine 40)
- ARM EABI5
- marked soft-float (`EF_ARM_ABI_FLOAT_SOFT`)
- linked to OpenGL ES 2
- linked to OpenAL
- built around SDL's Android platform layer
- exporting `JNI_OnLoad`
- exporting 32 SDL Activity/Input JNI methods

Direct dependencies:

1. `libopenal.so`
2. `libandroid.so`
3. `libGLESv2.so`
4. `liblog.so`
5. `libEGL.so`
6. `libm.so`
7. `libdl.so`
8. `libc.so`

There are 417 undefined dynamic symbols. Most are ordinary libc, libm, pthread,
socket, OpenAL, GLES2, and Android bionic compatibility calls. The only directly
imported Android native-window calls found are:

- `ANativeWindow_fromSurface`
- `ANativeWindow_release`
- `ANativeWindow_setBuffersGeometry`

The library exports roughly 29,310 symbols and appears to statically contain a
large part of its runtime, including SDL2, Lua, ZIP handling, video/audio codecs,
OpenSSL-era networking code, and game-engine code. This materially reduces the
number of separate Android libraries that a Vita loader must support.

## Graphics

The imported graphics surface is GLES2/EGL. It includes common GLES2 operations:
shader compilation, VBOs, textures, framebuffers, blending, stencil, scissor,
compressed textures, and readback.

This is broadly compatible with the established Vita Android-wrapper approach
using vitaGL. Risks requiring a live prototype:

- compressed texture formats (ATC/PVRTC/S3TC detection is present)
- shader compatibility
- framebuffer behavior
- texture memory pressure
- EGL/window assumptions

No mandatory GLES3 dependency was identified.

## Audio

The game dynamically imports a conventional subset of OpenAL/ALC and ships
`libopenal.so`. This is favorable. A wrapper can either adapt the bundled Android
OpenAL library or resolve calls to a Vita OpenAL-compatible implementation.

## Java/JNI boundary

The native library exports the standard SDL Android callbacks for:

- startup, pause, resume, quit, and low-memory events
- surface creation/change/destruction
- keyboard and text input
- mouse, touch, tap, pan, fling, and magnify
- joystick, hat, and gamepad input
- UI-cover and message callbacks
- imported-save handling

The DEX payload also contains Beamdog-specific DLC billing, license handling,
Google Play services, OBB-path handling, and Android UI code. Much of the large
DEX size is AndroidX, Material, Google Play, Firebase transport, and billing
library code rather than the game engine itself.

The native engine is clearly substantial and self-contained; the Java layer is
not the primary game implementation. For an offline Vita port, nonessential
Google Play, telemetry, billing UI, location, and online-service behavior should
be omitted from the port rather than recreated. Development must not depend on
circumventing purchase checks in a redistributed package; users should supply
data from their own legitimate installation.

## Preliminary verdict

### Direct Android wrapper: **technically plausible**

Confidence: medium.

Positive evidence:

- native ARMv7 engine is present
- correct 32-bit ARM EABI family
- soft-float marking is favorable for the Vita `.so` loader ecosystem
- GLES2 rather than mandatory GLES3
- SDL2 Android interface
- conventional OpenAL interface
- unusually self-contained main library
- all large game-data files are present

Unresolved risks:

- exact JNI methods called from native code
- licensing/DLC startup behavior in a clean official package
- OBB extraction/mounting and expected paths
- memory footprint on Vita
- compiled ARM instruction compatibility and runtime patches
- shader/texture compatibility with vitaGL
- movies and codec memory use
- online/multiplayer code paths
- behavior of `libstub.so` in this third-party repack

### GemRB Vita port: **plausible fallback and maintainable alternative**

The embedded OBBs likely contain the data needed for a desktop GemRB BG2EE test.
GemRB currently considers BG2EE completable but experimental. A desktop data test
should be conducted before selecting this route.

## Recommended next gates

1. Obtain a clean official APK/OBB set and compare hashes, libraries, and manifest.
2. Decode the manifest and DEX fully with `apktool`/`jadx`.
3. Enumerate native-to-Java `Call*Method` targets, not only exported JNI methods.
4. Inspect OBB directory trees without committing or distributing their contents.
5. Test the extracted data with current desktop GemRB.
6. Build a minimal Vita ARM `.so` loader skeleton.
7. Attempt to load `libBaldursGate.so` and resolve all 417 imports.
8. Implement only enough fake JNI/SDL lifecycle to reach `JNI_OnLoad` and
   `SDLActivity_nativeInit`.
9. Replace EGL/native-window behavior with the Vita display/vitaGL bridge.
10. Make a final wrapper-versus-GemRB decision after the first-frame attempt.

## Current go/no-go

- CPU gate: **GO**
- native-engine gate: **GO**
- graphics API gate: **GO, prototype required**
- audio API gate: **GO**
- data-availability gate: **GO**
- Java-dependency gate: **provisional GO**
- memory gate: **unknown**
- clean/legal input gate: **not yet satisfied**
- overall: **GO to a bounded proof of concept; not yet a full-port commitment**

## Follow-up decision

The embedded OBB and complete manifest/DEX interface were subsequently
inspected. The selected next route is a bounded direct Android ARMv7 wrapper
proof of concept, with automatic pivot conditions to GemRB. See
[`ROUTE_DECISION.md`](ROUTE_DECISION.md).
