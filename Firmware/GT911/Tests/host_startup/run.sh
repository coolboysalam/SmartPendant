#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
c++ -std=c++14 -Wall -Wextra -Werror \
 -ITests/host_startup/stubs -IApplication \
 Application/StartupSplash.cpp Tests/host_startup/test_startup.cpp \
 -o "$TMP/test_startup"
"$TMP/test_startup"
