# Deny when a rate limit policy cannot be resolved

* Status: accepted
* Date: 2026-09-06

Technical Story: issue #412

## Context and Problem Statement

A per-IP rate-limit policy can fail validation at resolution time — a
configured entry, a tier override, or the default failing
`rate_limit_policy_is_valid()`, for instance `window_seconds > 3600`. The
engine must decide what to do with the request in front of it.

## Considered Options

* Deny the request
* Allow the request through, falling back to whatever other policy resolves

## Decision Outcome

Chosen option: "Deny the request" — **even when a valid per-user policy
exists**, because the per-IP bucket is the only defence on unauthenticated
routes. A misconfigured policy must never silently disable rate limiting.

### Negative Consequences

* A configuration typo takes the affected routes offline rather than degrading
  them. That is the intended direction of failure, but it means rate-limit
  configuration is start-up-critical and should be validated before deployment.

## Links

* [`docs/http-transport.md`](../http-transport.md), "Fail-closed on an
  unresolvable policy"

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
