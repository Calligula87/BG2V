# BG2V installation

This guide prepares a legally obtained Baldur's Gate II: Enhanced Edition
Android APK for the Vita wrapper. The game APK and its data are not included
in the VPK or repository.

## What you need

- A homebrew-capable Vita with VitaShell and `kubridge.skprx` installed.
- `BG2v0_beta.vpk` from the project release.
- Your own matching 32-bit Android APK (the ARMv7 build).
- A Windows PC with Python 3 installed.

## Prepare the data on Windows

From a copy of the repository, run:

```powershell
python tools/prepare_vita_data.py "C:\path\to\your\BG2.apk" ".\bg2v-data"
```

The script treats the APK as a ZIP archive and extracts the required native
library and the embedded OBB expansion files. It also keeps a copy of the APK
as `game.apk` because the compatibility layer exposes that path to the engine.

The output should contain:

```text
bg2v-data\
├── game.apk
├── libBaldursGate.so
├── main.5826.com.beamdog.baldursgateIIenhancededition.obb
├── patch.5826.com.beamdog.baldursgateIIenhancededition.obb
└── assets\              (when the APK contains additional assets)
```

## Copy data and install

1. Start VitaShell and press **SELECT** to start its FTP server.
2. Create `ux0:data/bg2v/` if it does not already exist.
3. Copy the *contents* of `bg2v-data` into that directory.
4. Copy `BG2v0_beta.vpk` to the Vita and install it with VitaShell.
5. Launch **BG2VBETA**.

The OBB filenames must remain unchanged and must match the APK version. Do not
copy an ARM64 library; BG2V requires `lib/armeabi-v7a/libBaldursGate.so`.

## Optional movie optimization

The game runs without movie overrides. To reduce startup-video stutter, use
`tools/prepare_vita_movies.sh` with the patch OBB and copy the generated `.wbm`
files to `ux0:data/bg2v/movies/`. These files remain user-supplied and are not
redistributed by the project.

## Troubleshooting

If the game stops at startup, send `ux0:data/bg2v/bootstrap.log`. Confirm that
the two OBB files are in `ux0:data/bg2v/` (not inside another nested folder),
that the library is ARMv7, and that all files came from the same APK release.
