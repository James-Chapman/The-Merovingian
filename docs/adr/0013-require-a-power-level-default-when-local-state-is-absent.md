# Require a power level default when local state is absent

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

Matrix authorization rule 9.4 says an `m.room.power_levels` event is allowed
outright when the room has no previous one.

This server builds its `AuthEventMap` from **local resolved state**, not from
the event's declared `auth_events`. So "no previous power_levels" does not
reliably mean the room has none — it can equally mean this server has not
synced that state yet. Under the spec reading, that is a fail-open.

## Considered Options

* Follow rule 9.4 literally and allow outright
* Still require the default `state_default` (50) in that case, deviating from
  the spec

## Decision Outcome

Chosen option: "Still require the default `state_default` (50)", accepting a
deliberate deviation from the specification, because a blanket allow keyed on
absent *local* state is a fail-open in a place where the cost of being wrong is
room takeover.

### Positive Consequences

* **The deviation can only reject where the spec would allow, never the
  reverse.** It cannot accept an event the spec forbids, so it cannot make this
  server more permissive than a conforming one.
* Room creation is unaffected: the creator resolves to power 100, or to
  infinite under room v12.

### Negative Consequences

* A conformance test written strictly against rule 9.4 will fail against this
  server, and should be read as documenting the deviation rather than a bug.
* If the `AuthEventMap` is ever rebuilt from declared `auth_events` instead of
  local state, the premise disappears and this deviation should be revisited.

## Links

* [`docs/event-engine.md`](../event-engine.md), authorization rules

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
