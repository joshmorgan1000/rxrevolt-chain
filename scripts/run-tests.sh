#!/bin/bash
set -e
build_type=${1:-Release}
script_dir=$(cd "$(dirname "$0")" && pwd)
root_dir=$(dirname "$script_dir")
build_dir="$root_dir/build"
mkdir -p "$build_dir"
cd "$build_dir"
cmake -DCMAKE_BUILD_TYPE=$build_type ..
cmake --build .
ctest --output-on-failure
