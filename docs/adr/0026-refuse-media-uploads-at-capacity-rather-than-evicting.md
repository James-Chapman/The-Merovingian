# Refuse media uploads at capacity rather than evicting

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

The media repository's in-memory index had no cap on records, bytes, or
per-user usage — unbounded memory growth reachable from the network. Adding a
cap forces a choice about what happens when it is reached.

## Considered Options

* Refuse the new upload with `507`
* Evict older media to make room

## Decision Outcome

Chosen option: "Refuse the new upload with `507`". `security.media.max_records`,
`max_total_size` and `max_size_per_user` bound the repository; **nothing is
evicted**.

Clients hold `mxc://` URIs for what is stored. Eviction would break those links
rather than shed load — it converts a capacity problem into silent data loss
spread across every client that ever referenced the media.

### Negative Consequences

* A full repository stops accepting uploads until an operator intervenes.
  Capacity is an operator-facing alarm, not a self-healing condition.
* Bytes are counted over stored blobs, so a deduplicated upload consumes a
  record but no bytes — the two limits do not move together.

## Links

* `CHANGELOG.md`, 0.12.5
* [`docs/media-repository.md`](../media-repository.md)

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
