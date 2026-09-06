# Validate the forwarded client address

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

When the direct TCP peer is listed in `server.trusted_proxies`, the rate
limiter needs the real client address from `X-Forwarded-For` — otherwise the
entire downstream network collapses into one bucket.

## Considered Options

* Take the leftmost non-empty `X-Forwarded-For` value, but validate it as a
  real IPv4 or IPv6 literal first
* Trust the forwarded value verbatim

## Decision Outcome

Chosen option: "Validate it as an IP literal first"
(`federation::ip_address_is_valid()`), falling back to the direct peer address
when the header is missing, empty, or invalid.

A trusted proxy is trusted to forward *its own view of the client address*
correctly — not to hand the server an arbitrary string.

### Positive Consequences

* Without the check, an attacker reaching a trusted proxy could rotate
  malformed pseudo-IP values to mint a fresh rate-limit bucket per request and
  defeat per-IP limiting entirely.

## Links

* [`docs/http-transport.md`](../http-transport.md), "Trusted-proxy client IP
  resolution"

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
