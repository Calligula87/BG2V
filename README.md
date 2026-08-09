# BG2VBETA

BG2VBETA is a community-made PlayStation Vita port of **Baldur's Gate II:
Enhanced Edition**. It lets the game run on a homebrew-capable Vita with Vita
controls, touchscreen support, and Vita-optimized startup videos.

This project is a work in progress. Compatibility and performance may vary by
Vita and by Android game version.

## Installation

You need `BG2v0_beta.vpk`, your own Baldur's Gate II Android APK, the BG2V
Windows Setup package, and a homebrew-capable Vita with VitaShell.

1. Double-click **Prepare BG2V.bat**.
2. Select your legally obtained Baldur's Gate II Android APK.
3. Wait for the success message.
4. Copy the generated `bg2v` folder into `ux0:data/` on your Vita.
5. Install `BG2v0_beta.vpk` with VitaShell and launch **BG2VBETA**.

The Windows Setup uses tools already included with Windows. Python, WSL, and
command-line knowledge are not required.

Advanced users can still use `tools/prepare_vita_data.py` for manual APK data
preparation and `tools/prepare_vita_movies.sh` to create optional smoother
intro videos.

## Project policy

The game, APK, OBB files, and other copyrighted game data are not distributed
by this project. Please use files from your own legitimate installation.

## Troubleshooting

If you encounter a problem, please open an issue in this repository with a
description of what happened and, when possible, the `ux0:data/bg2v/bootstrap.log`
file. 