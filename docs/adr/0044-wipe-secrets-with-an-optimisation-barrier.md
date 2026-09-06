# Wipe secrets with an optimisation barrier

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

`SecretBuffer` holds signing-key material and must guarantee the memory is
actually cleared on destruction. A plain fill over memory that is never read
again is a dead store, and a compiler is entitled to remove it (CWE-14,
"Compiler Removal of Code to Clear Buffers").

## Considered Options

* `sodium_munlock`, which zeroises and unpins and is an optimisation barrier
* `std::ranges::fill` (the prior approach)

## Decision Outcome

Chosen option: "`sodium_munlock`", because it is a barrier the compiler cannot
elide, unlike the prior `std::ranges::fill` dead store.

### Negative Consequences

* `src/core` must link libsodium, which is otherwise confined by
  [ADR-0014](0014-confine-cryptographic-primitives-behind-providers.md).
  `src/core/secret_buffer.cpp` is one of the named exceptions in
  `scripts/reject-unsafe.sh` for exactly this reason.
* **Custom move-constructor and move-assignment are mandatory**: they must
  transfer the mlock and wipe the source, or the secret is duplicated or left
  pinned in a moved-from object. A defaulted move would silently break this.

## Links

* [`docs/hardening.md`](../hardening.md)

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
