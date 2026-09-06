# Decode untrusted images in a sandboxed child process

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

Generating thumbnails means decoding untrusted PNG and JPEG bytes. libpng and
libjpeg parsers are a historic CVE surface, and decoding is the highest-risk
operation the media repository performs.

## Considered Options

* Decode in a short-lived sandboxed child process
* Decode in the homeserver process

## Decision Outcome

Chosen option: "Decode in a short-lived sandboxed child process".
`merovingian-thumbnail-worker` reads one framed request on stdin, decodes and
resamples, and writes a framed response on stdout. It holds no secrets, no
sockets, and no filesystem access beyond the inherited stdio pipes, and is
further confined with rlimits, seccomp-bpf and `PR_SET_NO_NEW_PRIVS` before it
touches any input.

**Untrusted image bytes are never decoded in the homeserver process.**

### Negative Consequences

* A process spawn per thumbnail request.
* The worker must be installed and findable
  (`-DMEROVINGIAN_THUMBNAIL_WORKER_PATH`); a missing worker degrades
  thumbnailing — see
  [ADR-0025](0025-return-not-found-rather-than-the-original-on-thumbnail-failure.md).
* The static musl build omits the worker when static image codecs are
  unavailable, which is one of the trade-offs recorded in
  [ADR-0045](0045-ship-a-static-musl-tarball-as-the-portability-fallback.md).

## Links

* [`docs/media-repository.md`](../media-repository.md), "Thumbnailing"

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
