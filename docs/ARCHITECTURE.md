# RxRevoltChain — ARCHITECTURE

**Version:** 0.3

## 1. Overview

**RxRevoltChain** is a custom blockchain designed exclusively to incentivize nodes (miners) to **pin IPFS data** for healthcare transparency. The core motivation is to ensure that healthcare cost data (bills, EOBs, etc.) are available, tamper-evident, privacy-sanitized, and easy for patients or researchers to access.

### High-Level Goals
1. **Promote Data Availability:** Use a novel **Proof-of-Pinning (PoP)** mechanism to reward nodes that truly store the healthcare cost data.  
2. **Ensure Privacy:** By removing or redacting personally identifiable information (PII) from documents before they are pinned. This is done both client-side and checked again by the miners.  
3. **Simplify Blockchain Overhead:** Avoid heavy transaction loads—most of the chain references data via IPFS CIDs.  
4. **IPFS Integration:** Nodes rely on a **direct IPFS library** approach to pin/unpin data, ensuring close control over chunking and verification.  
5. **Node Reputation System:** Nodes can gain or lose reputation based on performance. If they perform poorly enough, they lose the ability to mine (but keep any tokens they have).  
6. **Non-Profit Ethos:** The project aims to be open, free, and non-commercial, fostering an ecosystem that primarily benefits patients and transparency initiatives.

> **Governance Choice:** A **hybrid model** (multi-sig + community vote) handles malicious data, major upgrades, and big policy changes.

---

## 2. System Components

```
rxrevoltchain/
├─ CMakeLists.txt                // Build instructions
├─ README.md                     // Project overview and build/run instructions
├─ docs/
│   ├─ ARCHITECTURE.md          // More detailed architecture explanations
│   ├─ SPECIFICATIONS.md        // Protocol specs, tokenomics, governance rules
│   └─ DESIGN_DECISIONS.md      // Rationale behind certain design choices
├─ config/
│   ├─ chainparams.hpp          // Network parameters (block time, reward, etc.)
│   └─ node_config.hpp          // Local node config (ports, data dirs, etc.)
├─ src/
│   ├─ core/
│   │   ├─ block.hpp            // Definition of the Block structure & validation (header-only)
│   │   ├─ transaction.hpp      // Basic value transfers + PoP references
│   │   ├─ chainstate.hpp       // Chain state tracking & database references (header-only)
│   │   ├─ ledger.hpp           // Abstract ledger interfaces for PoP logs, etc. (header-only)
│   │   └─ sync_manager.hpp     // Logic for block synchronization across the network (header-only)
│   ├─ consensus/
│   │   ├─ pop_consensus.hpp    // Core "Proof-of-Pinning" logic, challenge-response
│   │   ├─ cid_randomness.hpp   // Random CID selection (VRF, block-hash based, etc.)
│   │   └─ block_validation.hpp // High-level block acceptance rules (punish invalid proofs)
│   ├─ ipfs_integration/
│   │   ├─ ipfs_pinner.hpp      // Communication with IPFS node (direct library calls)
│   │   ├─ merkle_proof.hpp     // Merkle proofs for chunk data verification
│   │   └─ cid_registry.hpp     // On-chain registry of CIDs, referencing pinned healthcare files
│   ├─ network/
│   │   ├─ p2p_node.hpp         // Peer-to-peer networking, message handling
│   │   ├─ protocol_messages.hpp// Data structures for block/tx/proof messages
│   │   └─ connection_manager.hpp// Connection pooling, session management
│   ├─ miner/
│   │   ├─ pop_miner.hpp        // The "pinning miner" logic, building candidate blocks
│   │   ├─ proof_generator.hpp  // Generates proof of pinned chunks, includes chunk sampling
│   │   └─ reward_schedule.hpp  // Logic for block rewards (variable, based on network usage)
│   ├─ governance/
│   │   ├─ cid_moderation.hpp   // Hybrid multi-sig + community voting for removing bad CIDs
│   │   ├─ upgrade_manager.hpp  // Soft-fork approach with miner signaling
│   │   └─ anti_spam.hpp        // Built-in consensus measures to deter spam
│   ├─ validation_queue/
│   │   ├─ document_queue.hpp   // Queue where new documents (bills/EOBs) are placed
│   │   ├─ pii_stripper.hpp     // Ensures personally identifiable information is scrubbed
│   │   └─ document_verifier.hpp// Rudimentary consensus checks for authenticity (community moderation)
│   ├─ util/
│   │   ├─ logger.hpp           // Logging utilities (header-only)
│   │   ├─ hashing.hpp          // Cryptographic hashing routines (header-only)
│   │   ├─ thread_pool.hpp      // Concurrency helpers for PoP checks, pinning tasks (header-only)
│   │   └─ config_parser.hpp    // Reads/validates node_config.hpp at runtime (header-only)
│   ├─ service/
│   │   ├─ request.hpp          // Parses JSON requests into objects to interact with core or other services
│   │   ├─ response.hpp         // Creates a JSON response to the client from requests
│   │   └─ service_manager.hpp  // Coordinates logic among request/response, handles RPC or REST endpoints
│   └─ main.cpp                 // The main entry point to run the node service
├─ test/
│   ├─ CMakeLists.txt
│   ├─ unit/
│   │   ├─ test_block.hpp
│   │   ├─ test_pop_consensus.hpp
│   │   ├─ test_chainstate.hpp
│   │   └─ test_document_queue.hpp
│   └─ integration/
│       ├─ test_ipfs_integration.hpp
│       ├─ test_governance_flows.hpp
│       ├─ test_miner_integration.hpp
│       └─ test_network_integration.hpp
└─ scripts/
    ├─ build.sh                 // Helper script for building locally
    ├─ run_local_node.sh        // Example script to start a node locally
    └─ ipfs_bootstrap.sh        // Script to set up or connect to IPFS bootstrap peers
```

