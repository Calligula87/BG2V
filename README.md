# BG2VBETA

BG2VBETA is a community-made PlayStation Vita port of **Baldur's Gate II:
Enhanced Edition**. It lets the game run on a homebrew-capable Vita with Vita
controls, touchscreen support, and Vita-optimized startup videos.

This project is a work in progress. Compatibility and performance may vary by
Vita and by Android game version.

## Installation

You need `BG2v0_beta.vpk`, your own Baldur's Gate II Android APK, a Windows PC,
and a homebrew-capable Vita with VitaShell.

1. Download and extract the **BG2V Windows Setup** package.
2. Double-click **Prepare BG2V.bat**.
3. Select your Baldur's Gate II APK when asked.
4. Wait until the setup says **Success**. It will open a folder containing a
   new folder named `bg2v`.
5. Open VitaShell on the Vita and press **SELECT** to start its FTP server.
6. Connect to the address displayed by VitaShell using an FTP program on the
   PC.
7. Copy the complete `bg2v` folder into `ux0:data/` on the Vita. The final path
   must be `ux0:data/bg2v/`.
8. Copy `BG2v0_beta.vpk` to the Vita and install it with VitaShell.
9. Launch **BG2VBETA** from the Vita home screen.

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
file. GitHub Issues will be available to everyone once the repository is made
public.
