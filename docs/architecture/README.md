# libbitcoin-node — Architecture documentation

This directory contains a top-down, source-anchored description of
libbitcoin-node. It was written to serve two downstream purposes:

1. A **Lisp re-implementation** — needs precise functional
   decomposition, explicit module boundaries, and concurrency
   semantics.
2. A **formal-verification effort** — needs state machines,
   invariants, pre/post-conditions, and a clear bus-vs.-store
   ownership model.

Every behavioural claim is anchored with `file:line` references into
the C++ source so it can be re-verified or re-derived if source drifts.

---

## The 13 documents

| #  | File                                                                      | Topic                                                                       |
| -- | ------------------------------------------------------------------------- | --------------------------------------------------------------------------- |
| 00 | [`00-overview.md`](00-overview.md)                                        | Top-down map: layer stack, object graph, threading model, lifecycle         |
| 01 | [`01-event-bus.md`](01-event-bus.md)                                      | Every `chase` event with verified issuer/handler sites; methodology         |
| 02 | [`02-chaser-organize.md`](02-chaser-organize.md)                          | Templated header+block organize state machine; 15 `organizeN` fault sites   |
| 03 | [`03-chaser-check.md`](03-chaser-check.md)                                | Block-download orchestration; race_all purge barrier; σ slow-peer detector |
| 04 | [`04-chaser-validate.md`](04-chaser-validate.md)                          | Consensus validation; own threadpool + backlog; 8 `validateN` fault sites   |
| 05 | [`05-chaser-confirm.md`](05-chaser-confirm.md)                            | UTXO double-spend check + confirmed-chain writer; 12 `confirmN` fault sites |
| 06 | [`06-sessions-and-protocols.md`](06-sessions-and-protocols.md)            | Sessions, the protocol attach tree, `protocol_block_in_31800`                |
| 07 | [`07-header-protocols.md`](07-header-protocols.md)                        | Header in/out at 31800 + 70012; anti-echo discipline                         |
| 08 | [`08-block-out-protocols.md`](08-block-out-protocols.md)                  | Block out at 106 + 70012 supersede gate; streaming send loop                 |
| 09 | [`09-filter-out-70015.md`](09-filter-out-70015.md)                        | BIP157/158 client filters; three request types; one-shot stream trick         |
| 10 | [`10-tx-protocols.md`](10-tx-protocols.md)                                | Tx in (stub) + tx out at 106; flags a suspect `sub1`/`add1` line             |
| 11 | [`11-protocol-block-in-106.md`](11-protocol-block-in-106.md)              | Legacy blocks-first sync (used when peer doesn't negotiate `headers_protocol`) |
| 12 | [`12-periphery-chasers.md`](12-periphery-chasers.md)                      | `chaser_snapshot`, `chaser_storage`, `chaser_template`, `chaser_transaction`  |

---

## Recommended reading orders

### For a new contributor
Read in order: **00 → 01 → 02 → 03 → 04 → 05** for the consensus
pipeline, then **06 → 07/08/11** for the corresponding peer protocols.
Skim 09, 10, 12 as needed.

### For a Lisp re-implementer
Start with **00** (layer stack + object graph), then **01** (the
event bus is the system's interface backbone — read this carefully).
Then **02–05** for the consensus pipeline, paying attention to each
doc's "Notes for the Lisp port" §s. Network layer
(**06–11**) maps cleanly onto per-channel actors. Periphery
(**12**) can be deferred — three of the four chasers there are stubs
or near-stubs.

### For a formal-verification reader
**01** is foundational — the `chase` events are the inter-process
interface. Then **02 §3, §5, §7** (organize state machine, disorganize
state machine, error-code → proof-obligation list), **04 §4, §6**
(validate consensus + error inventory), **05 §4–§8** (confirm
algorithm, rollback atomicity, error inventory). The numbered
invariants throughout (`Validate-Backlog-1`, `Confirm-Rollback-1`,
etc.) are the recommended target set for a TLA+/Alloy spec.

### For a peer-protocol implementer
**00 §7** for the network-layer overview, then **06** for the
attach tree, then read whichever versioned family you need
(**07** for headers, **08+11** for blocks, **09** for filters,
**10** for tx).

---

## Conventions used throughout

### File:line citations
Every non-trivial behavioural claim has a `path/to/file.cpp:N` (or
`.hpp`, `.ipp`) citation. If you change source, run
`grep -rn 'pattern' src include` from the repo root to find new
locations and update the citation.

### Numbered invariants
Each doc names its invariants in the form `Topic-N` (e.g.
`Validate-Bypass-1`, `Confirm-Rollback-1`). The intent is that a
formal-spec encoding can cite them stably. Major invariant
families:

| Prefix          | Doc                              | Roughly                                            |
| --------------- | -------------------------------- | -------------------------------------------------- |
| `Concurrency-*` | 00                               | Strand discipline across the node                  |
| `Lifecycle-*`   | 00                               | start/run/close ordering                            |
| `Store-*`       | 00                               | Suspend/resume around store maintenance             |
| `Bus-*`         | 01                               | Subscription/unsubscription semantics               |
| `Organize-*`    | 02                               | Template state machine                              |
| `Check-*`       | 03                               | Download orchestration                              |
| `Validate-*`    | 04                               | Consensus validation                                |
| `Confirm-*`     | 05                               | Confirmation + UTXO                                 |
| `Session-*`, `Attach-*`, `Protocol-*`, `Observer-*`, `Performer-*`, `BlockIn-*`, `HeaderIn-*`, `HeaderOut-*`, `HeaderBus-*`, `Anti-Echo-*`, `BlockOut-*`, `Filter-*`, `TxProto-*`, `TxAttach-*`, `TxOut-*`, `TxIn-*`, `TxEvent-*`, `BlockIn106-*` | 06–11 | Network layer |
| `Snapshot-*`, `Storage-*`, `Periphery-*`, `TxChaser-*` | 12 | Periphery chasers |

### Mermaid diagrams
All diagrams use Mermaid (sequence, flowchart, stateDiagram-v2,
classDiagram). They render natively on GitHub.

### "Spec view" / "Notes for the Lisp port" / "Notes for the formal model"
Most subsystem docs have these three closing sections. They are the
distilled-for-export view of each subsystem.

---

## Honest caveats

1. **No build verification.** Citations are textual. They were
   re-grepped during writing but the code has not been built or
   executed during this documentation effort. A version drift
   between source and docs is possible.
2. **`block.accept` / `block.connect` semantics** live in
   libbitcoin-system, not this repo. Docs 02/04/05 describe the
   *sequencing* of those calls, not their content. The consensus
   surface is delegated to libbitcoin-system documentation.
3. **Store consistency** is treated as an oracle. Every numbered
   `organizeN`/`validateN`/`confirmN` fault is documented as a proof
   obligation against store-consistency invariants supplied by
   libbitcoin-database. A full proof would require importing those.
4. **Discrepancies with `chase.hpp` comments**: the inline event-bus
   doc in `chase.hpp` has several stale entries (issuer
   misattributions, dormant events). All are flagged in
   [`01-event-bus.md`](01-event-bus.md) with `⚠` and discussed in
   §2 of that doc.
5. **Stub subsystems**: `chaser_template`, `chaser_transaction`, and
   `protocol_transaction_in_106` are stubs. The docs describe their
   wiring and intended design but note clearly where current
   behaviour is "no-op".
6. **Reviewer-confirmed corrections**: a first pass of reviewer
   feedback (evoskuil) corrected several claims; the affected docs
   are 02 (milestone semantics, NDEBUG polarity, tree_ DoS), 03
   (`is_current(false)` is candidate-chain), 04 (consensus is split
   across multiple stages, not concentrated here), 05 (NDEBUG
   polarity, expanded `block_confirmable` description), 06
   (session-template diagram, "recent" vs "current"), 08
   (`superseded_` atomic rationale), 09 (no-handler messages are
   ignored, not protocol violations), 10 (the previously-flagged
   `sub1`/`add1` at `protocol_transaction_out_106.cpp:187` was an
   off-by-one bug, fixed in PR #1007), 11 (order-discipline is the
   same as headers-first; BIP130 typo), and 12 (chaser_storage
   timer runs on the network threadpool via the chaser's strand).

---

## Coverage map

```
libbitcoin-node
├── src/
│   ├── full_node.cpp ........................ 00
│   ├── configuration.cpp / settings.cpp ..... (mentioned in 00, not detailed)
│   ├── error.cpp ............................ 00 §9
│   ├── block_arena.cpp / block_memory.cpp ... 00 §8 (overview only)
│   │
│   ├── chasers/
│   │   ├── chaser.cpp ....................... 00 §3 (base; per-doc as needed)
│   │   ├── chaser_block.cpp ................. 02 §4
│   │   ├── chaser_header.cpp ................ 02 §4, §6.2
│   │   ├── chaser_check.cpp ................. 03
│   │   ├── chaser_validate.cpp .............. 04
│   │   ├── chaser_confirm.cpp ............... 05
│   │   ├── chaser_snapshot.cpp .............. 12 §1
│   │   ├── chaser_storage.cpp ............... 12 §2
│   │   ├── chaser_template.cpp .............. 12 §3
│   │   └── chaser_transaction.cpp ........... 12 §4
│   │
│   ├── sessions/
│   │   ├── session.cpp ...................... 06 §1
│   │   ├── session_inbound.cpp .............. 06 §1.3
│   │   ├── session_outbound.cpp ............. 06 §1 (typedef)
│   │   └── session_manual.cpp ............... 06 §1 (typedef)
│   │
│   └── protocols/
│       ├── protocol.cpp ..................... 06 §3
│       ├── protocol_peer.cpp ................ 06 (referenced); 07 §8 anti-echo
│       ├── protocol_observer.cpp ............ 06 §3.2
│       ├── protocol_performer.cpp ........... 06 §4
│       │
│       ├── protocol_block_in_106.cpp ........ 11
│       ├── protocol_block_in_31800.cpp ...... 06 §5
│       ├── protocol_block_out_106.cpp ....... 08 §2
│       ├── protocol_block_out_70012.cpp ..... 08 §3
│       │
│       ├── protocol_header_in_31800.cpp ..... 07 §2
│       ├── protocol_header_in_70012.cpp ..... 07 §3
│       ├── protocol_header_out_31800.cpp .... 07 §4
│       ├── protocol_header_out_70012.cpp .... 07 §5
│       │
│       ├── protocol_filter_out_70015.cpp .... 09
│       ├── protocol_transaction_in_106.cpp .. 10 §2
│       └── protocol_transaction_out_106.cpp . 10 §3
│
└── include/bitcoin/node/
    ├── full_node.hpp ........................ 00
    ├── chase.hpp ............................ 00 §4; 01 (verified against source)
    ├── events.hpp ........................... 00 §4 (metrics enum)
    ├── error.hpp ............................ 00 §9
    └── impl/chasers/chaser_organize.ipp ..... 02
```

Files **not** detailed (treated as "use as documented"):
- `configuration.cpp` / `settings.cpp` — straightforward configuration
- `messages/` headers — wire types from libbitcoin-network
- `channels/` headers — channel base from libbitcoin-network with a
  small node-specific subclass

---

## Next steps (suggested follow-on docs, not yet written)

- **`docs/spec/`** — TLA+ or Alloy skeletons encoding the numbered
  invariants. The recommended starting set: `Validate-Backlog-1`,
  `Validate-Ordering-1`, `Confirm-Rollback-1`, `Confirm-Order-1`,
  `Organize-Disorg-1`, `Bus-1..2`. These are the smallest set whose
  proofs would cover the consensus-critical safety claims.
- **`docs/lisp/`** — per-subsystem porting notes that turn each
  doc's "Notes for the Lisp port" section into concrete data type
  + module layouts.
- **A change log** — when source changes, the affected `file:line`
  citations should be updated. A `docs/architecture/CHANGES.md`
  recording which docs were re-verified at which git SHA would
  help keep the corpus honest over time.

---

## How to update these docs after source changes

1. Run the verification greps from
   [`01-event-bus.md §4`](01-event-bus.md#4-methodology) to detect
   bus-graph drift.
2. For per-chaser docs, the relevant invariants reference specific
   `file:line`s — re-grep those when their files change.
3. The error inventories in each chaser doc (`organizeN`,
   `validateN`, `confirmN`) are exhaustive at the time of writing
   — any new `error::<chaser>N` entry should add a row.
4. The attach tree in
   [`06 §2.3`](06-sessions-and-protocols.md#23-attach_protocolschannel----line-session_peeripp57-161)
   should be re-checked whenever `session_peer.ipp` is modified.
