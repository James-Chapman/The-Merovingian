# Request FORTIFY_SOURCE only on optimised builds

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

`_FORTIFY_SOURCE=3` is wanted for hardening. But this project builds with
`werror=true`, and on glibc platforms FORTIFY without optimisation is itself a
compiler warning — so requesting it unconditionally breaks the default debug
build.

## Considered Options

* Request `_FORTIFY_SOURCE=3` only when Meson reports an optimised build
* Request it always, and stop treating warnings as errors
* Request it always, and suppress that one warning

## Decision Outcome

Chosen option: "Only on optimised builds", keeping the default debug profile
warning-clean without weakening the warnings-as-errors policy, which is a
broader safety net than this one hardening flag.

### Negative Consequences

* **The debug profile gets no FORTIFY_SOURCE hardening.** Developers and
  debug-profile CI jobs run without a mitigation that production has, so a bug
  FORTIFY would catch can survive local testing.

## Links

* [`docs/build-warning-policy.md`](../build-warning-policy.md)

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
