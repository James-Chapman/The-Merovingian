# Ship a static musl tarball as the portability fallback

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

glibc is backward- but not forward-compatible, so a distro package built for
one Linux release cannot run on an older one. Older or minimal distros can
neither build nor run a glibc-dynamic build. A single dynamic build that runs
on both new and old Linux is not possible.

## Considered Options

* Native distro packages, plus a statically linked musl tarball as an escape
  hatch
* Native packages only
* A single portable dynamic build

## Decision Outcome

Chosen option: "Native packages, plus a static musl tarball". The static binary
has no glibc dependency, so it runs where a native package cannot be built.
**Prefer a native Tier 1/Tier 2 package whenever the platform is new enough;
use the static tarball only when it is not.**

### Negative Consequences

* The static binary is much larger.
* **It carries no automatic security updates for its bundled libraries.** A fix
  in a bundled dependency requires rebuilding and redeploying the tarball —
  the operator inherits that responsibility from their distro.
* It omits the sandboxed thumbnail worker when static image codecs are
  unavailable, so thumbnails fall back to original bytes — a weaker posture
  than [ADR-0024](0024-decode-untrusted-images-in-a-sandboxed-child-process.md)
  assumes.

## Links

* [`docs/platform-support.md`](../platform-support.md)

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
