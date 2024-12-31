# RxRevoltChain Whitepaper

**Version:** 1.0  
**Date:** December, 2024

---
## Table of Contents

1. [Introduction](#1-introduction)  
2. [Background & Motivation](#2-background--motivation)  
3. [System Overview](#3-system-overview)  
    - [High-Level Goals](#31-high-level-goals)  
    - [Key Components](#32-key-components)  
4. [Data Flow & Lifecycle](#4-data-flow--lifecycle)  
    - [Daily Merges & SQLite Snapshots](#41-daily-merges--sqlite-snapshots)  
    - [Document Submission & Validation](#42-document-submission--validation)  
    - [Proof-of-Pinning (PoP)](#43-proof-of-pinning-pop)  
5. [Architecture & Consensus](#5-architecture--consensus)  
    - [Consensus Without a Traditional Chain](#51-consensus-without-a-traditional-chain)  
    - [Reward Logic & Node Reputation](#52-reward-logic--node-reputation)  
    - [Security & Privacy Measures](#53-security--privacy-measures)  
6. [Governance & Upgrades](#6-governance--upgrades)  
7. [Non-Node Access & Public Usage](#7-non-node-access--public-usage)  
8. [Implementation & Roadmap](#8-implementation--roadmap)  
9. [Class Structure](#9-class-structure)
10. [References](#10-references)

---

## 1. Introduction

**RxRevoltChain** is a **non-profit** platform for hosting and verifying healthcare cost data (bills, EOBs, etc.) in a **single pinned SQLite database**, with a mechanism to **reward node operators** for providing reliable uptime and data availability. Instead of storing data in a long chain of cryptographically linked blocks, RxRevoltChain produces **daily merges** that result in a canonical `.sqlite` snapshot pinned via IPFS. This approach simplifies data retrieval, lowers overhead, and facilitates end-user privacy.

---

## 2. Background & Motivation

Healthcare cost transparency is a **global concern**—patients often struggle to access or compare costs for procedures, leading to opaque and inconsistent pricing. Traditional blockchains can be too **heavy** for hosting large records, and also place the data in a purely “on-chain” context. Meanwhile, IPFS solves distributed file storage but **lacks** built-in incentives for nodes to continually host specific data.

**RxRevoltChain** addresses this gap by:

- Ensuring **availability** through a **Proof-of-Pinning (PoP)** mechanism,  
- Maintaining a single pinned `.sqlite` database for **day-to-day queries** without forcing each node to store a chain of blocks,  
- Letting users **remove** or redact their own data if they wish,  
- **Rewarding** stable hosting in a **non-profit** manner, emphasizing public good over speculation.

---

## 3. System Overview

### 3.1 High-Level Goals

1. **Promote Data Availability**  
   Use **Proof-of-Pinning** to incentivize consistent hosting of the pinned healthcare database. Nodes providing stable uptime earn continuous rewards.

2. **Ensure Privacy & Removal Rights**  
   The system emphasizes **PII scrubbing** for new documents and provides a user-signed “remove data” mechanism so submitters can withdraw their content.

3. **Lightweight Overhead**  
   By pinning **one** database snapshot daily, new nodes avoid replaying extensive block histories. This fosters easier onboarding and simpler data queries.

4. **Non-Profit & Transparency**  
   Token issuance is inflation-based, with limited speculation. The primary objective is **healthcare cost transparency** and user empowerment.

### 3.2 Key Components

1. **Single `.sqlite` DB**: The core data store containing validated healthcare records.  
2. **Daily Merge Mechanism**: Consolidates pending updates or removals into a fresh snapshot.  
3. **Proof-of-Pinning**: Nodes must demonstrate they hold the full `.sqlite` to receive rewards.  
4. **Governance**: A hybrid approach (multi-sig + community votes) for data moderation and upgrades.  
5. **IPFS Pinner**: Manages pin/unpin calls and ensures the final DB snapshot is available on IPFS.

---

## 4. Data Flow & Lifecycle

### 4.1 Daily Merges & SQLite Snapshots

- **Virtual Write-Ahead Log**: As new documents (bills, EOBs) arrive, they remain in a “pending” state.  
- **Daily Merge**: At a set interval (e.g. once per day), the system finalizes these records and compresses them into the `.sqlite` file as **blobs**.  
- **Pinned Snapshot**: The resulting updated DB is pinned on IPFS. Old snapshots remain pinned for a retention period (e.g., 1 week).

### 4.2 Document Submission & Validation

1. **Submission**: Users compress and sign their healthcare documents (e.g., via ECDSA) and send them to a hosting node.  
2. **PII Removal**: The node runs a “stripper” or manual check to remove personally identifiable data.  
3. **Verification**: The data is reviewed for authenticity (signatures, community checks).  
4. **WAL Storage**: Accepted documents go into the “virtual WAL” queue, awaiting the daily merge.

### 4.3 Proof-of-Pinning (PoP)

- **Challenge**: Nodes claiming rewards must answer ephemeral random chunk checks on the pinned DB.  
- **Response**: If the node proves possession, it’s marked as a “good host” for that round/day.  
- **Reward Distribution**: Verified nodes share newly minted tokens, scaled by their reliability or uptime factor.

---

## 5. Architecture & Consensus

### 5.1 Consensus Without a Traditional Chain

**RxRevoltChain** does not maintain a chain of cryptographic blocks. Instead, the **latest** pinned `.sqlite` snapshot is the “current state.” Each node:

1. **Retrieves** the daily snapshot (if new).  
2. **Checks** that it includes the correct merges.  
3. **Pins** it, generating PoP if they wish to earn rewards.  
4. **Accepts** or **rejects** any snapshot not matching the majority network view.

### 5.2 Reward Logic & Node Reputation

1. **Inflationary Token**: Each day, the system mints new tokens.  
2. **PoP Check**: Only nodes passing the PoP get a portion of these tokens.  
3. **Uptime Factor**: If a node consistently appears day after day, it may earn higher or stable rewards.  
4. **Malicious/Offline Behavior**: Repeated failures or malicious forging of data can reduce node reputation or ban them from receiving future rewards.

### 5.3 Security & Privacy Measures

- **Hard Document Size Cap**: ~20 MB limit, chunking larger documents automatically.  
- **Encryption or PII Stripping**: The system encourages user-side encryption and local PII scrubbing, plus a second pass by the miner node.  
- **Governance on Malicious Data**: If harmful or fraudulent content is discovered, community or multi-sig moderation can remove it from the next snapshot.

---

## 6. Governance & Upgrades

1. **CID Moderation**  
   - Malicious or spam data can be flagged and removed by a multi-sig or majority community vote.  
2. **Upgrade Manager**  
   - Soft-fork style changes can be introduced via version bits. Nodes signal acceptance; upon reaching a threshold, new rules activate.  
3. **Anti-Spam**  
   - Built-in consensus checks detect unusual surges in data or suspicious repeated submissions from the same user.

---

## 7. Non-Node Access & Public Usage

- **Public Endpoints**: Certain hosting nodes can expose a simple HTTP/REST interface allowing any user to query aggregated cost data.  
- **Snapshot Download**: Anyone can fetch the pinned `.sqlite` from IPFS or node mirrors for offline analysis.  
- **Summaries & Metrics**: Because it’s just a standard SQLite DB, aggregator nodes can build dashboards or explorers with minimal overhead.

---

## 8. Implementation & Roadmap

### PHASE 1: Core Database & Daily Merge Foundations

#### 1.1 Basic SQLite + Daily Merge

1. **Setup the SQLite File**
   - Create a minimal `.sqlite` structure with tables for:
     - **Documents** (stored as compressed blobs),  
     - **Metadata** (basic references, timestamps, submitter IDs).  
   - Define a rudimentary “virtual WAL” for unmerged records.

2. **Daily Merge Mechanism**
   - Implement a scheduler that runs **once per day** (or at a configurable interval):
     1. Reads any pending document queue,
     2. Validates each item (signature check, basic format check),
     3. Inserts them as **compressed blobs** into the `.sqlite` file,
     4. Commits the changes, generating a new snapshot.

3. **Initial Pinned Snapshot**
   - Integrate a call to **IPFS** (via a direct library or daemon HTTP API) to pin this newly finalized `.sqlite`.  
   - Provide a simple method to retrieve the pinned CID for that snapshot.

**Deliverables**  
- A working `.sqlite` with a daily “merge and pin” script.  
- Basic document queue ingestion.  
- A pinned snapshot on IPFS that any node can fetch.

---

### PHASE 2: Proof-of-Pinning & Inflationary Rewards

#### 2.1 PoP Challenge & Response

1. **Chunk-Based or Ephemeral Proof**
   - Implement a **Proof-of-Pinning** scheme:
     1. Randomly pick offsets/blocks in the pinned `.sqlite`,
     2. Request each node to provide the corresponding data segments,
     3. Verify it matches the known file content or hash.

2. **Uptime / Streak Tracking**
   - Maintain a per-node “uptime streak” or “hosting streak” value:
     - Increment if the node passes the daily PoP challenge,
     - Decrement (or partially regress) if the node is offline or fails proof.

3. **Reward Distribution**
   - Introduce an **inflationary token** model:
     - Each day, a certain number of tokens are minted,
     - Nodes who pass PoP share those tokens in proportion to their “uptime streak” multiplier.

**Deliverables**  
- A functioning PoP system that challenges each node daily.  
- A token contract or issuance logic (even if minimal) awarding tokens to successful pinning nodes.  
- Simple “uptime streak” counters and basic logs showing node rewards.

---

### PHASE 3: Privacy, Data Removal & Governance

#### 3.1 PII Stripping & Enhanced Document Checks

1. **PII Stripper Integration**
   - Expand on-phase ingestion so that incoming documents:
     1. Are scanned for standard PII patterns (names, phone numbers, etc.),
     2. Flag suspicious patterns for manual or community-based oversight,
     3. Remove or redact found PII from the final blob stored in `.sqlite`.

2. **Community or Manual Oversight**
   - Provide an interface (CLI or minimal UI) allowing node operators (or a selected moderation group) to see flagged documents and approve or reject them before the daily merge.

#### 3.2 User-Initiated Removal

1. **Removal Transaction**
   - Let the original submitter sign a “remove document” request:
     1. This request is queued similarly to new documents,
     2. During the daily merge, the system marks that record “inactive” or deletes the actual blob from the next `.sqlite` snapshot.

2. **Daily Merge Integration**
   - Ensure that if a removal transaction is verified, the new pinned DB no longer contains the targeted record.

#### 3.3 Hybrid Governance

1. **Multi-Sig & Community Voting**
   - Implement a governance module that can:
     1. Trigger urgent takedowns for malicious or illegal data,
     2. Propose major changes (e.g., adjusting daily intervals or token inflation).
2. **On-Chain or Off-Chain Signaling**
   - Potentially store governance proposals in the DB, with each node operator’s “stake” or “signature” recorded to accept or reject.

**Deliverables**  
- A robust privacy pipeline ensuring PII gets removed or flagged.  
- A user removal mechanism, tested by an example flow (submit, confirm remove, next daily snapshot excludes it).  
- Governance logic that can forcibly remove malicious content if a threshold is reached.

---

### PHASE 4: Public Endpoints & Non-Node Access

#### 4.1 Public Query Nodes

1. **Read-Only API**
   - Certain volunteer or official nodes can host a minimal HTTP server that:
     1. Serves metric endpoints (total bills, aggregated costs, etc.),
     2. Allows queries for specific records or ranges of data,
     3. Returns data from the pinned `.sqlite` in a user-friendly format (JSON, CSV, etc.).

2. **Summaries & Analytics**
   - Build optional aggregator tables or “materialized views” in `.sqlite` that keep track of high-level metrics (e.g., average cost by procedure code, total claims per provider, etc.).

#### 4.2 Offline / Partial Exports

1. **Snapshot Download**
   - Make it straightforward for any user to retrieve the entire pinned `.sqlite` from IPFS,
     open it locally, and run standard SQL queries offline.

2. **Selective Exports**
   - Provide an optional tool or CLI to extract subsets of data. For instance, a user might want only “records from 2022 for hospital X.”

**Deliverables**  
- At least one working public node with a REST or gRPC interface, enabling simpler data queries.  
- Script or instructions for non-node users to fetch pinned snapshots from IPFS.

---

### PHASE 5: Reputation Model & Advanced Security

#### 5.1 Node Reputation / Weighted Rewards

1. **Longer-Term Streak**  
   - Possibly refine the reward schedule: the longer a node consistently passes PoP, the more stable or higher portion of the daily inflation they receive (could be a sigmoid or other function).
2. **Malicious Behavior Penalties**
   - Auto-detect if a node frequently claims hosting but fails challenges,
   - Possibly ban them or set their reward share to zero for a cooldown period.

#### 5.2 More Robust PoP

1. **Chunk/Offset Randomization**  
   - Increase complexity of the random checks (covering more offsets, verifying multiple times a day if needed).
2. **Encryption or Zero-Knowledge** (Optional Future Path)
   - Investigate partial or fully-encrypted data storage for more sensitive healthcare info.
   - Evaluate feasibility and overhead of zero-knowledge or advanced proofs.

**Deliverables**  
- Node reputation logic integrated with the daily merges,
- Possibly more frequent PoP checks if certain suspicious activity is detected.

---

### PHASE 6: Scaling, Integrations & Long-Term Maintenance

#### 6.1 Large Healthcare Systems

1. **External API Connectors**
   - Tools or modules to interface with EHR (Electronic Health Record) systems so they can auto-submit cost data,
   - Possibly integrate with insurance providers for direct feed of claims data.

2. **Sharding / Partitioning** (If the main DB grows huge)
   - Potentially split the pinned data into multiple region-based or category-based `.sqlite` files, if necessary.

#### 6.2 Maintenance & Upgrades

1. **Upgrade Manager Enhancements**
   - Solidify the soft-fork approach: version bits or multi-sig signals, automatically recognized by nodes that adopt the new rules.
2. **Community Governance Maturation**
   - Expand the multi-sig / stake-based or NGO-based system to handle complex policy decisions (e.g., changes to daily intervals, pinning fees, retention periods).

---

## 9. Class Structure

### src/pinner/pinner_node.hpp
Defines the primary logic for nodes (pinners) that host the pinned SQLite database. Key tasks include:
- Initializing or shutting down the node environment.  
- Coordinating with [`src/pinner/daily_scheduler.hpp`](#srcpinnerdailyschedulerhpp) to trigger merges or PoP checks at the correct time.  
- Maintaining an event loop for receiving messages (new documents, removal requests, etc.) from the network layer or a local interface.

---

### src/pinner/daily_scheduler.hpp
Handles the scheduling of the daily (or configurable) snapshot merges. Core functions:
- Tracking the time/frequency for the next merge cycle.  
- Invoking methods in [`src/core/daily_snapshot.hpp`](#srccoredailysnapshothpp) to finalize pending documents.  
- Kicking off proof-of-pinning routines (via [`src/consensus/pop_consensus.hpp`](#srcconsensuspop_consensushpp)) after a successful merge.

---

### src/core/daily_snapshot.hpp
Implements the process of taking all pending records from [`src/core/document_queue.hpp`](#srccoredocument_queuehpp) and merging them into the single `.sqlite` file:
- Integrates or removes documents based on user submissions or removal requests.  
- May compress blobs for storage.  
- Invokes IPFS pinning (using [`src/ipfs_integration/ipfs_pinner.hpp`](#srcipfs_integrationipfs_pinnerhpp)) once the updated snapshot is complete.

---

### src/core/pinned_state.hpp
Keeps track of:
- The current pinned `.sqlite` snapshot (e.g., which CID or local path is recognized).  
- Any ephemeral write-ahead data that hasn’t yet been merged.

This file’s declarations help each node know “which daily snapshot is official right now” and “what new data is still pending.”

---

### src/core/transaction.hpp
Defines minimal structures for “transaction-like” actions, such as:
- Document submission (with potential ECDSA signature).  
- Document removal requests from the original submitter.

Inline functions can verify signatures or parse message fields.

---

### src/core/document_queue.hpp
Maintains a queue (or buffer) of newly submitted records (bills/EOBs) and removal requests until the next merge:
- Provides methods for adding new items and retrieving them in bulk when [`src/core/daily_snapshot.hpp`](#srccoredailysnapshothpp) merges the data.  
- Ensures concurrency safety if multiple threads or external calls are appending data.

---

### src/core/privacy_manager.hpp
Implements PII redaction or other privacy logic:
- Scans documents for personally identifiable info before final storage in the pinned database.  
- Potentially flags suspicious content for further moderation if it detects something that cannot be automatically removed.

---

### src/consensus/pop_consensus.hpp
Coordinates proof-of-pinning challenges each day:
- Issues ephemeral requests for random offsets or chunk-based proof.  
- Collects node responses, verifying if they genuinely hold the pinned `.sqlite` file.  
- Works closely with [`src/pinner/proof_generator.hpp`](#srcpinnerproof_generatorhpp) to generate or validate these proofs.

---

### src/consensus/snapshot_validation.hpp
Verifies the correctness of each newly created snapshot:
- Ensures all queued documents are properly inserted, and user removal requests are respected.  
- May check structural or hashing consistency of the updated `.sqlite`.  
- Flags any mismatch that indicates an incomplete or manipulated merge.

---

### src/consensus/reward_scheduler.hpp
Distributes inflation-based rewards after proof-of-pinning is complete:
- Keeps track of which pinners passed the challenge.  
- Calculates the day’s minted tokens and splits them among successful nodes, possibly factoring in uptime or “streak” multipliers.

---

### src/ipfs_integration/ipfs_pinner.hpp
Handles the actual pin/unpin actions on IPFS:
- Provides methods to pin the new `.sqlite` snapshot and verify it by CID or local file checks.  
- May retrieve older snapshots on demand if needed for references or rollback.

---

### src/ipfs_integration/merkle_proof.hpp
Implements chunk-based or merkle-based proofs to confirm partial file possession:
- If the `.sqlite` is large, random chunk checks can be validated by merkle branches.  
- Used by [`src/consensus/pop_consensus.hpp`](#srcconsensuspop_consensushpp) or [`src/pinner/proof_generator.hpp`](#srcpinnerproof_generatorhpp) to provide more efficient PoP.

---

### src/network/p2p_node.hpp
Manages peer-to-peer communications for:
- Announcements of newly pinned snapshots (daily merges).  
- PoP challenge/response distribution among nodes.  
- Possibly governance signals or moderation notices.

---

### src/network/protocol_messages.hpp
Defines the data structures used when sending or receiving:
- Snapshot announcements (containing a new `.sqlite` CID).  
- Proof-of-pinning requests/responses.  
- Governance or upgrade signals.

---

### src/pinner/proof_generator.hpp
Generates ephemeral checks or chunk requests used in proof-of-pinning:
- Chooses random offsets within the new `.sqlite` to see if a node truly hosts that data.  
- Assists [`src/consensus/pop_consensus.hpp`](#srcconsensuspop_consensushpp) by returning the exact chunk or merkle path that must be validated.

---

### src/governance/content_moderation.hpp
Implements multi-sig or majority-based moderation for urgent takedowns:
- Tracks open proposals to remove content from the next snapshot.  
- Ensures removal requests gather enough signatures or votes before daily merge processing.

---

### src/governance/upgrade_manager.hpp
Handles changes to system parameters or versioned rules in a soft-fork style:
- Nodes can signal readiness for an upgrade.  
- Once a threshold is reached, new rules (like different merge intervals or reward logic) can apply.

---

### src/service/request.hpp / response.hpp
Data structures for external client communication, such as:
- A JSON-based “SubmitDocRequest” or “RemoveDocRequest.”  
- A “GenericResponse” indicating success, error, or additional data.

---

### src/service/service_manager.hpp
Acts as the inbound call controller:
- Receives requests from local or remote clients.  
- Delegates them to the relevant modules: “document_queue” for new items, “content_moderation” for flagged items, etc.  
- May provide an abstraction over network vs. local calls.

---

### src/util/logger.hpp
Provides macros or inline functions to log messages at various levels (info, debug, error):
- Can be toggled or configured to suppress or expand logs.  
- Ensures all parts of the node produce consistent logging output.

---

### src/util/hashing.hpp
Implements or wraps standard cryptographic hashes (e.g., SHA-256) used for:
- Verifying chunk data in merkle proofs,  
- Possibly hashing new `.sqlite` data for quick checks.

---

### src/util/thread_pool.hpp
Defines concurrency structures (like a static inline thread-pool) for:
- Handling parallel tasks during daily merges,  
- Distributing chunk verifications or PoP across threads.

---

### src/util/config_parser.hpp
Reads in local node or system-level config:
- E.g., from `rxrevolt_node.conf` or environment variables.  
- Supplies values (merge interval, IPFS connection info, etc.) to the rest of the code.

---

### src/main.cpp
Launches the pinner node:
- Loads config from [`src/util/config_parser.hpp`](#srcutilconfig_parserhpp).  
- Instantiates the pinner node described in [`src/pinner/pinner_node.hpp`](#srcpinnerpinner_nodehpp).  
- Kicks off scheduling logic in [`src/pinner/daily_scheduler.hpp`](#srcpinnerdaily_schedulerhpp), plus network listeners.  
- Optionally provides a command-line or minimal UI for local user commands.

---

## 10. References

1. [IPFS Documentation](https://docs.ipfs.tech/)  
2. [SQLite Official Documentation](https://www.sqlite.org/docs.html)  
3. [Filecoin & IPFS Relationship](https://filecoin.io) (for conceptual parallels, though RxRevoltChain differs in approach)  
