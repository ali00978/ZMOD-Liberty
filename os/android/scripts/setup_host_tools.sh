#!/bin/bash
# Host shader tools the gta4-native override cache needs at build time.
#   - DXC: the copy bundled in tools/XenosRecomp (1.8.2407) crashes on the
#     Bink pixel shaders, so a newer official release is used.
#   - glslangValidator: GLSL overrides (motion blur).
#   - spirv-val ships with the NDK and needs no download.
# Everything lands in out/host-tools inside the repository.
set -euo pipefail
. "$(dirname "$0")/env.sh"
DEST="$REPO/out/host-tools"
DXC_URL="https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2609/dxc_2026_09_29.zip"
GLSLANG_URL="https://github.com/KhronosGroup/glslang/releases/download/16.6.0/glslang-16.6.0-windows-x86_64-release.zip"
mkdir -p "$DEST"
cd "$DEST"

# Release asset CDN occasionally returns 502/503/504 on GitHub Windows
# runners. Do not throw away an entire multi-minute native build for one
# transient HTTP failure. A failed attempt must never leave a partial ZIP
# that a later build mistakes for a valid tool.
download_release_zip() {
  local url="$1" output="$2"
  local attempt
  for attempt in 1 2 3 4 5; do
    rm -f "$output"
    if curl --fail --location --silent --show-error \
        --connect-timeout 20 --max-time 180 \
        --retry 3 --retry-delay 2 --retry-max-time 180 \
        --retry-all-errors --output "$output" "$url" && \
        unzip -tqq "$output" >/dev/null; then
      return 0
    fi
    echo "!! release ZIP download failed (attempt $attempt/5): $url"
    rm -f "$output"
    if [ "$attempt" -lt 5 ]; then sleep $((attempt * 3)); fi
  done
  echo "!! could not fetch a valid host shader tool archive: $url" >&2
  return 1
}

if [ ! -x dxc/bin/x64/dxc.exe ]; then
  download_release_zip "$DXC_URL" dxc.zip
  # The archive uses backslash separators, which unzip mangles.
  powershell -NoProfile -Command "Expand-Archive -Force dxc.zip dxc"
fi
if [ ! -x glslang/bin/glslangValidator.exe ]; then
  download_release_zip "$GLSLANG_URL" glslang.zip
  unzip -q -o glslang.zip -d glslang
  cp glslang/bin/glslang.exe glslang/bin/glslangValidator.exe
fi
dxc/bin/x64/dxc.exe --version
glslang/bin/glslangValidator.exe --version | head -1
