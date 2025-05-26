# TODO

1. **Integrate NodeConfig across modules**
   - Parse `rxrevolt_node.conf` in `main.cpp` and pass values to `PinnerNode`, `DailyScheduler` and `P2PNode`.
   - Use config fields for data directory, IPFS endpoint, scheduler interval and network ports.

2. **Complete daily merge pipeline**
   - Persist `DocumentQueue` to disk (virtual WAL) so data survives restarts.
   - Compress document payloads before insertion into SQLite.
   - Ensure `DailySnapshot` updates `PinnedState` with the CID returned from `IPFSPinner`.

3. **Connect P2P networking**
   - Start a `P2PNode` inside `PinnerNode` and implement peer discovery.
   - Broadcast snapshot announcements and handle PoP request/response messages.

4. **Finalize Proof‑of‑Pinning logic**
   - Generate Merkle based proofs with `MerkleProof` and validate them in `PoPConsensus`.
   - Track challenge history and store passing nodes for reward calculation.

5. **Implement reward distribution storage**
   - Persist node streaks and minted tokens so rewards survive restarts.
   - Expose simple wallet/address handling for nodes.

6. **Add privacy and removal features**
   - Integrate `PrivacyManager` into `DailySnapshot` for automatic PII stripping.
   - Provide user‑initiated removal transactions and enforce them during merges.

7. **Governance modules**
   - Hook up `ContentModeration` and `UpgradeManager` through a `ServiceManager` or RPC interface.
   - Allow proposals, voting and upgrade activation according to the whitepaper rules.

8. **Public query endpoints**
   - Build a read‑only HTTP or gRPC service exposing metrics and simple queries on the pinned database.
   - Document how non‑nodes can download snapshots from IPFS.

9. **Reputation model and advanced PoP**
   - Refine reward logic based on long‑term node streaks and penalize failed proofs.
   - Increase randomness of PoP challenges and explore encrypted or zero‑knowledge options.

10. **Scaling and external integrations**
    - Plan connectors for EHR or insurance systems to auto‑submit data.
    - Consider partitioning the database when it grows large and manage long‑term maintenance tasks.

11. **Comprehensive test suite**
    - Expand GoogleTest coverage for each module (IPFSPinner, P2P networking, PoP flows, etc.).
    - Provide integration tests that simulate multiple nodes and reward distribution.

12. **Documentation and tooling**
    - Extend `README.md` with full build, run and configuration instructions.
    - Provide scripts for setting up IPFS and running a local network of nodes.

