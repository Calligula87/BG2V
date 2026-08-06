# BG2V installation

This guide prepares a legally obtained Baldur's Gate II: Enhanced Edition
Android APK for the Vita wrapper. The game APK and its data are not included
in the VPK or repository.

## Follow these steps

### On the Windows PC

1. Install Python 3 if it is not already installed.
2. Download the BG2V repository as a ZIP and extract it, or clone it with Git.
3. Download `BG2v0_beta.vpk` from the project release page.
4. Obtain your own legally purchased BG2 Enhanced Edition APK. Do not rename
   or modify the APK.
5. Open PowerShell in the extracted repository folder. For example:

   ```powershell
   cd "C:\path\to\BG2V"
   ```

6. Run the preparation script, replacing the APK path with the real path:

   ```powershell
   python tools\prepare_vita_data.py "C:\path\to\BG2.apk" ".\bg2v-data"
   ```

7. Wait for `Prepared Vita data in ...` to appear. The new `bg2v-data` folder
   is the only folder that must be copied to the Vita.

### On the Vita

8. Open VitaShell and press **SELECT** to start its FTP server. Note the IP
   address and port shown on the Vita.
9. In an FTP program on the PC, connect to that address.
10. Open `ux0:data/`. Create a folder named `bg2v` if it does not exist.
11. Copy everything inside the PC's `bg2v-data` folder into `ux0:data/bg2v/`.
12. Copy `BG2v0_beta.vpk` to any convenient Vita location, such as `ux0:data/`.
13. In VitaShell, select the VPK and press **X** to install it.
14. Return to the Vita home screen and launch **BG2VBETA**.

The preparation script treats the APK as a ZIP archive and extracts the
required native library and embedded OBB expansion files. It also keeps a copy
of the APK as `game.apk` because the compatibility layer exposes that path to
the engine.

The output should contain:

```text
bg2v-data\
├── game.apk
├── libBaldursGate.so
├── main.5826.com.beamdog.baldursgateIIenhancededition.obb
├── patch.5826.com.beamdog.baldursgateIIenhancededition.obb
└── assets\              (when the APK contains additional assets)
```

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
