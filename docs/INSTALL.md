# BG2V installation

## Easy Windows setup

You need `BG2v0_beta.vpk`, your own Baldur's Gate II Android APK, a Windows PC,
and a Vita with VitaShell.

1. Download and extract the BG2V Windows Setup package.
2. Double-click **Prepare BG2V.bat**.
3. Select your Baldur's Gate II APK when asked.
4. Wait until the setup says **Success**. It will open a new folder containing
   a folder named `bg2v`.
5. Open VitaShell on the Vita and press **SELECT** to start FTP.
6. Connect to the address shown by VitaShell using an FTP program on the PC.
7. Copy the complete `bg2v` folder into `ux0:data/` on the Vita. The final path
   must be `ux0:data/bg2v/`.
8. Copy `BG2v0_beta.vpk` to the Vita and install it with VitaShell.
9. Launch **BG2VBETA** from the Vita home screen.

The setup uses Windows' built-in tools. Python, WSL, and command-line knowledge
are not required.

## Advanced/manual setup

The original Python and WSL preparation tools remain available for advanced
users and developers:

- `tools/prepare_vita_data.py` extracts the required APK data.
- `tools/prepare_vita_movies.sh` creates optional smoother intro videos.

These tools are not required when using the Windows Setup package.
