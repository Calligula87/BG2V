#!/usr/bin/env bash
set -euo pipefail

VITASDK_SOFTFP="${VITASDK_SOFTFP:-$HOME/vitasdk-softfp}"
VDPM_DIR="${VDPM_DIR:-$HOME/bg2v-vdpm-softfp}"

if [[ -e "$VITASDK_SOFTFP" ]]; then
    echo "Refusing to overwrite existing path: $VITASDK_SOFTFP" >&2
    exit 2
fi

if [[ ! -d "$VDPM_DIR/.git" ]]; then
    git clone --depth 1 https://github.com/vitasdk-softfp/vdpm.git "$VDPM_DIR"
fi

export VITASDK="$VITASDK_SOFTFP"
export PATH="$VITASDK/bin:$PATH"

# vdpm's bootstrap invokes sudo even for a user-owned destination. Creating the
# isolated directory first lets its official installer extract without sudo.
mkdir -p "$VITASDK"
# shellcheck source=/dev/null
source "$VDPM_DIR/include/install-vitasdk.sh"
install_vitasdk "$VITASDK"
"$VDPM_DIR/install-all.sh"

echo
echo "VitaSDK-softfp installed at: $VITASDK"
"$VITASDK/bin/arm-vita-eabi-gcc" -v 2>&1 | grep -- "--with-float="
