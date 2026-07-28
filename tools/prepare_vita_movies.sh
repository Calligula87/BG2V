#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "Usage: $0 PATCH_OBB OUTPUT_DIRECTORY [MOVIE ...]" >&2
    echo "Example: $0 patch.5826.com.beamdog.baldursgateIIenhancededition.obb movies" >&2
    exit 2
fi

for tool in ffmpeg unzip; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Required tool not found: $tool" >&2
        exit 1
    fi
done

obb=$1
output_dir=$2
shift 2

if [[ ! -f "$obb" ]]; then
    echo "OBB not found: $obb" >&2
    exit 1
fi

if [[ $# -eq 0 ]]; then
    set -- logo.wbm intro.wbm intro15f.wbm
fi

mkdir -p "$output_dir"
temporary_dir=$(mktemp -d)
trap 'rm -rf "$temporary_dir"' EXIT

for movie in "$@"; do
    entry="movies/$movie"
    source_movie="$temporary_dir/$movie"
    output_movie="$output_dir/$movie"

    echo "Extracting $entry"
    if ! unzip -p "$obb" "$entry" >"$source_movie"; then
        echo "Movie not found in OBB: $entry" >&2
        exit 1
    fi

    echo "Optimizing $movie for PS Vita"
    ffmpeg -hide_banner -loglevel warning -y \
        -i "$source_movie" \
        -map 0:v:0 -map 0:a? \
        -vf "scale=640:360:flags=lanczos,fps=15" \
        -c:v libvpx -b:v 500k -crf 20 -deadline good -cpu-used 4 \
        -c:a copy \
        "$output_movie"
done

echo
echo "Prepared Vita movie overrides in: $output_dir"
echo "Copy the .wbm files to ux0:data/bg2v/movies/"
