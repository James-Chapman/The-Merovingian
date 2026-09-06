# Serialize inbound PDU ingestion on per-room stripes

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

Inbound federation PDUs were ingested while holding the global
`HomeserverRuntime::mutex` for the entire auth, persistence, membership-update
and sync-notification path. Under a burst of 30-40 events this serialized every
PDU behind the previous one's database commit — a drip-feed — even though the
SQLite and PostgreSQL backends open a fresh connection per transaction and
could otherwise overlap.

Events *within* one room must keep their order. Events in *different* rooms
need not.

## Considered Options

* Per-room stripe mutexes, with the global lock held only to reserve ordering
  tokens and to apply the committed result
* Keep holding the single global mutex across the whole ingestion path

## Decision Outcome

Chosen option: "Per-room stripe mutexes". `HomeserverRuntime` owns
`std::array<std::mutex, 256>` keyed by `std::hash<std::string>(room_id) % 256`.
The global lock reserves a `stream_ordering` token and a `sync_stream_id`, then
`ingest_pdu_event()` takes the room's stripe, prepares the write, **releases the
global lock for the backend commit**, and re-acquires it only to mirror the
committed rows into the in-memory vectors.

### Positive Consequences

* Commits for different rooms overlap; ordering within a room is preserved by
  its stripe.
* The global `runtime.mutex` is no longer held across database commits.

### Negative Consequences

* **A lock order now exists and must be respected: stripe first, then global.**
  Taking a stripe while already holding the global mutex inverts it and can
  deadlock against a concurrent `ingest_pdu_event` for the same room. This is
  why the `membership_acceptor` callback in `local_http_router.cpp` takes only
  the global mutex and deliberately never a stripe.
* Two rooms whose ids collide modulo 256 serialize against each other for no
  semantic reason.

## Links

* [`docs/architecture.md`](../architecture.md), "Per-room inbound PDU ingestion"

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
