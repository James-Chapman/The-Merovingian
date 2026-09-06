# Require a master key rather than storing secrets in plaintext

* Status: accepted
* Date: 2026-09-06

Technical Story: `security/audit-findings-2026-09-04.md`, findings 1-7, 16, 20

## Context and Problem Statement

A server with no `security.secrets.master_key_file` used to persist its Ed25519
signing seed as base64 with `encrypted='false'`. Anyone who exfiltrated the
database obtained a forgery-capable federation signing key — the exact threat
the column exists to defend against.

## Considered Options

* Make the master key mandatory: refuse to mint a signing secret, and refuse to
  start, without one
* Keep the plaintext fallback so a server with no key configured still starts

## Decision Outcome

Chosen option: "Make the master key mandatory". Both first generation *and*
rotation now fail closed.

### Positive Consequences

* A server can no longer write a new plaintext signing secret, on any path.

### Negative Consequences

* **Breaking for existing deployments**: a server that started without a master
  key will not start after upgrading until one is configured.
* Rows written by earlier versions are still *read* through the legacy
  plaintext branch, so an existing deployment keeps federating and can rotate on
  its own schedule. That legacy read path is a deliberate compatibility
  concession and is the remaining place plaintext secrets are handled.

## Links

* [`docs/crypto-boundary.md`](../crypto-boundary.md)
* `security/audit-findings-2026-09-04.md`, findings 1-7

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
