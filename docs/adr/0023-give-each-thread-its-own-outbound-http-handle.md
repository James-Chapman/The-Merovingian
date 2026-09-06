# Give each thread its own outbound HTTP handle

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

One `OutboundClient` instance is shared between the federation dispatch-worker
thread and the HTTP request-handler thread pool. A libcurl easy handle must
never be driven by more than one thread at a time.

## Considered Options

* A per-thread handle, lazily created on first use and freed at thread exit
* One shared handle guarded by a mutex
* One shared handle, unguarded (what was there before)

## Decision Outcome

Chosen option: "A per-thread handle". Every call resets the handle before
configuring it, so it is reused across calls — preserving per-thread connection
and TLS-session reuse — without leaking state between requests.

### Positive Consequences

* Fixes a real, hard-to-diagnose defect: sharing a single handle across threads
  previously caused intermittent `network_error` failures on federation key
  queries, **which broke E2EE**.

### Negative Consequences

* Connection and TLS-session reuse is per thread, so a request served by a cold
  thread pays a fresh handshake even though another thread has a warm
  connection to the same peer.

## Links

* [`docs/http-transport.md`](../http-transport.md), "Outbound HTTP client"

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
