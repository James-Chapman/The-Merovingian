# Base64 encode IPC response bodies

* Status: accepted
* Date: 2026-09-06

Technical Story: issue #342

## Context and Problem Statement

Federation-worker IPC frames are size-capped. A response body embedded in a
`fed_response` or `outbound_http_response` frame has to be escaped somehow, and
the escaping overhead must not push an already-near-cap response over the
frame limit.

## Considered Options

* Base64-encode the body before framing — a fixed 4/3 expansion
* Escape it as a raw JSON string

## Decision Outcome

Chosen option: "Base64, for its **fixed** expansion". JSON-string escaping
expands by an amount that depends on the content, so a body that fits the cap
before escaping may not fit after — a failure mode driven by remote data.

### Negative Consequences

* Every response body pays a 33% size cost, including the ones that would have
  escaped cheaply.
* The per-frame cap (`kIpcMaxFrameBytes`, 24 MiB, lowered from 50 MiB to bound
  memory exhaustion across concurrent workers) is derived independently by
  `WorkerPool` and by the worker from
  `security.federation.join_response_max_size`, and **both processes must
  restart to pick up a change**.

## Links

* [`docs/hardening.md`](../hardening.md)

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
