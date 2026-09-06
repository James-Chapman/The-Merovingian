# Use a longer login token window than the spec suggests

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

A login token minted at the end of the SSO redirect flow needs an expiry
window. The specification says it "SHOULD be limited to around five seconds".
Real redirect chains through an external SSO gateway routinely take longer.

## Considered Options

* A fixed ~30-second window
* The spec-suggested ~5 seconds

## Decision Outcome

Chosen option: "A fixed ~30-second window", to tolerate real-world redirect
latency. The exposure is bounded by the **single-use** guarantee rather than by
the clock: `database::consume_login_token` marks the token used in the backend
before returning the user id, so a persistence failure cannot leave it
redeemable twice from memory alone.

### Negative Consequences

* A deliberate deviation from a spec `SHOULD`. A leaked token is valid for six
  times longer than the spec contemplates, which is only acceptable because
  single-use redemption, not expiry, is the primary control.

## Links

* [`docs/auth-identity.md`](../auth-identity.md), "SSO login"

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
