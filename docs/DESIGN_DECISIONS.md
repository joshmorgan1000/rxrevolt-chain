# DESIGN_DECISIONS.md

This document enumerates the **open decisions** for RxRevoltChain, providing **pros** and **cons** for each approach. By clarifying these points, we can refine our design and eventually update the `ARCHITECTURE.md` with the chosen paths.

---

## 1. Governance Approach
**Decision**: How to handle malicious or irrelevant data, upgrades, and overall chain governance.

- **Option A: Foundation-Led Multi-Sig**  
  - **Pros**: Fast response to spam or dangerous content; centralized coordination can quickly adapt.  
  - **Cons**: Can undermine decentralization; trust in foundation is required.

- **Option B: Coin-Weighted Voting**  
  - **Pros**: Leverages economic stake; more decentralized than a foundation approach.  
  - **Cons**: Can favor whales; potential for plutocratic influence or bribery.

- **Option C: Hybrid (Multi-Sig + Community Vote)**  
  - **Pros**: Balances quick action by a core group with stakeholder input for major changes.  
  - **Cons**: Complexity in deciding thresholds; can slow urgent decision-making.

We have opted to go with **Option C.**

---

## 2. Block Time Target
**Decision**: How frequently new blocks are mined (5 minutes, 10 minutes, or another interval).

- **Option A: \~5 Minutes**  
  - **Pros**: Faster confirmations; quicker updates to chainstate.  
  - **Cons**: More orphan blocks; higher overhead for nodes.

- **Option B: \~10 Minutes**  
  - **Pros**: Fewer forks; less stress on the network.  
  - **Cons**: Slower confirmations can frustrate users.

- **Option C: Another Interval (e.g., 2 or 15 minutes)**  
  - **Pros**: Could be tuned to match expected data load or user usage.  
  - **Cons**: Less “standard” than 5 or 10 minutes; might need more simulation to find sweet spot.

We have decided to go with **Option C.** The time interval will depend on the load of the network and the number of worker nodes, opting for shorter block time, but not allowing the system to be overloaded.

---

## 3. Resource Allocation for Large File Pinning
**Decision**: Maximum chunk size, how to distribute large files, etc.

- **Option A: Small Chunks (e.g., 256 KB)**  
  - **Pros**: Easier to handle partial re-pinning; finer granularity in challenges.  
  - **Cons**: Higher overhead in managing more chunks; potential performance slowdown in IPFS lookups.

- **Option B: Larger Chunks (e.g., 4 MB)**  
  - **Pros**: Fewer chunk references to manage; fewer IPFS calls.  
  - **Cons**: Potentially big data transfers on challenge checks; slower partial re-pinning.

- **Option C: Dynamically Determined Chunk Size**  
  - **Pros**: Adaptable to network conditions; allows flexibility.  
  - **Cons**: More complex to implement; more potential for edge-case bugs.

We have decided to go with **Option B** with a size of 2 MB.

---

## 4. Additional Transaction Types Beyond PoP Proofs
**Decision**: Whether to allow typical token transfers or smart contracts.

- **Option A: PoP-Only**  
  - **Pros**: Keeps chain minimal; focus on data availability.  
  - **Cons**: No easy way to do “transfers” or more advanced on-chain logic.

- **Option B: Basic Value Transfers**  
  - **Pros**: Reward tokens become tradable in simple peer-to-peer fashion.  
  - **Cons**: Increases complexity; must handle wallet logic, addresses, fees, etc.

- **Option C: Full Smart Contracts**  
  - **Pros**: Extensible platform for additional healthcare dApps.  
  - **Cons**: Significant complexity, heavier resource requirements.

We have decided to go with **Option B.**

---

## 5. HIPAA/PHI Compliance Strategies
**Decision**: How thorough to be in removing personal data from documents.

- **Option A: Naive Regex + Client-Side Enforcement**  
  - **Pros**: Simpler to implement; lower overhead for miners.  
  - **Cons**: Risk of missed PII; doesn’t guarantee compliance.

