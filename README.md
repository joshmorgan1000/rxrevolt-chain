#rxrevolt - chain

The back-end proof-of-pin service for [The RxRevolt Project.](https://github.com/joshmorgan1000/RxRevolt)

For more information, please see [the whitepaper.](https://github.com/joshmorgan1000/rxrevolt-chain/blob/main/docs/WHITEPAPER.md)

## Building

Check out the repository and then build using cmake or the helper script:

```bash
#Manual build
git clone https://github.com/joshmorgan1000/rxrevolt-chain.git
cd rxrevolt-chain
mkdir build
cd build
cmake ..
make -j$(nproc)
```

You can also use `./scripts/build.sh` which performs the steps above automatically.

## Running a local node

After building, you can start a node using the provided helper script.  The
node expects a configuration file describing networking and data directories.
A sample config is available at `scripts/rxrevolt_node.conf`.

```bash
#Start a node with the sample config
./scripts/run_local_node.sh scripts/rxrevolt_node.conf
```

This script launches the `rxrevoltchain` binary from the `build` directory and
passes the configuration file path.  Adjust the config values as needed.

## Configuration

Configuration values are parsed from a simple `key=value` file.  Important
settings include:

- `p2pPort` – TCP port used for peer‑to‑peer connections.
- `maxConnections` – maximum number of peers.
- `dataDirectory` – where the chain stores its data and logs.
- `nodeName` – a friendly name shown in logs and peer handshakes.

Modify `scripts/rxrevolt_node.conf` or provide your own file when starting the
node.

## IPFS setup

RxRevoltChain pins daily snapshots to IPFS.  The repository contains a helper
script `scripts/ipfs_bootstrap.sh` which initializes and starts a local IPFS
daemon if one is not already running:

```bash
./scripts/ipfs_bootstrap.sh
```

Run this prior to launching your node if you do not already have an IPFS daemon
running on your machine.

## Downloading pinned snapshots

If you are not running a node but wish to inspect the data, you can download the
latest snapshot directly from IPFS.  Public nodes announce the current CID of
the `.sqlite` file.  With the [IPFS](https://docs.ipfs.io/) CLI installed you
can fetch the file via:

```bash
ipfs get <CID> -o snapshot.sqlite
```

If you do not have a daemon running locally, you may also use a public gateway:

```bash
curl -L https://ipfs.io/ipfs/<CID> -o snapshot.sqlite
```

After downloading simply open `snapshot.sqlite` with any SQLite tool to run
offline queries.

## Running the test suite

Unit tests are built and executed via CMake.  Use the helper script to build in
either `Debug` or `Release` mode (default `Release`) and run all tests:

```bash
./scripts/run-tests.sh [Debug|Release]
```

The script configures a build directory, compiles the project and then runs the
GoogleTest suite.
