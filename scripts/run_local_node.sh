#!/usr/bin/env bash
#
# run_local_node.sh - Launches a local RxRevoltChain node using an optional config file.
#
# USAGE:
#   ./run_local_node.sh [path_to_config]
#
# EXAMPLE:
#   ./run_local_node.sh rxrevolt_node.conf
#
# If no config file is specified, a default of "rxrevolt_node.conf" is assumed.
# This script requires that you have already built the project via build.sh.
#

set -e  # Exit on error

CONFIG_PATH="rxrevolt_node.conf"  # default config

# If the user passed a parameter, treat it as the config path
if [ ! -z "$1" ]; then
  CONFIG_PATH="$1"
fi

# Ensure the binary exists
if [ ! -f "./build/rxrevoltchain" ]; then
  echo "Error: ./build/rxrevoltchain not found. Have you run ./build.sh?"
  exit 1
fi

echo "Starting RxRevoltChain node with config: $CONFIG_PATH"
./build/rxrevoltchain --config="$CONFIG_PATH"