- **Option B: Advanced PII/PHI Detection (Machine Learning/OCR)**  
  - **Pros**: More robust privacy protection.  
  - **Cons**: High computational cost; increased development complexity.

- **Option C: Hybrid Approach**  
  - **Pros**: Flexible; can adopt increasingly sophisticated checks over time.  
  - **Cons**: Requires a phased rollout; more moving parts to maintain.

We have decided to go with **Option C.**

---

## 6. Implementation Detail: Header-Only vs. `.cpp` Files
**Decision**: Whether each module remains header-only or to separate implementation.

- **Option A: All Header-Only**  
  - **Pros**: Easier to include and distribute; no linking issues.  
  - **Cons**: Larger compile times; changes in headers require recompilation of many files.

- **Option B: Separate `.cpp` Implementations**  
  - **Pros**: Faster incremental builds; can encapsulate internal logic.  
  - **Cons**: Slightly more complex build process; need to manage private vs. public APIs.

We have decided to go with **Option A.** Only the main.cpp file will not be a header.

---

## 7. IPFS Integration Approach
**Decision**: Connect to IPFS via direct library calls, HTTP-based API, or managing a separate daemon.

- **Option A: Direct Library Calls (e.g. libipfs)**  
  - **Pros**: Potentially tighter integration; direct control over IPFS operations.  
  - **Cons**: Must compile/link a specialized library; fewer examples, possible platform issues.

- **Option B: Local/Remote HTTP API**  
  - **Pros**: Standard approach; easy to test with a separate IPFS daemon.  
  - **Cons**: Higher latency; need HTTP client code.

- **Option C: Separate Docker Container or “IPFS Companion Process”**  
  - **Pros**: Clear process isolation; easy to upgrade IPFS independently.  
  - **Cons**: More complexity in deployment.

We have decided to go with **Option A.**

---

## 8. Chunking Method for Large Documents
**Decision**: Precisely how large documents are split and reassembled.

- **Option A: Rely on IPFS Default Chunking**  
  - **Pros**: Leverages IPFS’s built-in chunking/merkle structure.  
  - **Cons**: Less direct control; reliant on IPFS updates or changes.

- **Option B: Custom Chunking Pre-Upload**  
  - **Pros**: Full control; possibly optimized for healthcare docs.  
  - **Cons**: More overhead, need to maintain custom chunk logic.

We have decided to go with **Option A.**

---

## 9. Document Authenticity Consensus Checks
**Decision**: Methods to decide if a document is “real” or meets minimal criteria.

- **Option A: Simple Format/Signature Check**  
  - **Pros**: Straightforward to implement; minimal overhead.  
  - **Cons**: May pass fraudulent documents that meet the format but are otherwise invalid.

- **Option B: External Oracles or Healthcare APIs**  
  - **Pros**: Can actually verify claims (e.g., provider IDs, procedure codes).  
  - **Cons**: Complexity; reliance on external data sources.

- **Option C: Community Moderation**  
  - **Pros**: Collective intelligence; flexible.  
  - **Cons**: Risk of human error or manipulation; slower resolution times.

We have decided to go with **Option C.**

---

## 10. Frequency for WAL Finalization & Database Pinning
**Decision**: How often the “virtual write-ahead log” merges into a pinned sqlite.

- **Option A: Every X Blocks**  
  - **Pros**: Predictable cadence; simple implementation.  
  - **Cons**: Might be inefficient if X is too high or too low.

- **Option B: Time-Based (e.g. once per day)**  
  - **Pros**: More aligned with real-world usage or reporting cycles.  
  - **Cons**: If the chain is idle, we still do merges; or we might need complex scheduling.

- **Option C: Adaptive**  
  - **Pros**: Adjust merges based on document volume or network load.  
  - **Cons**: More logic needed, potential unpredictability.

We have decided to go with **Option B** with once per day.

---

