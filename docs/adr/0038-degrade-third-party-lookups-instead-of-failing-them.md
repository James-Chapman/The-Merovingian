# Degrade third party lookups instead of failing them

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

The bare `location` and `user` third-party lookup routes fan out to every
registered application service. Any one of them may time out, refuse the
connection, or return a malformed body.

## Considered Options

* Skip the failing appservice and aggregate what the others returned
* Turn any single failure into an error for the whole client request

## Decision Outcome

Chosen option: "Skip the failing appservice and aggregate the rest".
Aggregation degrades, never fails. Only when no appservice contributes anything
does the route return 404.

A bridge being down must never turn a lookup into a 502.

### Negative Consequences

* A client cannot distinguish "this identifier does not exist" from "the bridge
  that knows about it was unreachable". Both are a 404.

## Links

* [`src/appservice/AGENTS.md`](../../src/appservice/AGENTS.md)

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
