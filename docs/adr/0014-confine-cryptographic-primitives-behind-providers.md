# Confine cryptographic primitives behind providers

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

libsodium is easy to misuse: parameter order, buffer sizing, and error handling
are all silent failure modes. Calls scattered across the tree would put that
risk everywhere, and make it impossible to substitute a deterministic
implementation in tests.

## Considered Options

* Confine every call to `src/crypto/` (plus `src/events/`, `src/auth/`, and
  `src/core/secret_buffer.cpp`) behind provider interfaces such as
  `Ed25519Provider`
* Call libsodium directly from whichever module needs it

## Decision Outcome

Chosen option: "Confine every call behind provider interfaces". Production
wires the libsodium provider; tests inject a deterministic double.

### Positive Consequences

* The boundary is **mechanically enforced**, not documented:
  `scripts/reject-unsafe.sh` rejects a direct libsodium call or a
  `<sodium.h>` include from anywhere outside the allowed directories.
* Crypto review has a fixed surface to read.

### Negative Consequences

* A module needing a new primitive must extend the provider interface rather
  than call the library, which is more work at the point of use.
* The allowed-directory list in the gate is a second place to keep in sync.

## Links

* [`src/crypto/AGENTS.md`](../../src/crypto/AGENTS.md)
* [`docs/crypto-boundary.md`](../crypto-boundary.md)

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
