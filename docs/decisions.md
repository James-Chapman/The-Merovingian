# Decision Register

Design decisions with lasting consequences, and the alternatives that were
rejected. One entry per decision, newest last, numbered `D<NNN>` and never
renumbered.

## What belongs here

A decision earns an entry when **a future reader would otherwise be likely to
undo it by accident**. That usually means one of:

- a constraint the code depends on but does not state locally (an invariant
  that holds only because nothing else in the tree violates it)
- a rejected alternative that looks obviously better in isolation
- a deliberate behaviour change whose blast radius was checked once and should
  not have to be rechecked from scratch
- a rule about how to write future code, not just what the current code does

What does not belong here: implementation notes (`CHANGELOG.md`), investigation
write-ups (commit messages), or restatements of a rule that already lives in
`coding-rules.md`, `security-coding-rules.md`, or a module `AGENTS.md`. Where a
decision produced such a rule, the entry links to it rather than repeating it.

## How to use it

- Add the entry in the same branch as the change it describes.
- Status is `Accepted`, `Superseded by D<NNN>`, or `Reversed in <version>`.
  Never delete an entry — a decision that was reversed is exactly the thing a
  future reader needs to find.
- Record the alternatives seriously. An entry that lists no rejected option is
  usually documenting a fact, not a decision.

**This register starts at 0.12.6.** Decisions made before that are not
backfilled, so its silence about an older choice means nothing.

---

## D001 — `HomeserverRuntime::mutex` is a `RuntimeMutex`, not a `std::recursive_mutex`

**Date:** 2026-09-06 · **Version:** 0.12.6 · **Status:** Accepted ·
**Issue:** #487 · **PR:** #490

### Context

`runtime.mutex` is recursive so that self-locking service functions
(`create_room`, `join_room`, `leave_room`, `invite_user_by_threepid`) stay
independently callable. A release scope around a blocking network call must
therefore drop *every* level the thread holds; dropping one leaves the mutex
locked for the whole of the "released" call and stalls the server. That defect
shipped three times (0.12.1, 0.12.3, 0.12.6).

`std::recursive_mutex` cannot report whether the calling thread owns it, so a
release scope had no way to know whether it had actually released anything.

### Decision

Wrap `std::recursive_mutex` in `homeserver::RuntimeMutex`
([`runtime_mutex.hpp`](../include/merovingian/homeserver/runtime_mutex.hpp)),
which records its owning thread and recursion depth and exposes
`held_by_current_thread()`.

### Alternatives rejected

- **Keep `std::recursive_mutex` and audit each call site.** Two audits had
  already missed instances; the second missed one that had been documented as
  fixed. The pattern was reaching production faster than review was catching it.
