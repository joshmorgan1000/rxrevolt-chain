# RxRevoltChain — SPECIFICATIONS

**Version:** 0.1 (Draft)

## 1. Introduction

**RxRevoltChain** is a custom blockchain protocol designed to reward nodes (miners) for **pinning IPFS data** related to healthcare cost transparency. Nodes earn rewards by proving they store relevant documents (bills, EOBs, etc.) and making them reliably available on the IPFS network. This document provides the detailed specifications of how RxRevoltChain operates—covering the chain’s **block and transaction format**, **consensus mechanism**, **tokenomics**, and **governance** rules.

---

## 2. Block & Transaction Specifications

### 2.1 Block Structure

A **RxRevoltChain** block is defined by the following components:

1. **BlockHeader**  
   - **prevBlockHash**: Hash of the previous block’s header.  
   - **blockHeight**: Index (height) of this block in the chain.  
   - **timestamp**: Unix epoch time (seconds) indicating block creation.  
   - **merkleRootTx**: Merkle root of all transactions included in this block.  
   - **merkleRootPoP**: Merkle root (or aggregator) of PoP (Proof-of-Pinning) data.  
   - **version**: Integer representing the protocol version/upgrade bits.  
   - **blockChallenge**: Ephemeral random challenge used in the PoP consensus cycle.

2. **Transactions**  
   - A list of zero or more `Transaction` objects (see **2.2**).
   - Each transaction can perform a token transfer (sender -> recipient) and/or reference pinned IPFS data by storing relevant CIDs.

3. **PopProofs**  
   - A list of zero or more PoP (Proof-of-Pinning) entries, each containing:
     - Node public key
     - Claimed pinned CIDs
     - Merkle root of pinned chunk proofs
     - Signatures verifying the node holds the data

4. **Block Validation**  
   - The node checks `blockHeader` integrity (timestamp, previous hash, etc.).
   - Transactions are verified (signatures, no double-spend, etc.).
   - PoP entries are validated to ensure the node truly pinned the IPFS data (see **3.2**).
   - If all checks pass, the block can be accepted onto the chain.

### 2.2 Transaction Format

Each transaction references a **sender** (fromAddress), a **recipient** (toAddress), an optional **value** (token amount), and an optional **list of CIDs** pointing to pinned IPFS data:

- **fromAddress**: A string representing the sending address (ECDSA/Ed25519-based).
- **toAddress**: A string for the receiving address (can be empty if it’s a “burn” or a chain-specific operation).
- **value**: An unsigned 64-bit integer representing the tokens to transfer.
- **cids**: A list of IPFS CIDs referencing pinned documents (could be zero-length if only transferring tokens).
- **signature**: (Optional) cryptographic signature over the transaction hash.

Transactions are **hashed** (SHA-256 by default) for reference in the block’s Merkle tree. A minimal or zero-value transaction referencing CIDs is valid if it includes legitimate pinned data references.

---

## 3. Consensus — Proof-of-Pinning

### 3.1 Overview

RxRevoltChain uses a novel **Proof-of-Pinning (PoP)** consensus mechanism where miners must:

1. Participate in a **challenge-response** to prove they store a subset of IPFS data.  
2. Submit valid PoP proofs in each block.  
3. Earn block rewards if the chain accepts these proofs.

The consensus is integrated into block production:
- **blockChallenge**: A random nonce (or VRF-based) included in the `BlockHeader`.
- **popProofs**: Data structures each miner includes to prove they pinned specific CIDs.

### 3.2 PoP Proofs

Each **PopProof** includes:
- **nodePublicKey**: The public key identifying the node.
- **cids**: IPFS CIDs that the node claims to pin.
- **merkleRootChunks**: A Merkle root referencing the pinned file chunks.
- **signature**: A signature combining the node’s private key, the `blockChallenge`, and the pinned data references.

Miners generate these proofs by:
1. Selecting subsets of pinned data (based on random selection or VRF).
2. Computing chunk-level Merkle proofs to show actual storage.
3. Signing the ephemeral challenge to prevent replay.

A block is only valid if **at least one** PoP proof from the block producer (and possibly other endorsing nodes) is correct. Without valid PoP proofs, the reward is **forfeited**.

### 3.3 VRF / Challenge Randomness

To ensure fairness in data selection, the chain uses:
- A **VRF-based** or **block-hash** based approach to randomize the chosen CIDs each block.
- The resulting ephemeral challenge is placed in the `blockChallenge`.

Miners must respond accordingly or risk producing an invalid block.

---

## 4. IPFS Integration

### 4.1 Pinning

