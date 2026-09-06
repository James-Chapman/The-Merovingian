# Spawn the federation worker with a minimal environment

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

The federation worker is spawned by the main process, which may hold secrets in
its environment. A spawned child inherits the parent environment by default.

## Considered Options

* Spawn with an explicit minimal environment (`PATH` only)
* Inherit the parent environment

## Decision Outcome

Chosen option: "Spawn with `PATH` only", so that no parent secret leaked via
the environment reaches the lower-privilege worker.

This is the same reasoning as
[ADR-0015](0015-keep-the-signing-secret-out-of-the-federation-worker.md)
applied to a different channel: the worker's value as an isolation boundary
depends on secrets not crossing it by any route, including ones nobody chose
deliberately.

### Negative Consequences

* Anything the worker legitimately needs from the environment must be passed
  explicitly, and a missing variable shows up as a worker failure rather than a
  silently inherited default.

## Links

* [`docs/hardening.md`](../hardening.md)

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
