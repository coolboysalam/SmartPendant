#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
c++ -std=c++14 -Wall -Wextra -Werror \
 -ITests/host_gt911/stubs -IApplication \
 Application/GT911.cpp Application/AutoTouch.cpp \
 Tests/host_gt911/test_gt911.cpp -o /tmp/gt911_host_tests
/tmp/gt911_host_tests
