# Return not found rather than the original on thumbnail failure

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

A thumbnail request can hit a missing worker, an unsupported content type (only
PNG and JPEG are resampled), or a decode failure. The server has the original
media bytes in hand and could return them.

## Considered Options

* Return `404`
* Fall back to serving the original full-size media bytes

## Decision Outcome

Chosen option: "Return `404`", so that **a thumbnail request cannot be used to
download arbitrary full-size media**. The fallback is not merely wasteful; it
is a way to bypass whatever size expectations a client or operator placed on
the thumbnail route.

### Negative Consequences

* A client asking for a thumbnail of a GIF or SVG gets nothing rather than
  something displayable, and must request the original explicitly.

## Links

* [`docs/media-repository.md`](../media-repository.md), "Thumbnailing"

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
