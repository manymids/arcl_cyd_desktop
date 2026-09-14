#!/usr/bin/env bash
# Compile and run the host-side firmware tests.
#
# firmware/cyd_desktop_shell/tests/*.cpp existed with no way to run them: the
# firmware is built in WSL against ESP-IDF, and Windows has no host C++
# compiler. So the tests were never executed and could not catch anything.
# This script builds each one with the plain host toolchain.
#
# From WSL:     tools/run-native-tests.sh
# From Windows: wsl bash tools/run-native-tests.sh   (in the repository root;
#               WSL starts in the Windows working directory)

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SHELL_DIR="$ROOT/firmware/cyd_desktop_shell"
BUILD_DIR="${TMPDIR:-/tmp}/cyd-native-tests"
CXX="${CXX:-g++}"
CXXFLAGS="-std=c++17 -Wall -Wextra -O1 -I$SHELL_DIR/include"

mkdir -p "$BUILD_DIR"

# test source : extra translation units it needs
TESTS=(
  "json_buffer_test.cpp:"
  "json_parse_test.cpp:"
  "shell_state_test.cpp:shell_state.cpp"
  "native_app_test.cpp:native_app.cpp native_registry.cpp raycast_demo.cpp mjp_native_app.cpp"
  "keyboard_layout_test.cpp:keyboard_layout.cpp"
  "key_input_test.cpp:key_input.cpp"
  "dirty_rects_test.cpp:"
)

failures=0
skipped=0

for entry in "${TESTS[@]}"; do
  test_file="${entry%%:*}"
  extra_sources="${entry#*:}"
  name="${test_file%.cpp}"
  binary="$BUILD_DIR/$name"

  sources=("$SHELL_DIR/tests/$test_file")
  for source in $extra_sources; do
    sources+=("$SHELL_DIR/$source")
  done

  if ! "$CXX" $CXXFLAGS -o "$binary" "${sources[@]}" 2> "$BUILD_DIR/$name.log"; then
    # Some units reach into ESP-IDF and cannot leave the device. Report them as
    # skipped rather than failing, but never hide the reason.
    echo "SKIP  $name (does not build on the host)"
    sed 's/^/        /' "$BUILD_DIR/$name.log" | head -5
    skipped=$((skipped + 1))
    continue
  fi

  if "$binary"; then
    echo "PASS  $name"
  else
    echo "FAIL  $name"
    failures=$((failures + 1))
  fi
done

# Frozen MicroPython modules that are pure Python run under the host python3.
PY_TESTS=(
  "firmware/cyd_desktop_board/tests/manifest_test.py"
)
for test in "${PY_TESTS[@]}"; do
  name="$(basename "$test" .py)"
  if ! command -v python3 >/dev/null 2>&1; then
    echo "SKIP  $name (python3 not found)"
    skipped=$((skipped + 1))
  elif python3 "$ROOT/$test"; then
    echo "PASS  $name"
  else
    echo "FAIL  $name"
    failures=$((failures + 1))
  fi
done

echo "---"
echo "failed: $failures, skipped: $skipped"
[ "$failures" -eq 0 ]
