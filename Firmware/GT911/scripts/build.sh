#!/usr/bin/env bash
# Build experimental GT911 firmware from the hardware fork overlay.
# NEVER flashes a physical board. Requires git, c++, ARM GCC and CMake >=4.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OVERLAY="$ROOT/Firmware/GT911"
FW_REF="af27c8e6f829295ef873784aa286e29e3156f134"
DEVCORE_REF="07bb40292059bc81b2b8a6b13f01da24b783f2d8"
SRC="$ROOT/build/gt911/firmware"
OUT="$ROOT/build/gt911/out"
ART="$ROOT/build/gt911/artifacts"

for command in git cmake c++ arm-none-eabi-gcc arm-none-eabi-g++ arm-none-eabi-objcopy; do
  command -v "$command" >/dev/null 2>&1 || { echo "ERROR: missing $command" >&2; exit 2; }
done

if [[ -e "$SRC" ]]; then
  echo "ERROR: $SRC exists; set up a clean build directory by moving it out of the way." >&2
  exit 2
fi
mkdir -p "$(dirname "$SRC")"

git clone --quiet https://github.com/nickshl/SmartPendant.git "$SRC"
git -C "$SRC" checkout --quiet --detach "$FW_REF"
[[ "$(git -C "$SRC" rev-parse HEAD)" == "$FW_REF" ]] || { echo 'Wrong firmware revision' >&2; exit 2; }
git -C "$SRC" submodule update --init --recursive
[[ "$(git -C "$SRC/DevCore" rev-parse HEAD)" == "$DEVCORE_REF" ]] || { echo 'Wrong DevCore revision' >&2; exit 2; }

for name in AppMain.cpp GT911.h GT911.cpp AutoTouch.h AutoTouch.cpp; do
  cp "$OVERLAY/Application/$name" "$SRC/Application/$name"
done
git -C "$SRC" apply "$OVERLAY/scripts/integration.patch"
mkdir -p "$SRC/Tests"
cp -R "$OVERLAY/Tests/host_gt911" "$SRC/Tests/host_gt911"

echo "Running isolated host tests..."
(cd "$SRC" && sh Tests/host_gt911/run.sh)

echo "Building STM32F411CEUx firmware..."
cmake -S "$SRC" -B "$OUT" -DCMAKE_TOOLCHAIN_FILE="$SRC/cmake/arm_none_eabi_gcc.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build "$OUT" --parallel "${BUILD_JOBS:-4}"
for file in SmartPendant.elf SmartPendant_1.0.0.bin SmartPendant_1.0.0.hex; do
  [[ -s "$OUT/$file" ]] || { echo "Missing build output: $file" >&2; exit 2; }
done
mkdir -p "$ART"
cp "$OUT/SmartPendant.elf" "$OUT/SmartPendant_1.0.0.bin" "$OUT/SmartPendant_1.0.0.hex" "$ART/"
[[ -s "$OUT/SmartPendant.map" ]] && cp "$OUT/SmartPendant.map" "$ART/" || true
arm-none-eabi-size "$OUT/SmartPendant.elf"
sha256sum "$ART/SmartPendant_1.0.0.bin" "$ART/SmartPendant_1.0.0.hex"
echo "EXPERIMENTAL build complete. Artifacts: $ART (NOT hardware tested)."