**Implementation Detail**: Almost everything is **header-only** (Option A), except for `main.cpp`. This is to simplify distribution, at the cost of larger compile times when changing headers.

**Block Time**: Dynamically selected (Option C), typically shorter than 10 minutes but scaled based on node load.

**Anti-Spam**: **Built into consensus**. No direct deposit or burn is used.

---

## 3. Data Flow & Lifecycle

### 3.1 Document Submission & Validation

1. **Client Submits Document**  
   - Documents (e.g., **compressed** in 2 MB chunks and **signed** via ECDSA) arrive at any node (miner).  
   - Placed into a **```validation_queue/document_queue.hpp```** pipeline.

2. **PII Removal**  
   - A **```pii_stripper.hpp```** module attempts to scrub identifiable data.  
   - Currently a **community/manual** approach, but designed to allow future ML-based methods.

3. **Document Verification**  
   - The chain’s logic (via **```document_verifier.hpp```** or broader consensus) checks authenticity.  
   - **Community moderation** can challenge documents if suspicious or fraudulent.  
   - Metadata is extracted (provider, date, etc.) from the cleaned document.

4. **Pinning & CID Registration**  
   - Once verified, chunks are pinned to IPFS.  
   - A new entry for the IPFS CID(s) is placed in **```cid_registry.hpp```**.

5. **Virtual Write-Ahead Log (WAL)**  
   - “Confirmed” documents and metadata wait in an intermediate WAL.  
   - **Time-based** finalization (once per day) merges them into a new **sqlite** snapshot (Option B).  
   - This snapshot is pinned for 1 week (retention policy).

### 3.2 Mining & Proof-of-Pinning

1. **Challenge Creation**  
   - Each block includes a **```blockChallenge```** (random nonce) referencing random CIDs in the registry.

2. **Node Proof Generation**  
   - Nodes that store pinned data generate Merkle proofs (via **```proof_generator.hpp```**).  
   - Proofs are signed with ECDSA to confirm ownership.

3. **Block Assembly**  
   - A miner aggregates:  
     - **PoP proofs** from itself (and possibly others),  
     - Basic transactions (value transfers),  
     - A block header referencing the prior block hash.

4. **Validation**  
   - Peers receive the block, run checks in **```block_validation.hpp```**:  
     - Verify PoP signatures, chunk references.  
     - Enforce rules (dynamic block time, reward logic, etc.).

