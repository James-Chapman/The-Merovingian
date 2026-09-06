# Record architecture decisions

* Status: accepted
* Date: 2026-09-06

## Context

Decisions get made in this project that the code depends on but does not state
locally: an invariant that holds only because nothing else in the tree violates
it, an alternative that was rejected for a reason that is not visible from the
result, a behaviour change whose blast radius was checked once. `CHANGELOG.md`
records *what* changed and the module `AGENTS.md` files record *the rule*, but
neither records *why this and not that*, so the reasoning survives only in a
pull request thread that nobody will find again.

Issue #487 is the case in point. The same locking defect reached production
three times across 0.12.1, 0.12.3 and 0.12.6, in three shapes, found three
different ways — twice because the previous fix's reasoning was not written
down anywhere a later author would read it.

## Decision

We will use Architecture Decision Records, as
[described by Michael Nygard](http://thinkrelevance.com/blog/2011/11/15/documenting-architecture-decisions),
held in `docs/adr/` and listed in [index.md](index.md).

A decision earns an ADR when a future reader would otherwise be likely to undo
it by accident. Implementation notes belong in `CHANGELOG.md`, investigation
write-ups in commit messages, and settled rules in `coding-rules.md`,
`security-coding-rules.md`, or the relevant module `AGENTS.md` — an ADR links
to those rather than repeating them.

This register starts at 0.12.6. Earlier decisions are not backfilled, so its
silence about an older choice means nothing.

## Consequences

See Michael Nygard's article, linked above.

`docs/AGENTS.md` otherwise forbids creating new documents. Adding an ADR is the
one sanctioned exception, because the format's value comes from one immutable
file per decision rather than an ever-edited page.
