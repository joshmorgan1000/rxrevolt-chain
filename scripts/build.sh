#!/usr/bin/env bash
#
# build.sh - A simple script to create a build directory, run CMake, and compile.
#
# USAGE:
#   ./build.sh
#
# PREREQUISITES:
#   - CMake 3.12+ (or suitable version matching your top-level CMakeLists.txt)
#   - A C++ compiler (e.g., g++, clang++)
#
# This script will:
#   1) Create a 'build' directory (if not present).
#   2) cd into 'build'.
#   3) Invoke cmake .. to configure.
#   4) Compile the sources with make (or ninja, if you prefer).
#   5) Place the binaries in the 'build' folder.
#

set -e  # Exit on first error

# 1) Create build directory if needed
if [ ! -d "build" ]; then
  mkdir build
fi

# 2) Enter build directory
cd build

# 3) Run cmake to configure
echo "Running CMake..."
cmake ..

# 4) Compile
echo "Compiling RxRevoltChain..."
make -j4

echo "Build completed successfully. Binaries are located in the 'build' directory."