- **A debug assertion that fails loudly when a release leaves the mutex held**
  (suggested in #487). Detects the defect; does not prevent it, and only on a
  path a test happens to exercise.
- **A `thread_local` recursion counter maintained by the release primitive.**
  The primitive does not see acquisitions taken anywhere else, so the count
  would be wrong exactly when it mattered.

### Consequences

- 37 explicit `std::unique_lock<std::recursive_mutex>` spellings became
  `std::unique_lock<RuntimeMutex>`. Sites using CTAD were unaffected.
- `FederationRuntimeState::mutex` is a *different* mutex and was deliberately
  left as `std::recursive_mutex`; it has no release-scope problem.
- Every acquisition pays one atomic store. Measured cost was not separable from
  noise in the existing lock soak harness.

---

## D002 — One release primitive, and it drains every recursion level

**Date:** 2026-09-06 · **Version:** 0.12.6 · **Status:** Accepted ·
**Issue:** #487 · **PR:** #490

### Context

Two primitives existed. `NetworkIoUnlock` acted on the thread's *published*
guard (`RequestLockScope`); `ScopedGuardRelease` acted on a guard *in hand*. At
a nested call site those are different objects, and picking the wrong one
produced code that compiled, passed its tests, and held a global mutex across a
network round trip. That distinction is precisely what made the 0.12.3
`leave_room` fix incomplete.

### Decision

Collapse both into `homeserver::RuntimeLockRelease`
([`request_lock.hpp`](../include/merovingian/homeserver/request_lock.hpp)). Its
two constructors say only which `unique_lock` to read the mutex address from.
Both release **every** recursion level the calling thread holds and restore
exactly that many on scope exit, throwing paths included.

### Alternatives rejected

- **Keep both and document the difference.** The difference was already
  documented when `leave_room` was got wrong.
- **Fix the third instance (`invite_user_by_threepid`) at its call site**, as
  the first two were fixed. That leaves correctness depending on every future
  caller of every self-locking function remembering to release first.

### Consequences

- Caller-side releases before delegating to a self-locking function are now
  redundant. The existing ones are kept because they are explicit, not because
  correctness depends on them.
- **Behaviour change:** a caller holding the mutex across a broader critical
  section now has it dropped by a callee's release scope. The only site where
  that could matter is `ingest_pdu_event`'s backend commit; both of its callers
  (the IPC handler-pool task in `worker_pool.cpp`, and
  `handle_federation_http_request`) reach it holding no level, so it is a no-op
  there. **A new caller of `ingest_pdu_event` that holds `runtime.mutex` would
  reopen this question.**
- Gap, stated so it is not mistaken for coverage: the default-constructed
  `RuntimeLockRelease{}` does nothing when a caller holds the mutex without
  publishing a `RequestLockScope`. Pass the guard in that case.

---

## D003 — A release scope goes through the mutex and never through a guard

**Date:** 2026-09-06 · **Version:** 0.12.6 · **Status:** Accepted ·
**PR:** #490 (review round)

### Context

The first implementation of D002 released the *named* guard through
`unique_lock::unlock()` and drained the remaining levels through the mutex.
Draining cannot clear another `unique_lock`'s ownership flag, so the flag and
the real recursion depth then disagreed in opposite directions across two
objects. An inner release scope reaching for the thread's published guard saw a
stale `owns_lock() == true`, unlocked a mutex the thread no longer held,
underflowed the depth, and would have stranded `runtime.mutex` locked for the
life of the process.

Found in review on #490, and confirmed by restoring the old shape and running
the new nested-scope scenario against it: it fails with `Operation not
permitted` — the recursive mutex refusing an unlock the thread does not own.

### Decision

The scope drops every level through `RuntimeMutex::unlock()` and never consults
any guard's `owns_lock()`. The constructor argument is read only for the mutex
address.

### Alternatives rejected

- **Track which guards have been drained** so an inner scope can recognise
  them. Requires a registry of live `unique_lock` objects per thread; more
  machinery than the problem needs.
- **Have the inner scope check `held_by_current_thread()` before trusting
  `owns_lock()`.** Fixes the reported case but not the one where inner code
  takes a fresh lock inside a released region: the stale flag and the fresh
  level then belong to different objects and the depth mismatches on unwind.

### Consequences

**Standing constraint — read before touching this code.** For the lifetime of a
release scope, *every* `std::unique_lock` on `runtime.mutex` reports
`owns_lock() == true` while the mutex is actually free, including the one
passed to the scope. This is sound only because:

1. nothing outside `src/homeserver/request_lock.cpp` reads that flag
   (`grep -rn owns_lock src include`), and
2. the depth is restored exactly before any of those guards can act on it.

**Introducing a read of `owns_lock()` on `runtime.mutex` anywhere else breaks
this.** Read `RuntimeMutex::held_by_current_thread()` instead. A nested release
scope does exactly that, which is why it correctly finds nothing to release.

---

## D004 — A blocking call opens its own release scope, at the callee

**Date:** 2026-09-06 · **Version:** 0.12.6 · **Status:** Accepted ·
**Issue:** #487 · **PR:** #490

### Context

The 0.12.1 and 0.12.3 fixes released the *caller's* guard before delegating to
a self-locking service function. With D002 in place that is no longer required,
and the two placements answer different questions: the callee knows it is about
to do network I/O, the caller may not.

### Decision

The function that performs the blocking call opens the release scope. Callers
do not release on a callee's behalf.

### Consequences

- Reviewing a new outbound call means looking at one function, not at every
  caller of it.
- The rule is stated in
  [`src/homeserver/AGENTS.md`](../src/homeserver/AGENTS.md) so it is read
  before the code is written, not after.

---

## D005 — A released region that produces values becomes a function

**Date:** 2026-09-06 · **Version:** 0.12.6 · **Status:** Accepted ·
**Issue:** #487 · **PR:** #490

### Context

`join_room`'s released region spanned ~350 lines and nine of the forty-four
values declared inside it were consumed after the re-lock. Wrapping it in a
scope mechanically would have meant hoisting those nine above the scope:
`const` stripped from each, every type forced to be default-constructible, and
nine initialisations split into declare-then-assign inside a 1000-line
function.

### Decision

Extract the region into a function returning a result struct
(`perform_federated_join` → `FederatedJoinOutcome`), so the lock boundary is a
function boundary. At smaller scale, an immediately-invoked lambda returning a
small result struct does the same job — used by `invite_user_by_threepid` and
the three `local_http_router.cpp` sites.

### Alternatives rejected

- **Hoist the nine declarations.** A real loss of const-correctness bought for
  no behaviour change.
- **Leave the hand-written `unlock()`/`lock()` pair.** It was the last one, and
  keeping it would have kept the gate exempting it.

### Consequences

- The result struct holds the room-version policy **by value**, not as the raw
  pointer `find_room_version_policy` returns. A pointer would have made the
  caller's dereference depend on a separate `failure` field being disengaged,
  and the project forbids raw pointers ([`AGENTS.md`](../AGENTS.md)).
- Moving ~350 lines re-indents them, so patch-coverage tools count the whole
  region as new. The uncovered lines are its failure branches, which were
  uncovered before the move.
- Values crossing the boundary must be **owned**. Extraction turned `key_id`
  into a `std::string_view` parameter and `auto key_id_copy = key_id;` silently
  deduced a view — captured by make_join tasks that are parked in
  `runtime.orphan_futures_` and outlive `join_room`'s frame. Caught in review
  on #490. Check every `auto` copy when a parameter's type changes from owning
  to view.

---

## D006 — Lock tests assert on what another thread observes

**Date:** 2026-09-06 · **Version:** 0.12.6 · **Status:** Accepted ·
**PR:** #490

### Context

A recursive mutex answers `try_lock()` with "yes" to its own owner at any
depth, and a `unique_lock`'s `owns_lock()` is bookkeeping rather than fact
(D003). Neither distinguishes "the mutex is free" from "one level of it was
released". Every one of the three shipped stalls passed the tests that existed
at the time.

### Decision

Tests for the release primitives assert on what a thread holding no level of
the mutex can observe — a probe thread attempting `try_lock()` — not on what a
guard reports. Assertions run on the main thread, because Catch2's macros are
not thread-safe.

### Consequences

- Recorded as a convention in
  [`testing-standards.md`](testing-standards.md) so it applies beyond this
  module.
- The first round of #490 still shipped a defect that single-scope tests could
  not see; the nested-scope scenario was added in review. A green suite proves
  only what it exercised.

---

## D007 — `reject-unsafe.sh` exemptions stay line-scoped

**Date:** 2026-09-06 · **Version:** 0.12.6 · **Status:** Accepted ·
**PR:** #490 (review round)

### Context

The primitives implementing the release scope cannot be written in terms of
themselves, so their own `unlock()` calls trip the manual-lock-release gate.
The first attempt exempted those two files wholesale, which would have let any
future unannotated release in either file through.
[`scripts/AGENTS.md`](../scripts/AGENTS.md) is explicit: *"If it blocks you,
fix the code — do not add an exception to the script."*

### Decision

No file-level exemptions. Each unlock in the primitives carries the same
`// LOCK_RELEASE: reviewed — <reason>` annotation any other site would need.
The gate's pattern also matches `->unlock()`, not only `.unlock()` — a release
through a pointer used to pass unseen.

### Consequences

An unrelated manual release added to `request_lock.cpp` or `runtime_mutex.cpp`
still trips the gate.
