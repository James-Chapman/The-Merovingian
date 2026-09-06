# Apply rate limiting before authentication

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

Every client-server request passes both a rate-limit check and an auth check.
One has to come first.

## Considered Options

* Rate limit first, then authenticate
* Authenticate first, then rate limit

## Decision Outcome

Chosen option: "Rate limit first". Authenticating first would let an
unauthenticated caller drive the auth path — token lookup, hashing — as often
as it liked, exhausting server resources before any limit applied.

### Negative Consequences

* The per-IP bucket is doing the work on unauthenticated routes, since there is
  no user to key on yet. That makes the per-IP policy load-bearing rather than
  a secondary defence — see
  [ADR-0021](0021-deny-when-a-rate-limit-policy-cannot-be-resolved.md).

## Links

* [`src/http/AGENTS.md`](../../src/http/AGENTS.md), "Rules" — stated there as
  an invariant future changes must preserve

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
