# Never regenerate a signing key silently

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

A signing key is the server's federation identity. When no *usable* key is
found, the server could mint a fresh one and carry on, or fail.

Minting silently is superficially attractive: it always produces a running
server.

## Considered Options

* Generate only on first boot (no usable key stored) or through an explicit
  rotation
* Regenerate whenever no usable key is found

## Decision Outcome

Chosen option: "Generate only on first boot or through an explicit rotation".
Silent regeneration is prohibited.

A key minted behind the running signing provider cannot sign anything — the
provider does not hold it — and no peer has ever seen it. Both local event
composition and outbound federation would fail at once, and the server would
look healthy while doing so.

### Positive Consequences

* A lapsed `valid_until_ts` window is **republished, never replaced**.
  `valid_until_ts` tells peers when to re-fetch; it is not a lease after which
  the server may adopt a different identity.
* Selecting the greatest `valid_until_ts` also picks the correct key after a
  rotation, because the retired key is stamped with "now" while the new key
  carries a full window.

### Negative Consequences

* A server whose stored key is genuinely unusable fails rather than
  self-healing. That is intended, but it means key loss is an operator
  incident, not an automatic recovery.
* The legacy `ed25519:auto` sentinel must stay permanently excluded from
  selection: notary servers cached it with a far-future `valid_until_ts` and
  will never re-fetch, so it can only be retired by minting a key id they have
  not cached.

## Links

* [`docs/crypto-boundary.md`](../crypto-boundary.md)
* Implemented in `src/homeserver/room_service.cpp`,
  `ensure_runtime_server_signing_key` / `find_active_server_signing_key`

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