## 11. Node Architecture: Full vs. Light vs. Dedicated Pinning Nodes
**Decision**: Whether different node “types” exist, or if all nodes behave similarly with optional roles.

- **Option A: Single “Full Node” Model**  
  - **Pros**: Simpler design; all nodes have the same capabilities.  
  - **Cons**: Higher resource demand on every node.

- **Option B: Light Nodes & Pinning Nodes**  
  - **Pros**: Users can run light nodes without storing big data; dedicated pinning nodes handle PoP.  
  - **Cons**: More complexity in protocols; potential synergy needed for data requests.

- **Option C: Multi-tiered (Full, Light, Archival, Pinning)**  
  - **Pros**: Tailored roles for different stakeholders.  
  - **Cons**: Might fragment the network, requiring robust role-specific logic.

We have decided to go with **Option A.**

---

## 12. Old Snapshot Retention Policy
**Decision**: How to handle older sqlite database snapshots pinned on IPFS.

- **Option A: Keep Last N Snapshots**  
  - **Pros**: Manageable storage overhead; easy to configure.  
  - **Cons**: Might lose older historical data if N is too small.

- **Option B: Keep All Snapshots**  
  - **Pros**: Maximum historical integrity; advanced analytics possible.  
  - **Cons**: Potentially large storage overhead over time.

- **Option C: Time-Based Retention (e.g., 1 year)**  
  - **Pros**: Balances historical value vs. bloat.  
  - **Cons**: Might discard useful older data if not carefully planned.

We have decided to go with **Option C**, with a retention time of 1 week.

---

## 13. Anti-Spam Measures (Deposit, Fee, Burn)
**Decision**: Mechanisms to prevent malicious flooding of CIDs or documents.

- **Option A: Minimal Fee per Document**  
  - **Pros**: Simple; might discourage frivolous submissions.  
  - **Cons**: Fee might hinder legitimate users with limited means.

- **Option B: Deposit/Stake Approach**  
  - **Pros**: Larger commitment from submitters; stake can be slashed for abuse.  
  - **Cons**: Implementation complexity; could lock up capital for honest users.

- **Option C: Token Burn**  
  - **Pros**: Reduces supply, benefiting token holders if spam is high.  
  - **Cons**: Might discourage usage if burn is too large.

We have decided that none of these options will work. Anti-spam measures will be built into the consensus mechanism.

---

## 14. Upgrade Mechanisms (Soft-Fork, Version Bits)
**Decision**: How to adopt new features or consensus changes.

- **Option A: Soft-Fork with Miner Signaling**  
  - **Pros**: Common approach (e.g., Bitcoin); allows backward compatibility.  
  - **Cons**: Requires majority miner buy-in; might be slow to activate changes.

- **Option B: Hard-Fork**  
  - **Pros**: Clear cut; no ambiguity if changes pass.  
  - **Cons**: Can split the chain if consensus isn’t unanimous.

- **Option C: Version Bits**  
  - **Pros**: Flexible approach for multiple parallel changes.  
  - **Cons**: Complexity in tracking multiple bits; requires robust code.

We have decided to go with **Option A.**

---

## 15. Exact Reward Value per Block
**Decision**: How many tokens to mint each block.

- **Option A: Fixed Small Reward (e.g., 10 Tokens/Block)**  
  - **Pros**: Predictable inflation; easy to model.  
  - **Cons**: Might become too small if adoption grows significantly.

- **Option B: Adjustable or Declining Reward**  
  - **Pros**: Mirrors typical halving schedules; reduces long-term inflation.  
  - **Cons**: Might discourage new pinning nodes if rewards shrink too fast.

- **Option C: Variable Reward Based on Network Usage**  
  - **Pros**: Dynamically incentivizes more pinning when demand is high.  
  - **Cons**: Potentially complex to implement or gameable.

We have decided to go with **Option C.** Miners will be rewarded for both their contributions to consensus as well as their pinning.

---

