# Key the pre-auth key resolution budget on source address

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

Verifying an inbound federation signature requires resolving the sending
server's key first. An unauthenticated sender can name **any** origin in an
X-Matrix header, forcing this server to perform discovery and an outbound key
fetch against a host of the attacker's choosing — before any authentication has
happened.

## Considered Options

* Budget the work, keyed on **source IP**, rejecting over the cap
* Budget it keyed on the claimed origin
* Queue excess in-flight resolutions rather than rejecting

## Decision Outcome

Chosen option: "Budget keyed on source IP, rejecting over the cap".
`key_resolution_per_ip_rate` and `key_resolution_max_in_flight` (8) bound the
work, with a negative-cache TTL for failures.

**Keyed on source IP because the origin field is attacker-chosen** — a budget
keyed on origin is a budget the attacker allocates for themselves. Rejecting
rather than queuing, because a queue is just a slower way to accumulate the
same unbounded work.

### Negative Consequences

* A legitimate new federation partner behind a busy shared address can be
  throttled and intermittently fail to join. A throttled resolution therefore
  logs a warning naming the origin and the budget, because that symptom is
  otherwise very hard to diagnose.

## Links

* `CHANGELOG.md`, 0.12.4

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
