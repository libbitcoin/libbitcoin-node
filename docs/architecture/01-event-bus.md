# 01 — The Event Bus (`chase` events)

> Companion to [`00-overview.md`](00-overview.md). This is the authoritative,
> source-verified reference for every `chase` enumerator: payload, issuer
> sites, handler sites, and known discrepancies with the inline
> `chase.hpp` documentation.
>
> Every fact below is grep-derived. Methodology and queries are in
> [§4](#4-methodology) so any future change can be re-verified mechanically.

---

## 1. Subscriber model

### 1.1 Storage

```
event_subscriber_ : full_node              // include/bitcoin/node/full_node.hpp:198
   = network::desubscriber<object_key, chase, event_value>   // define.hpp:87
```

A `desubscriber` is the libbitcoin-network primitive that combines pub/sub
with explicit unsubscription via a key. All subscribers share one bus.

### 1.2 Subscribe protocol

There are two subscribe APIs, both eventually owning state on the
`full_node` strand:

| API                                                       | Used by   | Strand discipline                                              |
| --------------------------------------------------------- | --------- | -------------------------------------------------------------- |
| `subscribe_events(notifier) → object_key` (sync)          | chasers   | Caller must already be on `full_node` strand                   |
| `subscribe_events(notifier, completer)` (async)           | protocols | Posts to `full_node` strand internally; completer fires there  |

Chaser subscription path (`include/bitcoin/node/chasers/chaser.hpp:101`,
`src/chasers/chaser.cpp:92-94`):
```
chaser::subscribe_events
  → full_node::subscribe_events(handler)         // src/full_node.cpp:219-225
    → event_subscriber_.subscribe(handler, key)
```
The `BIND` macro that produces `notifier` re-posts the handler back to the
chaser's own strand, so handlers always execute stranded with respect to
their owning chaser. Each chaser subscribes exactly once, in its `start()`,
via `SUBSCRIBE_EVENTS(handle_event, _1, _2, _3)` — see
`include/bitcoin/node/chasers/chaser.hpp:167-168` for the macro.

Protocol subscription path (`src/protocols/protocol.cpp:72-89`):
```
protocol::subscribe_events
  → session::subscribe_events(handler, complete)  // src/sessions/session.cpp:86-90
    → full_node::subscribe_events(handler, complete) // src/full_node.cpp:227-242
      → posts to strand → do_subscribe_events
        → event_subscriber_.subscribe(handler, key)
        → complete(ec, key)
```
The async variant exists because a protocol is constructed on its channel
strand, not the node strand, so it must hop across.

### 1.3 Notify protocol

```
chaser/protocol calls notify(ec, chase::X, value)
   → full_node::notify posts to strand          // src/full_node.cpp:187-193
     → do_notify on strand
        → event_subscriber_.notify(ec, X, value)
           → for each subscriber: re-post to subscriber's own strand
```

### 1.4 Unsubscribe semantics

`full_node::unsubscribe_events(key)` does not remove the entry; it sends a
single `chase::stop` to that subscriber only
(`src/full_node.cpp:244-247`). The subscriber's `handle_event` returns
`false` on `chase::stop`, and the `desubscriber` interprets that as
removal. So:

> **Invariant (Bus-1).** A `false` return from `handle_event` means
> "remove me". Every chaser returns `false` on `chase::stop`
> (verified: each `chaser_*.cpp` has `case chase::stop: return false;`).

> **Invariant (Bus-2).** The bus is torn down only in `full_node::do_close`,
> which broadcasts `chase::stop` to all subscribers
> (`src/full_node.cpp:154`). After that point no further `notify` will
> deliver.

---

## 2. Verified event reference

Notation:
- **Payload** column shows the active variant member; types are in
  `include/bitcoin/node/define.hpp:70-75`.
- **Issuer** is every `notify(... chase::X ...)` or `notify_one(... chase::X ...)`
  call site that is not commented out.
- **Handler** is every `case chase::X:` arm that is not commented out.
- Discrepancies with the `chase.hpp` inline doc are flagged ⚠.

### 2.1 Work shuffling

| Event       | Payload    | Issuer (file:line)                                                                                      | Handler (file:line)                                                                                                                                                                                |
| ----------- | ---------- | ------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `start`     | `height_t` | `src/full_node.cpp:115` (`do_run`)                                                                      | `chaser_check.cpp:130`, `chaser_validate.cpp:77`, `chaser_confirm.cpp:75`                                                                                                                          |
| `space`     | `count_t`  | `src/full_node.cpp:279` (`fault` when `query_.is_full()`)                                               | `chaser_storage.cpp:84` (`chaser_snapshot.cpp` does *not* handle `space` currently)                                                                                                                |
| `snap`      | `height_t` | **none** ⚠ — chase.hpp says issued by `confirm`, but no live `notify(chase::snap, ...)` in source        | `chaser_snapshot.cpp:126`                                                                                                                                                                          |
| `bump`      | `height_t` | `chaser_organize.ipp:311` (after first non-stale candidate push)                                        | `chaser_check.cpp:131`, `chaser_validate.cpp:78`, `chaser_confirm.cpp:76`                                                                                                                          |
| `suspend`   | — (`{}`)   | `src/full_node.cpp:270` (`suspend`); **also** `chaser_organize.ipp:442` (after `do_disorganize`) ⚠      | `protocol_observer.cpp:77` (only)                                                                                                                                                                  |
| `resume`    | — (`{}`)   | `src/full_node.cpp:261`                                                                                 | `chaser_check.cpp:129`, `chaser_validate.cpp:76`, `chaser_confirm.cpp:74`                                                                                                                          |
| `starved`   | `object_t` | `src/protocols/protocol_block_in_31800.cpp:423`                                                         | `chaser_check.cpp:120`                                                                                                                                                                             |
| `split`     | `object_t` | `src/chasers/chaser_check.cpp:238` (via `notify_one`) ⚠                                                 | `src/protocols/protocol_block_in_31800.cpp:108`                                                                                                                                                    |
| `stall`     | `peer_t`   | `src/chasers/chaser_check.cpp:243` ⚠                                                                    | `src/protocols/protocol_block_in_31800.cpp:115`                                                                                                                                                    |
| `purge`     | `peer_t`   | `src/chasers/chaser_check.cpp:351`                                                                      | `src/protocols/protocol_block_in_31800.cpp:123`                                                                                                                                                    |
| `report`    | `count_t`  | **external** — issued by `executor` in libbitcoin-server, not in this repo                              | `src/protocols/protocol_block_in_31800.cpp:139`                                                                                                                                                    |

Discrepancy notes:
- ⚠ **`split`, `stall`**: `chase.hpp:60-66` lists `session_outbound` as the
  issuer, but the actual emits are in `chaser_check`. The functional intent
  matches (slow/stalled channel detection drives work redistribution), but
  the issuer is mislabeled in the inline doc.
- ⚠ **`suspend`** is also issued by `chaser_organize` after a
  disorganization (`chaser_organize.ipp:442`), not only by `full_node`. The
  comment in chase.hpp omits this.
- ⚠ **`snap`** has no live issuer anywhere in this repo. The handler in
  `chaser_snapshot.cpp:126` is wired but unreachable from the current
  source. Treat as a planned/dormant event.

### 2.2 Candidate chain

| Event          | Payload    | Issuer (file:line)                                                                                          | Handler (file:line)                                                                          |
| -------------- | ---------- | ----------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------- |
| `blocks`       | `height_t` | `chaser_organize.ipp:318` when `Block = system::chain::block` (via `chase_object()` helper)                 | None active. ⚠ `chaser_snapshot.cpp:89` has a commented-out arm. Spec-wise: handler dormant. |
| `headers`      | `height_t` | `chaser_organize.ipp:318` when `Block = system::chain::header`                                              | `chaser_check.cpp:151`                                                                       |
| `download`     | `count_t`  | `chaser_check.cpp:409`, `chaser_check.cpp:455`                                                              | `protocol_block_in_31800.cpp:130`                                                            |
| `regressed`    | `height_t` | `chaser_organize.ipp:265`                                                                                   | `chaser_check.cpp:142`, `chaser_validate.cpp:90`, `chaser_confirm.cpp:88`                    |
| `disorganized` | `height_t` | `chaser_organize.ipp:439`                                                                                   | `chaser_check.cpp:143`, `chaser_validate.cpp:91`, `chaser_confirm.cpp:89`                    |

Notes:
- `chase_object()` is the template helper at
  `chaser_organize.hpp:127-130` that selects `chase::blocks` vs
  `chase::headers` based on `Block` parameter. So one source line
  (`ipp:318`) is the issuer of *both* events — choose-by-template.
- `chase::blocks` has **no live handler** today (snapshot's handler is
  commented out at `chaser_snapshot.cpp:89`). This is consistent with the
  current emphasis on headers-first sync.

### 2.3 Check / Identify

| Event       | Payload    | Issuer (file:line)                                | Handler (file:line)                                                                                                  |
| ----------- | ---------- | ------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------- |
| `checked`   | `height_t` | `protocol_block_in_31800.cpp:348`                 | `chaser_check.cpp:136`, `chaser_validate.cpp:83` (snapshot arm commented out at `chaser_snapshot.cpp:90`)            |
| `unchecked` | `header_t` | `protocol_block_in_31800.cpp:324`                 | `chaser_organize.ipp:89` (handled identically with `unvalid`, `unconfirmable`)                                       |

### 2.4 Accept / Connect

| Event     | Payload    | Issuer (file:line)         | Handler (file:line)                                                                                            |
| --------- | ---------- | -------------------------- | -------------------------------------------------------------------------------------------------------------- |
| `valid`   | `height_t` | `chaser_validate.cpp:330`  | `chaser_check.cpp:157`, `chaser_confirm.cpp:81` (snapshot arm commented out at `chaser_snapshot.cpp:99`)       |
| `unvalid` | `header_t` | `chaser_validate.cpp:321`  | `chaser_organize.ipp:90` (treated as disorganize trigger)                                                      |

### 2.5 Confirm (block)

| Event           | Payload    | Issuer (file:line)        | Handler (file:line)                                                  |
| --------------- | ---------- | ------------------------- | -------------------------------------------------------------------- |
| `confirmable`   | `header_t` | `chaser_confirm.cpp:345`  | None active (snapshot arm commented out at `chaser_snapshot.cpp:108`) |
| `unconfirmable` | `header_t` | `chaser_confirm.cpp:338`  | `chaser_organize.ipp:91`                                             |

### 2.6 Confirm (chain) and Mining

| Event           | Payload         | Issuer (file:line)                  | Handler (file:line)                                                                              |
| --------------- | --------------- | ----------------------------------- | ------------------------------------------------------------------------------------------------ |
| `block`         | `header_t`      | `chaser_confirm.cpp:427` ⚠          | `chaser_snapshot.cpp:117`, `protocol_block_out_106.cpp:75`, `protocol_header_out_70012.cpp:71`   |
| `organized`     | `header_t`      | `chaser_confirm.cpp:396`            | None active (chase.hpp predicts `transaction` consumer; not wired)                                |
| `reorganized`   | `header_t`      | `chaser_confirm.cpp:363`            | None active (chase.hpp predicts `transaction` consumer; not wired)                                |
| `transaction`   | `transaction_t` | `chaser_transaction.cpp:85`         | `chaser_template.cpp:67`, `protocol_transaction_out_106.cpp:72`                                  |
| `template_`     | `height_t`      | **none** ⚠ — chase.hpp says `template` issues; currently dormant | (miners, external) |

Discrepancy:
- ⚠ **`chase::block`**: `chase.hpp:138` says "Issued by 'transaction' and
  handled by 'protocol_header/block_out'". In reality `chaser_confirm`
  issues it (`chaser_confirm.cpp:427`), and `chaser_snapshot` is *also* a
  live consumer (`chaser_snapshot.cpp:117`).

### 2.7 Stop

| Event  | Payload | Issuer (file:line)                                                                            | Handler                  |
| ------ | ------- | --------------------------------------------------------------------------------------------- | ------------------------ |
| `stop` | — (`{}`) | `full_node::unsubscribe_events` via `notify_one` (`src/full_node.cpp:246`); `full_node::do_close` via `event_subscriber_.stop(...)` (`src/full_node.cpp:154`) | Every chaser & protocol  |

---

## 3. Verified issuer / handler diagram

This replaces and supersedes the pipeline diagram in `00-overview.md §5`,
with one node per *actual* source location rather than per chase.hpp
comment.

```mermaid
flowchart LR
    %% Issuers
    FN[full_node]
    ORG["chaser_organize&lt;Block&gt;\n(template; instantiated as\nchaser_header and chaser_block)"]
    CHK[chaser_check]
    VAL[chaser_validate]
    CNF[chaser_confirm]
    TX[chaser_transaction]
    PIN["protocol_block_in_31800"]

    %% Handlers (only those that introduce new edges)
    SNP[chaser_snapshot]
    STG[chaser_storage]
    TPL[chaser_template]
    OBS[protocol_observer]
    POUT_B["protocol_block_out_106"]
    POUT_H["protocol_header_out_70012"]
    POUT_T["protocol_transaction_out_106"]

    %% Edges (event labels)
    FN -- "start, resume, suspend, space, stop" --> CHK
    FN -- "start, resume" --> VAL
    FN -- "start, resume" --> CNF
    FN -- "space" --> STG
    FN -- "suspend" --> OBS
    ORG -- "regressed, disorganized, bump" --> CHK
    ORG -- "regressed, disorganized, bump" --> VAL
    ORG -- "regressed, disorganized, bump" --> CNF
    ORG -- "suspend (after disorganize)" --> OBS
    ORG -- "headers" --> CHK
    ORG -. "blocks (no live consumer)" .-> SNP
    PIN -- "checked" --> CHK
    PIN -- "checked" --> VAL
    PIN -- "unchecked" --> ORG
    PIN -- "starved" --> CHK
    CHK -- "download, split, stall, purge" --> PIN
    CHK -- "purge (also re-handles disorganize indirectly via organize)" --> ORG
    VAL -- "valid" --> CHK
    VAL -- "valid" --> CNF
    VAL -- "unvalid" --> ORG
    CNF -- "confirmable" -. "(dormant)" .-> SNP
    CNF -- "unconfirmable" --> ORG
    CNF -- "block" --> SNP
    CNF -- "block" --> POUT_B
    CNF -- "block" --> POUT_H
    TX -- "transaction" --> TPL
    TX -- "transaction" --> POUT_T

    style ORG fill:#eef
    style CNF fill:#fee
    style VAL fill:#fee
    style CHK fill:#fee
```

(Solid = live; dashed = wired in source but currently dormant.)

---

## 4. Methodology

The tables above were generated by these greps from repo root:

```sh
# Issuers (excluding notify_one)
grep -rn 'notify(.*chase::' src include | grep -v 'notify_one'

# Issuers via notify_one
grep -rn 'notify_one(.*chase::' src include

# Handlers
grep -rn 'case chase::' src include

# Subscription sites
grep -rn 'SUBSCRIBE_EVENTS\|subscribe_events' src

# Indirect issuers via the chase_object() template helper
grep -rn 'chase_object' src include
```

To recheck after source changes, run the same set and diff against the
tables here. Any new row must come with its source citation, and any
deletion should be cross-referenced with this doc to ensure no downstream
spec/Lisp port assumes the old behaviour.

---

## 5. Specification view

A formal model can treat the bus as follows.

**Processes.** Each chaser and each event-subscribing protocol is a
process `P` with:
- a single inbox typed `(code, chase, event_value)`
- a private state space `Σ_P`
- a deterministic transition `δ_P : Σ_P × (code, chase, event_value) → Σ_P × Output_P`
- where `Output_P` is a finite set of `notify` calls and outbound
  asynchronous actions (e.g. DB writes).

**Bus.** The bus is broadcast-with-filter: every `notify(ec, X, v)` is
delivered to every subscriber, and each subscriber's `handle_event`
inspects the `chase` value to decide whether to act.

**Invariants worth proving.**
1. *Liveness of stop*: after `full_node::do_close`, every process
   eventually reaches a terminal state. (Follows from Bus-2 + the universal
   `case chase::stop: return false;`.)
2. *Disorganize convergence*: every `chase::regressed`/`disorganized` is
   eventually responded to by the consuming chasers' "rewind" branches
   (`chaser_check.cpp:142-143`, `chaser_validate.cpp:90-91`,
   `chaser_confirm.cpp:88-89`) before the next forward event is processed.
3. *No spurious validation*: `chase::valid` is emitted only after
   `chaser_validate` has executed its validation routine on the height
   (`chaser_validate.cpp:330` is the unique emit site).
4. *Confirm uniqueness*: `chase::confirmable`, `unconfirmable`,
   `organized`, `reorganized`, `block` all have a single issuer
   (`chaser_confirm`). Confirmation is centralized.

**Dormant events (do not model as live).**
- `chase::snap` — handler exists, no issuer.
- `chase::template_` — handler is "miners" (external), no issuer in this
  repo.
- `chase::report` — issuer is `executor` (external, libbitcoin-server).

---

## Cross-references

- [`00-overview.md`](00-overview.md) §3 (concurrency model) and §4
  (high-level event table).
- Future doc `02-chaser-organize.md` — the templated state machine that
  emits `bump`, `regressed`, `disorganized`, `headers`/`blocks`,
  `suspend`.
- Future doc `04-chaser-validate.md` — the unique issuer of `valid` /
  `unvalid`.
