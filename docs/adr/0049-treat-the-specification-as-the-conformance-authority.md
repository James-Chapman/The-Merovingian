# Treat the specification as the conformance authority

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

When Merovingian's behaviour and another Matrix implementation's disagree,
something has to decide which is right. Synapse is the reference implementation
in practice, and copying it is the path of least resistance.

## Considered Options

* The specification is the authority
* Match Synapse's observed behaviour
* Use judgement per case

## Decision Outcome

Chosen option: "The specification is the authority. Not Synapse, not other
implementations, not intuition." An expected value in a conformance test may
only be changed by citing the spec section and version that justifies it.

### Positive Consequences

* Conformance tests are auditable: every expectation traces to a citation.
* A deliberate deviation must be argued explicitly rather than absorbed — which
  is why [ADR-0013](0013-require-a-power-level-default-when-local-state-is-absent.md)
  and [ADR-0034](0034-use-a-longer-login-token-window-than-the-spec-suggests.md)
  exist as records rather than as quiet code.

### Negative Consequences

* Where the spec is ambiguous and Synapse has settled on one reading,
  interoperating may still require matching Synapse. That case must be recorded
  as a deviation with its reasoning, not treated as conformance.

## Links

* [`tests/conformance/AGENTS.md`](../../tests/conformance/AGENTS.md)

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
