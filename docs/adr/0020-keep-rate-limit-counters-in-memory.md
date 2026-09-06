# Keep rate limit counters in memory

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

Rate-limit counters are updated on every request — the hottest path in the
server. They could be durable, surviving restarts, or live only in process
memory.

## Considered Options

* In-process counters (`m_ip_buckets` / `m_user_buckets` on `RateLimitEngine`)
* A shared durable counter table, written per request
* Front the homeserver with a proxy that enforces a durable cap

## Decision Outcome

Chosen option: "In-process counters", because rate limiting here is a
best-effort abuse throttle, not a hard correctness invariant. The latency and
contention a per-request durable write would add to the hottest path in the
server is not worth the marginal benefit.

### Negative Consequences

* **A restart, or a worker crash under a federated deployment, resets the
  counters**, so a client that was being throttled can immediately retry. This
  is an accepted operator sign-off, not an oversight.
* An operator who needs a durable cap for a specific route must front the
  homeserver with a proxy that enforces it. Merovingian will not grow one.

## Links

* [`docs/http-transport.md`](../http-transport.md), "In-memory counter trade-off"

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
