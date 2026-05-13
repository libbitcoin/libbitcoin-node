# 10 — Transaction protocols (in/out 106)

> Companion to [`06-sessions-and-protocols.md`](06-sessions-and-protocols.md),
> [`08-block-out-protocols.md`](08-block-out-protocols.md), and
> [`09-filter-out-70015.md`](09-filter-out-70015.md).
>
> The transaction protocols implement BIP35-era loose-tx relay. There
> are two versioned classes:
>
> - **`protocol_transaction_in_106`** — receive `inv(tx)` from peer,
>   request and ingest. **Currently stubbed**: the handler exists but
>   takes no action. Tx ingestion via this protocol is not implemented
>   in this repo.
> - **`protocol_transaction_out_106`** — emit `inv(tx)` on bus
>   `chase::transaction`; serve `get_data(tx)` requests.
>
> Both are pre-BIP339; the inline TODOs note that wtxidrelay handling
> is future work. There is no `transaction_*_70012` family — BIP130
> does not apply to transactions, only headers.

| File                                                    | Lines | Role                                                              |
| ------------------------------------------------------- | ----- | ----------------------------------------------------------------- |
| `src/protocols/protocol_transaction_in_106.cpp`         |  76   | Stub: subscribes to `inv`, ignores                                 |
| `src/protocols/protocol_transaction_out_106.cpp`        | 194   | Outbound announce + serve                                          |

---

## 1. Why these are small

A full Bitcoin node has a *mempool* — an in-memory store of unconfirmed
transactions used for mining templates and relay. In this codebase, the
mempool is represented by `chaser_transaction` and `chaser_template`,
both of which are currently **mostly inactive**:

- `chaser_transaction` issues a single `chase::transaction(...)` event
  at startup (`chaser_transaction.cpp:85`, the only live emit; another
  emit at `:105` is commented out) and otherwise has no body beyond
  the `chase::stop` handler.
- `chaser_template` consumes `chase::transaction` but currently issues
  no `chase::template_` events.

Consequently the transaction relay path is **wired but largely
dormant**. The protocols are present for the case where a future
mempool/template implementation drives them.

> **Invariant (TxProto-State-1).** As of this repo state, the
> outbound protocol will emit at most one `inv(tx)` announcement at
> startup (driven by the single `chase::transaction` emit from
> `chaser_transaction`). Operational tx relay requires upstream
> changes.

> **Invariant (TxProto-State-2).** The inbound protocol is a stub;
> `inv(tx)` from peers is observed but no `getdata` is sent. Tx
> ingestion does not currently produce store mutations.

---

## 2. `protocol_transaction_in_106` — stub

### 2.1 Full implementation

```cpp
// :38-47
void start() {
    SUBSCRIBE_CHANNEL(inventory, handle_receive_inventory, _1, _2);
    protocol_peer::start();
}

// :55-69
bool handle_receive_inventory(ec, message) {
    if (stopped(ec)) return false;
    // TODO: get and handle tx as required.
    ////const auto tx_count = message->count(type_id::transaction);
    ////set_announced(hash);
    return true;
}
```

That's all of it. The commented-out lines (`:66-67`) show the intended
shape: count tx-typed items in the inventory and record each as
"announced from this peer" via `set_announced(hash)` so that the
*out* protocol won't echo it back. None of this is currently active.

### 2.2 TODO breadcrumbs

Inline comments (`:51-53, :65`):

> *"TODO: bip339: After a node has received a wtxidrelay message
> from a peer, the node SHOULD use a MSG_WTX getdata message to
> request any announced transactions."*

> *"TODO: get and handle tx as required."*

This protocol will need:

1. A subscription to peer `wtxidrelay` to decide between MSG_TX and
   MSG_WTX `getdata`.
2. A handler that calls `get_data` for unknown tx hashes.
3. A `tx` message handler that validates and stores the tx.
4. A `chase::transaction(link)` emission point (the only one currently
   in the codebase is `chaser_transaction`, but the natural source
   would be this protocol — see §6).

