#!/usr/bin/env bash
#
# ipfs_bootstrap.sh - A script to set up or connect to an IPFS daemon for RxRevoltChain usage.
#
# USAGE:
#   ./ipfs_bootstrap.sh
#
# This script will:
#   1) Initialize a new IPFS repo if none exists.
#   2) Start the IPFS daemon in the background.
#   3) Optionally add known bootstrap peers or do custom commands.
#

set -e  # Exit on any error

# 1) Check if IPFS is installed
if ! command -v ipfs &> /dev/null
then
    echo "Error: ipfs could not be found. Please install IPFS (https://docs.ipfs.io/install/)."
    exit 1
fi

# 2) Initialize IPFS if needed
if [ ! -d "$HOME/.ipfs" ]; then
  echo "Initializing IPFS..."
  ipfs init
else
  echo "IPFS repo already exists, skipping init."
fi

# 3) Start IPFS daemon in background
echo "Starting IPFS daemon..."
ipfs daemon &
DAEMON_PID=$!

echo "IPFS daemon started with PID=$DAEMON_PID"
sleep 5  # give it a few seconds to spin up

# 4) Add or remove bootstrap peers if desired
# Example: ipfs bootstrap rm --all
# Then add specific peers or a private swarm address
# ipfs bootstrap add /ip4/127.0.0.1/tcp/4001/ipfs/XXXXXX
echo "Adjusting IPFS bootstrap peers if needed..."
# By default, we do nothing. If you have custom peers, add them here:
# ipfs bootstrap rm --all
# ipfs bootstrap add /ip4/192.168.1.100/tcp/4001/ipfs/QmSomePeerID
# For demonstration, we skip.

echo "IPFS bootstrap done. The daemon is running in the background."
echo "Press Ctrl-C or kill $DAEMON_PID to stop it."

# Wait for the daemon process to finish
wait $DAEMON_PID
