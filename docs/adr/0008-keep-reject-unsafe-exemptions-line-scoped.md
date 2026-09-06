# Keep reject-unsafe exemptions line-scoped

* Status: accepted
* Date: 2026-09-06

Technical Story: [PR #490](https://github.com/OMG-Software/Merovingian/pull/490),
review round

## Context and Problem Statement

`scripts/reject-unsafe.sh` is a pre-commit gate that rejects a manual
`unlock()` unless the line carries a `// LOCK_RELEASE: reviewed — <reason>`
annotation. The primitives implementing the release scope
(`RuntimeMutex::unlock`, `RuntimeLockRelease::release`) cannot be written in
terms of themselves, so their own unlock calls trip it.

How should the gate be satisfied for code that *is* the RAII mechanism?

## Decision Drivers

* [`scripts/AGENTS.md`](../../scripts/AGENTS.md) is explicit: *"If it blocks
  you, fix the code — do not add an exception to the script."*
* These two files are the most security-sensitive place in the tree for an
  unreviewed lock release to appear.

## Considered Options

* Annotate each unlock in the primitives, per line, like any other site
* Exempt the two implementing files by name in the gate

## Decision Outcome

Chosen option: "Annotate each unlock in the primitives, per line", because a
file-level exemption would let *any* future unannotated release in either file
through — including one that has nothing to do with implementing the scope.

The gate's pattern was also widened to match `->unlock()`, not only
`.unlock()`; a release through a pointer previously passed unseen.

### Positive Consequences

* An unrelated manual release added to `request_lock.cpp` or
  `runtime_mutex.cpp` still trips the gate.
* Widening the pattern found no existing violations, so the tree was already
  clean by the stricter rule.

### Negative Consequences

* The primitives carry annotations whose reason is essentially "this is the
  mechanism", which reads as boilerplate — but boilerplate that a reviewer must
  actively copy in order to add a new release.

## Pros and Cons of the Options

### Annotate per line

* Good, because the exemption is exactly as wide as the thing it exempts.
* Good, because it obeys the scoped repository rule rather than amending the
  script.
* Bad, because it must be repeated if the implementation grows another unlock.

### Exempt the two files by name

* Good, because it is one line in the gate and needs no upkeep.
* Bad, because it silently exempts code not yet written, in the two files where
  that matters most.
* Bad, because `scripts/AGENTS.md` forbids adding exceptions to this script,
  and a file-level filter is exactly that.

## Links

* Rule in [`scripts/AGENTS.md`](../../scripts/AGENTS.md)
* Rule in [`docs/security-coding-rules.md`](../security-coding-rules.md)
