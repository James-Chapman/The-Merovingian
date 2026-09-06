# Populate the full device list on an initial sync

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

On an initial `/sync` — no `since`, or a sliding sync with no `pos` — there is
no baseline to diff device lists against. The specification permits either
behaviour: "the server need only populate this property for an incremental
`/sync`".

## Considered Options

* Report the full set of the user's devices
* Report nothing, as the spec permits

## Decision Outcome

Chosen option: "Report the full set", so a freshly logged-in device is prompted
to `/keys/query` its own user's devices straight away rather than waiting for
the first incremental sync to notice them.

Deduplication is what keeps the initial response bounded.

### Positive Consequences

* `tests/unit/test_sync_handler.cpp` pins the behaviour, so it cannot silently
  regress to the empty-set reading during a refactor.

### Negative Consequences

* A larger initial sync payload for users with many devices, on the one request
  that is already the largest.

## Links

* [`src/sync/AGENTS.md`](../../src/sync/AGENTS.md)

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
