#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BUILD_TYPE="${BUILD_TYPE:-Release}"
CMAKE_BUILD_DIR="${SCRIPT_DIR}/build"
MAKE_BUILD_DIR="${SCRIPT_DIR}/build_make"

print_usage() {
  cat <<'EOF'
Usage:
  ./build.sh [--clean] [--test]

Options:
  --clean   Remove previous build output before building
  --test    Run roundtrip tests after build

Behavior:
  1. If cmake is available, use CMake build in ./build
  2. Otherwise, fall back to Makefile build in ./build_make

Environment:
  BUILD_TYPE=Release|Debug   Effective only for CMake builds
EOF
}

do_clean=0
do_test=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --clean)
      do_clean=1
      shift
      ;;
    --test)
      do_test=1
      shift
      ;;
    --help|-h)
      print_usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      print_usage >&2
      exit 2
      ;;
  esac
done

echo "Project dir: ${SCRIPT_DIR}"

if command -v cmake >/dev/null 2>&1; then
  echo "Build mode: CMake"
  if [[ ${do_clean} -eq 1 ]]; then
    rm -rf "${CMAKE_BUILD_DIR}"
  fi

  cmake -S "${SCRIPT_DIR}" -B "${CMAKE_BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
  cmake --build "${CMAKE_BUILD_DIR}" -j

  echo "Build done:"
  echo "  ${CMAKE_BUILD_DIR}/polygon_bench"
  echo "  ${CMAKE_BUILD_DIR}/polygon_bench_tests"

  if [[ ${do_test} -eq 1 ]]; then
    echo "Running tests..."
    "${CMAKE_BUILD_DIR}/polygon_bench_tests"
  fi
else
  echo "Build mode: Makefile"
  if [[ ${do_clean} -eq 1 ]]; then
    make -C "${SCRIPT_DIR}" clean
  fi

  make -C "${SCRIPT_DIR}" -j

  echo "Build done:"
  echo "  ${MAKE_BUILD_DIR}/polygon_bench"
  echo "  ${MAKE_BUILD_DIR}/polygon_bench_tests"

  if [[ ${do_test} -eq 1 ]]; then
    echo "Running tests..."
    "${MAKE_BUILD_DIR}/polygon_bench_tests"
  fi
fi

