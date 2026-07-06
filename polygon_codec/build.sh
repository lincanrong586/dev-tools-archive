#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

MODE="build"
RUN_TEST=0

for arg in "$@"; do
  case "$arg" in
    --clean) MODE="clean" ;;
    --test) RUN_TEST=1 ;;
    *) ;;
  esac
done

if [[ "$MODE" == "clean" ]]; then
  rm -rf "$ROOT_DIR/build" "$ROOT_DIR/build_make"
fi

USE_CMAKE=0
if command -v cmake >/dev/null 2>&1; then
  USE_CMAKE=1
fi

if [[ "$USE_CMAKE" == "1" ]]; then
  cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$ROOT_DIR/build" -j
  if [[ "$RUN_TEST" == "1" ]]; then
    "$ROOT_DIR/build/polygon_codec_tests"
  fi
else
  make -C "$ROOT_DIR" clean
  make -C "$ROOT_DIR" all
  if [[ "$RUN_TEST" == "1" ]]; then
    make -C "$ROOT_DIR" test
  fi
fi

