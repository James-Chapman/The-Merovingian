# Revocation of credentials is one-way

* Status: accepted
* Date: 2026-09-06

Technical Story: security audit of 0.12.6, finding M-05.

## Context and Problem Statement

`POST /_matrix/client/v3/account/password` with `logout_devices` set (the
default) must revoke the access tokens of every device belonging to the user
*except* the one making the request. The obvious way to express "all but one" on
top of the existing per-user and per-device helpers is to revoke everything and
then put one device back.

That is what the code did: `revoke_access_tokens_for_user` plus
`revoke_refresh_tokens_for_user`, followed by `restore_tokens_for_device` for the
caller's own device. `restore_tokens_for_device` was
`UPDATE access_tokens SET revoked = false WHERE user_id = $1 AND device_id = $2`.

That `UPDATE` cannot distinguish a token it revoked microseconds earlier from a
token revoked days earlier by a logout, an admin action, or a previous password
change. Every historical token for that device came back to life. A password
change is precisely the action a user performs *after* discovering a compromise,
so the remediation handed the attacker's revoked token back to them.

## Decision Drivers

* A credential that has ever been revoked must never authenticate again. This is
  the property users are relying on when they change a password.
* Helpers that are individually correct can compose into something unsafe. The
  danger here was not in either revoke helper or in the restore helper alone; it
  was in the sequence.
* An inverse operation is an attractive nuisance: once `restore_tokens_for_device`
  exists, the next person needing "all but one" will reach for it again.

## Considered Options

* Keep revoke-then-restore, but narrow the restore to the token rows the revoke
  call just touched (return their ids and restore only those).
* Add a dedicated "revoke all except this device" operation and delete the
  restore helper entirely.
* Add a `revoked_at` timestamp and restore only rows revoked within this
  transaction's window.

## Decision Outcome

Chosen option: **a dedicated "revoke all except this device" operation, with no
inverse**, implemented as
`database::revoke_tokens_for_user_except_device(store, user_id, keep_device_id)`
(`UPDATE ... WHERE user_id = $1 AND device_id <> $2`), and
`restore_tokens_for_device` removed from both the header and the implementation.

The caller's tokens are never revoked in the first place, so there is no window
during which they are invalid and nothing to reinstate. The unsafe sequence
becomes unrepresentable rather than merely unused.

**The rule this sets for future code: no function in the database layer may
clear a `revoked` flag.** Revocation is a one-way transition. A requirement of
the shape "revoke some subset" must be expressed as a narrower revoke, never as
a broad revoke followed by a restore.

### Positive Consequences

* No code path can un-revoke a credential, so the class of bug is gone rather
  than fixed in one instance.
* The remaining API is smaller: one operation replaces three calls at the only
  call site.

### Negative Consequences

* Callers wanting "all but two devices" would need another predicate rather than
  composing existing helpers. This is deliberate — the composition is exactly
  what was unsafe — but it does mean the API grows by intent rather than by
  combination.
* The `device_id <> $2` predicate does not match rows where `device_id` is NULL.
  Tokens are always written with a device id, so this does not arise today; a
  future nullable `device_id` would need `(device_id IS NULL OR device_id <> $2)`.

## Pros and Cons of the Options

### Narrow the restore to rows just revoked

* Good, because it keeps the existing helpers untouched.
* Bad, because it still leaves an un-revoke primitive in the codebase for the
  next caller to misuse.
* Bad, because it makes correctness depend on threading state between two calls,
  which is harder to review than a single predicate.

### Dedicated revoke-except-device, no inverse

* Good, because the caller's tokens are never invalidated, so no reinstatement
  is needed.
* Good, because it removes the un-revoke primitive entirely.
* Bad, because it does not generalise to arbitrary subsets without more code.

### `revoked_at` timestamp with a restore window

* Good, because it would give an audit trail of when each token was revoked.
* Bad, because it retains the un-revoke primitive and adds clock-skew and
  transaction-boundary questions to a security-critical predicate.
* Bad, because it requires a migration for no benefit the chosen option lacks.

## Links

* Implements the fix for security audit finding M-05 (0.12.7).
* Related: [ADR-0049](0049-treat-the-specification-as-the-conformance-authority.md)
  — the `logout_devices` requirement itself comes from the spec, not from us.
