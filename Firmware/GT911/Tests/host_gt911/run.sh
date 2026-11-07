#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
set --
CORE=${DEVCORE_DIR:-"$ROOT/DevCore"}
if [ -f "$CORE/Display/FT6236.cpp" ]; then
  # Use the real FT6236 implementation alongside the mocked I2C and HAL.
  ln -s "$CORE/Display" "$TMP/Display"
  set -- -DTEST_UPSTREAM_FT6236 -I"$TMP" -idirafter "$CORE" -Wno-unused-parameter "$CORE/Display/FT6236.cpp"
fi
c++ -std=c++14 -Wall -Wextra -Werror "$@" \
 -ITests/host_gt911/stubs -IApplication \
 Application/GT911.cpp Application/AutoTouch.cpp \
 Tests/host_gt911/test_gt911.cpp -o "$TMP/test_gt911"
"$TMP/test_gt911"
