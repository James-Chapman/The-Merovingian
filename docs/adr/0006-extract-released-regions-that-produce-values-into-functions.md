# Extract released regions that produce values into functions

* Status: accepted
* Date: 2026-09-06

Technical Story: [#487](https://github.com/OMG-Software/Merovingian/issues/487)
· [PR #490](https://github.com/OMG-Software/Merovingian/pull/490)

## Context and Problem Statement

`join_room`'s released region spanned roughly 350 lines, and nine of the
forty-four values declared inside it were consumed after the re-lock
(`verified_critical_state`, `verified_auth_chain`, `signed_event`,
`event_id_result` among them).

An RAII scope is a *block*, so anything it produces that outlives it must be
declared before it. How should a released region that produces values be
shaped?

## Decision Drivers

* [#487](https://github.com/OMG-Software/Merovingian/issues/487) named this as
  the blocker that had kept `join_room` on a hand-written
  `unlock()`/`lock()` pair while eleven other sites were converted.
* The alternative had a real cost in const-correctness, in a function long
  enough that losing it matters.

## Considered Options

* Extract the region into a function returning a result struct
* Hoist the nine declarations above the scope and assign inside it
* Leave the hand-written `unlock()`/`lock()` pair in place

## Decision Outcome

Chosen option: "Extract the region into a function returning a result struct",
because the scope boundary then *is* the function boundary — nothing needs
hoisting, every declaration keeps its `const`, and the release itself becomes a
two-line scope around the call.

`perform_federated_join` returns a `FederatedJoinOutcome` carrying the nine
values plus an optional `failure` the caller returns verbatim. At smaller
scale, an immediately-invoked lambda returning a small result struct does the
same job — used by `invite_user_by_threepid` and the three
`local_http_router.cpp` sites.

### Positive Consequences

* The compiler enforces the shape of the released region: a value that must
  survive the re-lock has to be returned, and cannot be silently read from a
  stale local.
* The result struct holds the room-version policy **by value**, not as the raw
  pointer `find_room_version_policy` returns. A pointer would have made the
  caller's dereference depend on a separate `failure` field being disengaged,
  and the project forbids raw pointers ([`AGENTS.md`](../../AGENTS.md)).

### Negative Consequences

* **Values crossing the boundary must be owned, and a parameter type change can
  break that silently.** Extraction turned `key_id` into a `std::string_view`
  parameter, and the existing `auto key_id_copy = key_id;` then deduced a
  *view* rather than the owning `std::string` the surrounding comment promised.
  Make_join tasks that lose the race are parked in `runtime.orphan_futures_`
  and outlive `join_room`'s frame, so this was a live use-after-free. Caught in
  review. **Re-check every `auto` copy when a parameter changes from owning to
  view.**
* Moving ~350 lines re-indents them, so patch-coverage tools count the whole
  region as new. The uncovered lines are its failure branches, which were
  uncovered before the move too.

## Pros and Cons of the Options

### Extract into a function returning a result struct

* Good, because the lock boundary becomes a boundary the compiler checks.
* Good, because the extracted function is independently readable and testable.
* Bad, because every value the caller needs must be named twice — once in the
  struct, once at the call site.

### Hoist the nine declarations above the scope

* Bad, because it strips `const` from nine initialisations, requires each type
  to be default-constructible, and splits nine initialisations into
  declare-then-assign, inside a 1000-line function.
* Bad, because it buys a real loss of const-correctness for no behaviour
  change.

### Leave the hand-written pair

* Bad, because it was the last one, and keeping it would have kept the
  `reject-unsafe.sh` gate exempting it — see
  [ADR-0008](0008-keep-reject-unsafe-exemptions-line-scoped.md).
* Bad, because the pair has no exception safety: a throw between the release
  and the re-lock leaves the guard down.

## Links

* Context in [`docs/http-transport.md`](../http-transport.md), "`join_room`,
  and why the released region became a function"
