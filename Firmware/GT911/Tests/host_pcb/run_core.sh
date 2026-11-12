#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror -g -O1 \
 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer \
 -IApplication Application/PcbHeightMap.cpp Application/PcbProbeSequence.cpp \
 Tests/host_pcb/test_core.cpp -o "$TMP/pcb-core"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} UBSAN_OPTIONS=halt_on_error=1 "$TMP/pcb-core"