5. **Reward**  
   - Blocks that include valid PoP get a **variable** reward (Option C). If no valid PoP is present, **miners forfeit** the reward.  
   - The reward is based on network usage/conditions (potential synergy with the number of pinned CIDs).

---

## 4. Node Architecture

A **single “Full Node” model** is employed:

- **Network Layer**: Manages peers, message passing, block/transaction relay.  
- **Chainstate**: Tracks best chain, block indices, pinned data references.  
- **Validation Queue**: Processes new documents via WAL flow.  
- **Miner**: An optional role in the same code—can be toggled on or off.  
- **IPFS**: Direct library calls within **```ipfs_pinner.hpp```**.

**Resource Limits**: Hard limit of **20 MB** per block. Documents above that are split.

---

## 5. Data Storage

1. **On-Chain**  
   - Each block references PoP proofs, minimal transactions, and metadata.  
   - No large data on-chain—only IPFS CIDs.

2. **Off-Chain (IPFS)**  
   - Document data stored in IPFS, chunked at 2 MB.
   - Snapshots pinned daily; older snapshots kept for 1 week.

3. **Deprecation of Old Snapshots**  
   - After 1 week, old snapshots are no longer guaranteed pinned by the network.  

---

## 6. Governance & Upgrades

1. **Hybrid Moderation**  
   - **```cid_moderation.hpp```** uses a multi-sig approach for urgent takedowns, plus community voting for broader decisions.

2. **Built-in Anti-Spam**  
   - The consensus mechanism includes logic to throttle or reject suspiciously high volumes of new CIDs without normal user activity.

3. **Soft-Fork via Miner Signaling**  
   - Upgrades are introduced with version bits in **```upgrade_manager.hpp```**.  
   - Miners signal acceptance. If majority is reached, new rules activate.

---

## 7. Tokenomics & Reward

- **Variable Rewards**: Tied to network usage and pinned data volume.  
- **No Hard Cap**: Inflation continues indefinitely, aligned with data storage incentives.  
- **Basic Value Transfers**: The chain supports simple token transfers, enabling a small economy around pinning.  
- **Unclaimed Rewards**: If no valid PoP in a block, the reward is **forfeited** (no work = no pay).

---

## 8. Security & Privacy

1. **Manual/Community PII Scrubbing**  
   - Basic checks plus manual oversight. Potential for future ML-based modules.  
   - Document authenticity is verified via ECDSA signatures and **community moderation**.

2. **Basic Cryptography**  
   - **Option A**: Standard signatures and Merkle proofs, no advanced zero-knowledge or enclave solutions at this time.

3. **Attack Mitigations**  
   - **Resource Limits**: 20 MB block size cap.  
   - **Reputation System**: Chronic poor performance or malicious behavior can ban a node from mining.

---

## 9. Future Outlook

- **Incremental Upgrades**: Additional PII scanning or advanced cryptography can be integrated over time via soft-forks.  
- **Integrations with Healthcare Systems**: Potential for verified external APIs if community consensus supports it.  
- **Multi-Stakeholder Governance**: Foundation + coin-weighted voters shape the evolution of the chain.  
- **Adaptive Data Pipelines**: Daily pinned sqlite snapshots can transition to incremental updates if needed.

---

## 10. Summary & Next Steps

- **RxRevoltChain** merges blockchain immutability with IPFS-based distributed storage.  
- **Proof-of-Pinning** ensures nodes truly hold the data, rewarding them with a variable token issuance.  
- **Daily WAL Merges** produce pinned sqlite snapshots, retained for 1 week.  
- **Hybrid Governance** and a single Full Node model provide a unified approach to managing the network.  
- **Open Implementation**: Nearly everything is header-only for simpler integration, with future expansions possible through soft-forks and community input.

---

## Appendix

- **Related Files**:
  - [docs/SPECIFICATIONS.md](docs/SPECIFICATIONS.md): In-depth protocol details (block format, reward formula, etc.).  
  - [docs/DESIGN_DECISIONS.md](docs/DESIGN_DECISIONS.md): Rationale behind major design trade-offs.  

- **Conventions**:
  - Where possible, major changes are introduced via **version-bit soft-fork** (miner signaling).
