# Drop push deliveries at the concurrency cap

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

Background push-delivery futures were parked in `orphan_futures_` and never
reaped, so enabling push delivery caused unbounded memory growth and unbounded
OS thread creation under sustained message volume.

## Considered Options

* Drop the delivery at a fixed cap, logging it
* Block the request until capacity frees up
* Keep spawning

## Decision Outcome

Chosen option: "Drop the delivery at a fixed cap, logging it". Completed
futures are reaped before a new one is parked, and
`k_max_in_flight_push_deliveries` (128) is checked before spawning.

**A missed push is recoverable; an exhausted thread pool is not.** The client
still sees the event on its next `/sync`.

### Negative Consequences

* Under sustained load, push notifications are silently best-effort. The log
  line is the only signal that any were dropped.
* Blocking would have applied backpressure to the producer; dropping does not,
  so the underlying overload is not slowed by this mechanism.

## Links

* `CHANGELOG.md`, 0.11.11
* [ADR-0010](0010-run-fire-and-forget-work-as-parked-futures.md)

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
