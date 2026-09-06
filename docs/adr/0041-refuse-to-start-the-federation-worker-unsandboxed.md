# Refuse to start the federation worker unsandboxed

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

`apply_worker_hardening()` unconditionally reported success on every non-Linux
build, regardless of configuration. On the BSDs the federation worker therefore
handled untrusted federation traffic **completely unsandboxed while logging
that hardening was active** — the worst of both, since the log said the control
was there.

There is no in-process sandbox equivalent to seccomp on FreeBSD, OpenBSD or
NetBSD for this worker.

## Considered Options

* Return a fail-closed rejection naming the unavailable controls, so the worker
  refuses to start
* Report the true "unsandboxed" status and keep running

## Decision Outcome

Chosen option: "Fail closed". `WorkerSupervisor` sees a crash-looping child and
**federation degrades to 503 rather than ever accepting unsandboxed traffic**.

### Negative Consequences

* Federation is unavailable by default on the BSDs for this worker. An operator
  who accepts the risk can still run it by explicitly setting
  `federation.worker.apply_hardening=false` — the escape hatch is deliberate,
  and being explicit is the point.
* A crash-loop is an ugly way to express "misconfigured"; the log message
  naming the unavailable controls is what makes it diagnosable.

## Links

* [`docs/hardening.md`](../hardening.md)
* `CHANGELOG.md`, 0.12.1

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
