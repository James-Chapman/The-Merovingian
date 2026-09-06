# Assert lock behaviour from another thread

* Status: accepted
* Date: 2026-09-06

Technical Story: [PR #490](https://github.com/OMG-Software/Merovingian/pull/490)

## Context and Problem Statement

A `std::recursive_mutex` answers `try_lock()` with "yes" to its own owner at any
depth, and a `std::unique_lock`'s `owns_lock()` is bookkeeping that a release
scope deliberately leaves stale
([ADR-0004](0004-release-the-runtime-lock-through-the-mutex-not-a-guard.md)).

Neither distinguishes "the mutex is free" from "one level of it was released" —
which is the exact distinction three separate production stalls turned on.
Every one of them passed the tests that existed when it shipped. What must a
lock test assert on to be worth anything?

## Considered Options

* Assert on what a thread holding no level of the lock can observe
* Assert on the guard's `owns_lock()` and the mutex's own `try_lock()`

## Decision Outcome

Chosen option: "Assert on what a thread holding no level of the lock can
observe", because it is the only formulation that can fail when the mutex is
still held. A probe thread attempts `try_lock()`; the result comes back as an
atomic and the assertion runs on the main thread, because Catch2's assertion
macros are not thread-safe.

Recorded as a convention in [`testing-standards.md`](../testing-standards.md)
so it applies beyond this module.

### Positive Consequences

* The scenarios read as statements about the server's actual property — "no
  level of the mutex is left held across that call" — rather than about an
  implementation detail.
* The same probe caught the nested-scope defect in review once a nested
  scenario existed to run it in.

### Negative Consequences

* Each assertion spawns and joins a thread, so these tests are slower and
  meaningfully heavier under TSan.
* The convention alone is not sufficient, which is the uncomfortable part:
  the first round of this PR used the probe throughout and *still* shipped
  the [ADR-0004](0004-release-the-runtime-lock-through-the-mutex-not-a-guard.md)
  defect, because every scenario exercised a single scope. Two further rules
  follow from that, and are recorded with it:
  * **Cover the nested case, not only the single-scope one.** A primitive that
    is correct in isolation can still be wrong one frame down.
  * **Prove a new scenario fails against the unfixed code before trusting it.**
    A regression test that passes either way documents nothing.

## Pros and Cons of the Options

### Assert on what another thread can observe

* Good, because it cannot be satisfied by a mutex that is still held.
* Bad, because it is slower, and because a reader has to understand why the
  probe thread exists.

### Assert on `owns_lock()` and `try_lock()` from the test thread

* Good, because it is simple and fast.
* Bad, because both answer "yes" to the owning thread regardless of depth, so
  the assertion passes on exactly the broken code it exists to catch.

## Links

* Motivated by [ADR-0004](0004-release-the-runtime-lock-through-the-mutex-not-a-guard.md)
* Convention recorded in [`testing-standards.md`](../testing-standards.md),
  "Concurrency and locking tests"