> **Invariant (TxIn-Stub-1).** No bus events are emitted, no store
> mutations are performed, no further messages are sent in response
> to `inv`. The protocol exists solely to subscribe and (in future)
> implement.

---

## 3. `protocol_transaction_out_106` — announce + serve

### 3.1 Subscriptions

```cpp
// :39-50
void start() {
    subscribe_events(BIND(handle_event, _1, _2, _3));      // bus
    SUBSCRIBE_CHANNEL(get_data, handle_receive_get_data, _1, _2);
    protocol_peer::start();
}

// :52-58
void stopping(ec) {
    unsubscribe_events();
    protocol_peer::stopping(ec);
}
```

One bus subscription, one channel subscription. Symmetric to
`protocol_block_out_106` ([`08 §2.1`](08-block-out-protocols.md#21-subscriptions))
but for tx not block.

### 3.2 Bus event handler — `chase::transaction → do_announce`

```cpp
// :63-86
bool handle_event(ec, event_, value) {
    if (stopped()) return false;
    switch (event_) {
        case chase::transaction: {
            POST(do_announce, std::get<transaction_t>(value));
            break;
        }
        default: break;
    }
    return true;
}

// :93-117
bool do_announce(transaction_t link) {
    const auto hash = query.get_tx_key(link);
    if (was_announced(hash))                 // anti-echo
        return true;
    if (hash == null_hash) return true;      // store inconsistency; logged only
    SEND(inventory{ { { type_id::transaction, hash } } }, handle_send, _1);
    return true;
}
```

> **Invariant (TxOut-Announce-1).** Same anti-echo discipline as
> `protocol_block_out_106` and `protocol_header_out_70012`: a peer
> that announced a tx to us does not receive an echo. Implementation
> identical except for the `type_id::transaction` selector.

> **Invariant (TxOut-Announce-2).** `chase::transaction(link)` is
> emitted only by `chaser_transaction` (verified in
> [`01-event-bus.md §2.6`](01-event-bus.md#26-confirm-chain-and-mining)).
> Currently fires at most once (at startup), so the announce path is
> driven by upstream rather than by tx relay traffic.

### 3.3 Inbound `get_data` — streamed tx send

The pattern mirrors `protocol_filter_out_70015` (one-shot subscription
during streaming, resubscribe on completion) but threads through the
peer's `get_data.items` list rather than an ancestry list.

```cpp
// :122-133
bool handle_receive_get_data(ec, message) {
    if (stopped(ec)) return false;
    send_transaction(error::success, zero, message);    // start at index 0
    return false;                                       // ← one-shot
}
```

```cpp
// :142-188 (simplified)
void send_transaction(ec, size_t index, get_data::cptr message) {
    if (stopped(ec)) return;

    // Skip over non-tx inventory items
    for (; index < message->items.size(); ++index)
        if (message->items.at(index).is_transaction_type())
            break;

    // BUGBUG: registration race.
    if (index >= message->items.size()) {
        SUBSCRIBE_CHANNEL(get_data, handle_receive_get_data, _1, _2);   // resubscribe
        return;
    }

    const auto& item = message->items.at(index);
    const auto witness = item.is_witness_type();
    if (!node_witness_ && witness) {
        stop(network::error::protocol_violation);
        return;
    }

    const auto ptr = query.get_transaction(query.to_tx(item.hash), witness);
    if (!ptr) {
        stop(system::error::not_found);
        return;
    }

    SEND(transaction{ ptr }, send_transaction, _1, sub1(index), message);
}
```

> **Invariant (TxOut-Stream-1).** One outstanding `tx` send per
> channel; the recursive `send_transaction` callback chains them.
> Same shape as block-out and filter-out streaming.

> **Invariant (TxOut-Stream-2).** Resubscription to `get_data`
> happens only after the entire request's tx items have been served
> (or skipped). Until then, the channel is unsubscribed from
> `get_data` — a second incoming `get_data` while streaming will
> hit the libbitcoin-network "no handler" path.

> ⚠ **Suspect: the `sub1(index)` continuation.** At `:187`,
> the next iteration is scheduled with `sub1(index)` (= `index - 1`),
> not `add1(index)` (= `index + 1`). The loop top is
> `for (; index < size; ++index)`, so the next call enters with
> `index - 1`, the `for` test passes, the inner `if` either matches
> at `index - 1` or `++index` runs and matches at the same `index`
> we just sent. Either way the same tx may be sent again. This
> reading suggests a possible off-by-one — likely intended
> `add1(index)`. Worth reviewing against intent before mirroring in
> a port. Flagged here rather than asserted as a bug because we have
> not built or run the codebase; there may be a subtlety we are
> missing.

### 3.4 Witness gating

```cpp
// :164-170
if (!node_witness_ && witness)
    stop(network::error::protocol_violation);
```

Same logic as `protocol_block_out_106` (`08 §2.6`): a node that
doesn't advertise witness service drops any peer that requests
witness data.

### 3.5 BIP339 TODOs

Multiple inline TODOs at `:51-53, :90-91, :137-140` reference
**BIP339 (`wtxidrelay`)**: after exchanging `wtxidrelay`, the
inv/getdata semantics for transactions switch to `MSG_WTX` (using
witness txids). This is not yet implemented; the protocol currently
uses MSG_TX universally.

---

## 4. Attachment

From `session_peer.ipp:156-160`:

```cpp
if (txs_in_out) {
    if (peer->peer_version()->relay)
        channel->attach<protocol_transaction_out_106>(self)->start();
}
```

where `txs_in_out = relay && peer.is_negotiated(bip37) && (!delay || is_current(true))`.

Two important consequences:

> **Invariant (TxAttach-1).** Only the OUT protocol is attached
> here. `protocol_transaction_in_106` is **never attached** in this
> repo's current attach tree. This explains why its stubbed status is
> not yet a problem — it cannot receive anything because it isn't
> wired in.

> **Invariant (TxAttach-2).** Tx out attaches only when (a) relay is
> enabled, (b) peer has negotiated BIP37 (mempool filtering), and (c)
> the peer's version message claimed `relay = true`. So tx
> announcement traffic is opt-in on both sides.

---

## 5. Bus integration

| Protocol                          | Subscribes to        | Emits           |
| --------------------------------- | -------------------- | --------------- |
| `protocol_transaction_in_106`     | none                 | none            |
| `protocol_transaction_out_106`    | `chase::transaction` | none            |

The out protocol never *emits* bus events. It is a pure consumer of
`chase::transaction`. Compare with `protocol_block_in_31800` which
both consumes (`chase::download`, etc.) and emits (`chase::checked`,
`chase::unchecked`, `chase::starved`) — block-in is much more
deeply integrated.

---

## 6. Where `chase::transaction` comes from

Currently only `chaser_transaction.cpp:85`:

```cpp
notify(error::success, chase::transaction, transaction_t{});
```

This is called from the constructor path with a *zero* `transaction_t`
value. The commented-out emit at `:105` would be the operational
emission point — once mempool ingestion exists, each accepted tx
would emit `chase::transaction(link)`.

> **Invariant (TxEvent-1).** Until `chaser_transaction` is fleshed
> out, the out protocol's bus subscription receives effectively no
> traffic. The protocol is *correctly idle*, not broken — it would
> activate the moment the chaser starts emitting.

The natural extension path:

1. Implement `protocol_transaction_in_106::handle_receive_inventory`
   to `getdata` for unknown tx hashes.
2. Add a `tx` message handler that runs consensus + policy checks and
   calls a store mutation (analogous to `set_code` for blocks).
3. Emit `chase::transaction(link)` from that handler — or from a
   chaser that consumes a per-channel "received tx" message.
4. Add `chaser_transaction` body that drives mempool eviction,
   miner-template trigger, etc.

---

## 7. State machine view (out_106)

```mermaid
stateDiagram-v2
    [*] --> SUBSCRIBED: start (subscribe bus + get_data)
    SUBSCRIBED --> SUBSCRIBED: chase::transaction → do_announce → SEND inv
    SUBSCRIBED --> STREAMING: get_data → unsubscribe from get_data\nsend_transaction(0, msg)
    STREAMING --> STREAMING: SEND tx; send_transaction(sub1(i), msg)
    STREAMING --> SUBSCRIBED: index ≥ size → resubscribe to get_data
    STREAMING --> DROPPED: witness mismatch / not_found / send error
    SUBSCRIBED --> [*]: stop / chase::stop
    STREAMING --> [*]: stop
    DROPPED --> [*]
```

State space (per channel): `{SUBSCRIBED, STREAMING}` — same as
filter-out.

---

## 8. Error / outcome inventory

| Site                                          | Code                                  | Trigger                                       |
| --------------------------------------------- | ------------------------------------- | --------------------------------------------- |
| `:167-169`                                    | `protocol_violation`                  | witness requested but `node_witness_` false   |
| `:183`                                        | `system::error::not_found`            | tx requested but missing from store           |

No node-faults. No store mutations originate here (the out protocol is
strictly read+send).

---

## 9. Spec view

### 9.1 As processes

```
protocol_transaction_in_106 : Process    (stub)
  state: ∅
  inputs: peer inv(tx)
  outputs: none (ignored)

protocol_transaction_out_106 : Process
  state: streaming : Bool
  inputs:
    bus chase::transaction(link) → emit inv(tx) [filtered by was_announced]
    peer get_data(items) → enter streaming
    send_transaction continuation
  outputs:
    peer inv(tx) | tx messages
    drop_channel
  store reads: get_tx_key, to_tx, get_transaction
```

### 9.2 Safety properties

1. **Anti-echo** (TxOut-Announce-1).
2. **Single in-flight stream** (TxOut-Stream-1).
3. **Witness consistency**: never serves witness tx if not advertising
   witness service.
4. **Stub no-op** (TxIn-Stub-1, TxProto-State-2): the in protocol
   produces no observable effects.

### 9.3 Liveness

Bounded entirely by upstream. Until `chase::transaction` fires
frequently, the out protocol is idle.

### 9.4 Open question for the spec

The `sub1(index)` continuation (§3.3) needs verification against
intended behaviour. If it's a bug, a fix to `add1(index)` is a
one-character change. If it's intentional, the rationale is non-obvious
and warrants a comment in the source.

---

## 10. Notes for the Lisp port

- The in protocol is currently a no-op; mirror it as such until the
  mempool design is settled.
- The out protocol is a near-clone of block-out: one announce path,
  one streaming serve path. Reuse the same actor template.
- The `sub1` vs `add1` continuation should be tested when porting.

---

## 11. Notes for the formal model

- The in protocol contributes no transitions; it can be modelled as
  identity.
- The out protocol's streaming state machine is identical in shape
  to filter-out and block-out — three serialised request types
  collapse to one "drain queue then resubscribe" pattern in the
  abstract.
- The dormancy of `chase::transaction` is a *deployment* fact, not a
  model property. A spec should still encode the protocol's intended
  behaviour given an active source.

---

## Cross-references

- [`01-event-bus.md`](01-event-bus.md) §2.6 (`chase::transaction` —
  emitter / consumer table)
- [`06-sessions-and-protocols.md`](06-sessions-and-protocols.md) §2.3
  — `txs_in_out` attach predicate
- [`08-block-out-protocols.md`](08-block-out-protocols.md) §2.5
  (streaming send loop — same pattern, working version)
- [`09-filter-out-70015.md`](09-filter-out-70015.md) §6 (streaming +
  resubscribe pattern — same pattern, working version)
- BIPs 35, 37, 144, 339 (external)
