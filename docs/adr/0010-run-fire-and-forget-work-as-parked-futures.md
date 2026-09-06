# Run fire-and-forget work as parked futures

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

Some work must start inside a request handler but must not make the client wait
on it: `join_room`'s background member-fill after a fast join, and push
notification delivery reached from `send_event()` and every membership-mutating
path.

## Considered Options

* `std::async(std::launch::async, ...)`, with the future parked in
  `HomeserverRuntime::orphan_futures_`
* A dedicated thread pool for background work

## Decision Outcome

Chosen option: "`std::async` with the future parked in `orphan_futures_`". The
handler snapshots everything it needs while still holding `runtime.mutex`, then
launches the task; the future is parked so its lifetime is owned by the
runtime rather than by the request that started it.

### Positive Consequences

* No second scheduling mechanism to configure, size, or starve.
* Both call sites use the same pattern, so there is one place to reason about
  background-work lifetime.

### Negative Consequences

* **`~HomeserverRuntime` must drain every parked future before destroying any
  other member.** A task holds references into the runtime; tearing down a
  member while one is in flight is a use-after-free. This ordering constraint is
  invisible from the destructor's member list.
* Unbounded growth is prevented only by policy, not by the mechanism:
  `reap_completed_futures()` prunes completed entries, and push delivery caps
  concurrent in-flight tasks at 128, **dropping and logging** past the cap
  rather than queueing.
* Every task is an OS thread. A dedicated pool would bound that; this does not.

## Links

* [`docs/architecture.md`](../architecture.md), "Fire-and-forget background work"

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
