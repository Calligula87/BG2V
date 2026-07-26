#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export VITASDK="${VITASDK_SOFTFP:-$HOME/vitasdk-softfp}"
export PATH="$VITASDK/bin:$PATH"

if [[ ! -x "$VITASDK/bin/arm-vita-eabi-gcc" ]]; then
    echo "VitaSDK-softfp is missing. Run tools/setup_wsl_softfp.sh first." >&2
    exit 2
fi

compiler_config="$("$VITASDK/bin/arm-vita-eabi-gcc" -v 2>&1)"
if [[ "$compiler_config" != *"--with-float=soft"* ]]; then
    echo "Refusing to build with a non-soft-float Vita compiler." >&2
    exit 3
fi

NATIVE_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/bg2v-build.XXXXXX")"
trap 'rm -rf "$NATIVE_ROOT"' EXIT
NATIVE_SOURCE="$NATIVE_ROOT/source"
NATIVE_BUILD="$NATIVE_ROOT/build"
mkdir -p "$NATIVE_SOURCE" "$PROJECT_DIR/artifacts"

# vita-pack-vpk does not correctly quote Windows paths containing spaces.
# Stage only the open-source project files on WSL's native filesystem.
tar -C "$PROJECT_DIR" \
    --exclude=.git \
    --exclude=analysis \
    --exclude=artifacts \
    --exclude=build-wsl \
    --exclude=input \
    -cf - . | tar -C "$NATIVE_SOURCE" -xf -

cmake -S "$NATIVE_SOURCE" -B "$NATIVE_BUILD" \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build "$NATIVE_BUILD" --parallel "${BG2V_BUILD_JOBS:-4}"

cp "$NATIVE_BUILD/BG2V.vpk" "$PROJECT_DIR/artifacts/BG2V-debug.vpk"
cp "$NATIVE_BUILD/eboot.bin" "$PROJECT_DIR/artifacts/eboot-debug.bin"
cp "$NATIVE_BUILD/bg2v" "$PROJECT_DIR/artifacts/bg2v-debug.elf"
sha256sum "$PROJECT_DIR/artifacts/BG2V-debug.vpk" \
    "$PROJECT_DIR/artifacts/eboot-debug.bin" \
    "$PROJECT_DIR/artifacts/bg2v-debug.elf"