- **Nodes** run an IPFS daemon or link to an IPFS library to pin documents (bills, EOBs, etc.).
- A specialized component `ipfs_pinner.hpp` handles:
  - **pin()**: Request to store a file or chunk in the local IPFS repository.
  - **unpin()**: Free up storage (if consensus rules allow).
  - **verifyPin()**: Check that a given CID is truly available locally.

### 4.2 Merkle Proofs

- Files are chunked and hashed into a Merkle tree (managed by IPFS).
- `merkle_proof.hpp` includes logic to confirm chunks are present and correct.
- PoP proofs embed the **merkleRootChunks** to tie an ephemeral challenge to the actual pinned data.

### 4.3 Daily WAL Merge

- **WAL merges**: The chain can pin an updated sqlite snapshot daily, containing aggregated healthcare data.
- Old snapshots are eventually pruned or unpinned after a retention period (e.g., 1 week).

---

## 5. Tokenomics & Rewards

### 5.1 Token Creation

- **Block Reward**: Each block mints a variable or fixed amount of tokens, credited to the miner’s address if a valid PoP proof is provided.
- No valid PoP? => Reward is **forfeited** for that block.

### 5.2 No Hard Cap

- The chain aims to incentivize continuous pinning of healthcare data.  
- Inflation is indefinite, focusing on utility, not store-of-value scarcity.

### 5.3 Value Transfers

- Transactions can transfer tokens between addresses, referencing pinned data if desired.
- Minimal fees or zero fees may apply, subject to **anti_spam** or governance decisions.

---

## 6. Governance & Moderation

### 6.1 Multi-Sig + Community Voting

- **cid_moderation.hpp**: Modules enabling multi-sig or coin-weighted voting to remove malicious or unwanted CIDs from the active registry.
- **upgrade_manager.hpp**: Soft-fork approach with miner signaling for protocol changes.
- **anti_spam.hpp**: Additional measures to deter spam transactions and CIDs.

### 6.2 Reputation System

- Nodes accumulate a reputation score based on successful PoP proofs and reliability.
- Malicious or non-performing nodes lose reputation; eventually, they can be banned from mining.

---

## 7. Security & Privacy

- **PII Scrubbing**: Documents must be sanitized (either client-side or by the network via `pii_stripper.hpp`) to remove personal data.  
- **Community Audits**: The chain’s validation queue or `document_verifier.hpp` can flag suspicious or incomplete docs.
- **Sybil Resistance**: The chain relies on PoP + real data storage. Attackers must store large volumes of real data to mine effectively.

---

## 8. Implementation Summary

### 8.1 Header-Only Modules

- **core/block.hpp**, **core/transaction.hpp**: Define fundamental blockchain data structures.  
- **consensus/pop_consensus.hpp**: Implements PoP challenge-response details.  
- **ipfs_integration/ipfs_pinner.hpp**: Interacts with IPFS pin/unpin.  
- **miner/proof_generator.hpp**: Creates Merkle proofs for pinned data.  
- **governance/cid_moderation.hpp**: Multi-sig + community approach for removing malicious data.  
- **util/logger.hpp**, **util/hashing.hpp**: Logging and hashing utilities.  
- **util/thread_pool.hpp**: Thread pool for asynchronous tasks (e.g., PoP checks, IPFS ops).

### 8.2 Node Startup (Optional Outline)

1. **Read Config**: Using `config/node_config.hpp`, parse node settings.  
2. **Initialize**: 
   - Logger (`logger::setLogLevel`),
   - IPFS pinner,
   - Miner (if enabled).  
3. **Sync**: Connect to P2P peers, fetch the latest chain.  
4. **Run**: Continuously produce blocks if mining, or just validate incoming blocks if not.  
5. **Shut Down**: Gracefully close IPFS connections, finalize WAL merges, etc.

---

## 9. Future Extensions

- **Zero-Knowledge Proofs**: Possibly prove pinned data ownership without exposing file chunks.  
- **Advanced Tokenomics**: Adjust reward distribution to also benefit nodes that verify data (not just produce blocks).  
- **Healthcare Data Standards**: Integrate with FHIR or other EHR systems to automate cost transparency.  

---

## 10. Conclusion

**RxRevoltChain** provides a specialized blockchain that rewards IPFS pinning of healthcare cost documents. By combining minimal transaction structures, PoP-based consensus, and a flexible governance model, it aims to enhance **data availability**, **patient privacy**, and **community-driven** healthcare transparency.

Further technical details (such as exact PoP cryptographic flows, node identity management, or daily WAL snapshot rules) can be found in:

- [`docs/ARCHITECTURE.md`](ARCHITECTURE.md): System-level design and rationale  
- [`docs/DESIGN_DECISIONS.md`](DESIGN_DECISIONS.md): Key trade-offs and justifications  
