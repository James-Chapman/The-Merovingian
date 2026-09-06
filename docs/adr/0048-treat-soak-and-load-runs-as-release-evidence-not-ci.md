# Treat soak and load runs as release evidence not CI

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

The release charter conflated "we must know this" with "CI must prove it on
every pull request". Soak, load and chaos runs cannot produce meaningful
numbers on shared, time-limited runners: a real measurement needs tens of
seconds to minutes of wall clock, and throughput figures on shared hardware are
noisy and not reproducible. Blocking 1.0.0 on CI-proven soak coverage blocked
it permanently.

## Considered Options

* Maintainer-run release evidence, recorded at tag time
* Keep tightening CI thresholds until the runs are reliable

## Decision Outcome

Chosen option: "Maintainer-run release evidence, recorded at tag time" beside
compiler flags and checksums. Numbers from real hardware are *stronger*
evidence than a CI figure, not weaker.

The harness (`tests/integration/test_runtime_lock_soak_flow.cpp`) is gated
behind the `build_load_tests` Meson option, matching the existing
`build_live_tests` precedent. Its short default run is CI-safe as a correctness
check — nothing deadlocks or starves — while real measurement is a manual,
dedicated-host activity.

### Negative Consequences

* **A performance regression will not be caught by CI.** It is caught at
  release time or not at all, which is a real gap accepted knowingly.
* Descoped items are recorded under "Decided against" rather than deleted, so a
  later audit does not silently reintroduce them — the register and that list
  serve the same purpose.

## Links

* [`docs/http-transport.md`](../http-transport.md), "Load/soak evidence"
* `CHANGELOG.md`, 0.12.2

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
