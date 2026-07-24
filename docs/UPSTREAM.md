# Upstream foundation

The wrapper foundation was imported from `v-atamanenko/soloader-boilerplate`
and remains under its MIT license. Its copyright notice is preserved in
`LICENSE` and the copied source files.

Pinned Git submodules:

- `v-atamanenko/FalsoJNI`
- `Rinnegatamante/so_util`
- `elliencode/FalsoNDK` (kept for upstream compatibility; BG2V builds with
  `NDK_PORT=OFF`)
- `Rinnegatamante/vitaGL`

The BG2-specific configuration uses title ID `BG2V00001`, data directory
`ux0:data/bg2v/`, and shared object
`ux0:data/bg2v/libBaldursGate.so`.
