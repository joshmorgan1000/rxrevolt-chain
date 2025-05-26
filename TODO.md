#TODO

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
