# Release the runtime lock through the mutex, not through a guard

* Status: accepted
* Date: 2026-09-06

Technical Story: [PR #490](https://github.com/OMG-Software/Merovingian/pull/490),
review round

## Context and Problem Statement

The first implementation of
[ADR-0003](0003-drain-every-recursion-level-in-one-release-primitive.md)
released the *named* guard through `std::unique_lock::unlock()` and drained the
remaining levels through the mutex.

Draining cannot clear another `std::unique_lock`'s ownership flag, so the flag
and the real recursion depth then disagreed in opposite directions across two
objects. An inner release scope reaching for the thread's published guard saw a
stale `owns_lock() == true`, unlocked a mutex the thread no longer held,
underflowed the depth, and would have stranded `runtime.mutex` locked for the
life of the process.

Found in review, and confirmed rather than assumed: restoring the old shape and
running the new nested-scope scenario against it fails with `Operation not
permitted` — the recursive mutex refusing an unlock the thread does not own.

## Decision Drivers

* A release primitive that can itself strand the global lock is worse than the
  hand-written pairs it replaced.
* Whatever is chosen must hold for a scope nested inside another scope, and for
  a fresh lock taken *inside* a released region.

## Considered Options

* Release every level through the mutex and never consult a guard's
  `owns_lock()`
* Track which guards have been drained, so an inner scope can recognise them
* Have the inner scope check `held_by_current_thread()` before trusting
  `owns_lock()`

## Decision Outcome

Chosen option: "Release every level through the mutex and never consult a
guard's `owns_lock()`", because it makes the inconsistency uniform instead of
split across two objects, and uniform inconsistency is something a nested scope
can reason about.

The constructor argument is read only for the mutex address
(`std::unique_lock::mutex()`, which answers even when the lock is not held).

### Positive Consequences

* A nested scope reads `held_by_current_thread()`, correctly finds nothing left
  to release, and does nothing.
* A fresh lock taken inside a released region is released and restored
  correctly by a scope opened around it, because the mutex is the single source
  of truth.
* The implementation got shorter.

### Negative Consequences

**This is a standing constraint. Read it before touching this code.**

For the lifetime of a release scope, *every* `std::unique_lock` on
`runtime.mutex` reports `owns_lock() == true` while the mutex is actually free
— including the one passed to the scope. This is sound only because:

1. nothing outside `src/homeserver/request_lock.cpp` reads that flag
   (`grep -rn owns_lock src include`), and
2. the depth is restored exactly before any of those guards can act on it.

**Introducing a read of `owns_lock()` on `runtime.mutex` anywhere else breaks
this, and no compiler will say so.** Read
`RuntimeMutex::held_by_current_thread()` instead.

## Pros and Cons of the Options

### Release every level through the mutex, never through a guard

* Good, because there is one source of truth for whether the mutex is held.
* Good, because it composes with nesting without any bookkeeping.
* Bad, because it leaves `owns_lock()` lying for the duration — an invariant
  that is invisible at the call site and enforced only by this record and the
  header comment.

### Track which guards have been drained

* Good, because `owns_lock()` would stay honest.
* Bad, because it requires a per-thread registry of live `std::unique_lock`
  objects — far more machinery than the problem needs, in the one place in the
  codebase where machinery is most expensive to get wrong.

### Have the inner scope check `held_by_current_thread()` before trusting `owns_lock()`

* Good, because it is a two-line change and fixes the reported case.
* Bad, because it does not fix the case where inner code takes a fresh lock
  inside a released region: the stale flag and the fresh level then belong to
  different objects, and the depth mismatches on unwind. Covered by the second
  `WHEN` block of the nested-scope scenario.

## Links

* Refines [ADR-0003](0003-drain-every-recursion-level-in-one-release-primitive.md)
* Motivates [ADR-0007](0007-assert-lock-behaviour-from-another-thread.md) —
  single-scope tests could not see this
* Regression: `tests/unit/test_request_lock.cpp`, "release scopes nest without
  corrupting the recursion depth"
