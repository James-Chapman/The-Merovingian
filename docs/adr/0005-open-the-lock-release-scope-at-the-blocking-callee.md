# Open the lock release scope at the blocking callee

* Status: accepted
* Date: 2026-09-06

Technical Story: [#487](https://github.com/OMG-Software/Merovingian/issues/487)
· [PR #490](https://github.com/OMG-Software/Merovingian/pull/490)

## Context and Problem Statement

The 0.12.1 and 0.12.3 fixes released the *caller's* guard before delegating to
a self-locking service function. With
[ADR-0003](0003-drain-every-recursion-level-in-one-release-primitive.md) in
place that is no longer required, so the placement is now a free choice. Where
should a `RuntimeLockRelease` scope be opened — around the call, or inside the
function that makes it?

## Considered Options

* At the callee: the function performing the blocking call opens its own scope
* At the caller: each call site releases before delegating

## Decision Outcome

Chosen option: "At the callee", because the callee knows it is about to do
network I/O and the caller may not. The two placements answer different
questions, and only the callee's is answerable locally.

The rule is stated in
[`src/homeserver/AGENTS.md`](../../src/homeserver/AGENTS.md) so it is read
before the code is written, not after.

### Positive Consequences

* Reviewing a new outbound call means looking at one function rather than at
  every caller of it.
* A new caller of an existing self-locking function inherits correct behaviour
  without knowing this rule exists.

### Negative Consequences

* Existing caller-side releases (`create_room` and `join_room` in
  `local_http_router.cpp`, `create_room` in `client_server.cpp`) are now
  redundant. They are kept — they are correct and explicit — but a reader may
  reasonably wonder why they are there, which is what this record answers.
* The lock is released slightly later, for the duration of the callee's own
  validation work. That work is in-memory and bounded; the network call it
  precedes is not.

## Pros and Cons of the Options

### At the callee

* Good, because the knowledge and the code are in the same place.
* Good, because it does not scale with the number of callers.
* Bad, because a function that *sometimes* blocks releases the lock on paths
  where it does not need to.

### At the caller

* Good, because the released region is visible at the call site.
* Bad, because it is the arrangement that produced the 0.12.3 defect: the
  reviewer has to notice that a given callee blocks, for every call site
  separately.

## Links

* Enabled by [ADR-0003](0003-drain-every-recursion-level-in-one-release-primitive.md)
* Rule recorded in [`src/homeserver/AGENTS.md`](../../src/homeserver/AGENTS.md),
  "The runtime lock and blocking calls"