## 16. Handling Unclaimed Block Rewards (If No Valid PoP)
**Decision**: If a block is mined but no valid PoP is included, what happens to the reward?

- **Option A: Forfeit the Reward**  
  - **Pros**: Encourages miners to always include PoP.  
  - **Cons**: Miners might get nothing even if they do partial work.

- **Option B: Partial Reward or Secondary Distribution**  
  - **Pros**: Partial compensation for other efforts (e.g., transaction confirmations).  
  - **Cons**: Could reduce the incentive to properly store data.

We have decided to go with **Option A.**

---

## 17. Advanced Cryptographic Approaches (Zero-Knowledge, Enclaves)
**Decision**: Whether to adopt more complex privacy-preserving or integrity solutions.

- **Option A: Stick to Basic Signatures/Merkle Proofs**  
  - **Pros**: Simpler; fewer dependencies.  
  - **Cons**: Less privacy and advanced security features.

- **Option B: Zero-Knowledge Proofs**  
  - **Pros**: Enhanced privacy; can prove possession without revealing data.  
  - **Cons**: High complexity, potential performance hit, limited developer familiarity.

- **Option C: Trusted Execution Enclaves**  
  - **Pros**: Hardware-level security; can process data in a secure environment.  
  - **Cons**: Hardware trust model; not all network participants have enclaves.

We have decided to go with **Option A.**

---

## 18. Resource Limits (Avoiding Node Overload)
**Decision**: Setting caps on CPU, memory, or bandwidth usage.

- **Option A: Soft Limits (Warning Only)**  
  - **Pros**: Flexible; no forced stops.  
  - **Cons**: Might not prevent DDoS or resource exhaustion.

- **Option B: Strict Limits (Reject Overly Big Blocks/Docs)**  
  - **Pros**: Clear boundary; helps maintain stable node performance.  
  - **Cons**: Potential for legitimate large docs to be rejected.

- **Option C: Dynamic Throttling**  
  - **Pros**: Adjust usage based on current network load.  
  - **Cons**: Complexity; potential for unpredictable user experience.

We have decided to go with **Option B** with a hard limit of 20 MB.

---

## 19. Pinned SQLite Database Updates
**Decision**: Format for updated “snapshots” and how they’re pinned.

- **Option A: Single Monolithic DB File**  
  - **Pros**: Simple conceptually; all data in one place.  
  - **Cons**: Large file changes each time; potential heavy re-pinning.

- **Option B: Incremental/Delta Updates**  
  - **Pros**: Pin only changed segments; saves bandwidth.  
  - **Cons**: Complexity in applying diffs; must maintain accurate partial snapshots.

- **Option C: Multiple Smaller DB Partitions**  
  - **Pros**: Potential modular approach; partial re-pins.  
  - **Cons**: Harder to query globally without merging partitions.

We have decided to go with **Option A.**

---

## 20. PII Scrubbing: Naive vs. Advanced
**Decision**: Depth of data sanitization required to meet privacy goals.

- **Option A: Naive Regex**  
  - **Pros**: Lightweight, easy to update.  
  - **Cons**: High chance of missing new PII patterns or formats.

- **Option B: Heuristic/ML Scrubbing**  
  - **Pros**: More thorough; can adapt to novel patterns.  
  - **Cons**: Heavier compute; might require specialized NLP or OCR.

- **Option C: Manual/Community Audit**  
  - **Pros**: Community might catch errors; human intelligence.  
  - **Cons**: Inconsistency, time-consuming, not guaranteed for all docs.

We have decided to go with **Option C**, but note that above this should be extensible to someday include ML scrubbing.

---

## Conclusion

Each decision above has a **set of trade-offs**. The final design for RxRevoltChain will hinge on which choices best align with:

- **Mission Goals** (non-profit, high transparency, privacy protections),  
- **Technical Feasibility** (resources, developer skillset, network constraints),  
- **Community & Stakeholder Preferences** (governance, user experience).
