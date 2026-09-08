# A suspended account may still refresh its access token

* Status: accepted
* Date: 2026-09-08

Technical Story: security audit of 0.12.8, finding H-01.

## Context and Problem Statement

`POST /_matrix/client/v3/refresh` authenticates with a refresh token, not an
access token, so it never reaches the access-token moderation gate in
`handle_client_server_request`. `refresh_local_session` checked only
`deactivated`. A **locked** account could therefore keep minting access tokens
indefinitely, defeating `M_USER_LOCKED` on every endpoint but logout.

Fixing the locked case is not in question: the spec's [Account
locking](../matrix-v1.19-spec/client-server-api.md#account-locking) section is a
MUST, and [Soft logout](../matrix-v1.19-spec/client-server-api.md#soft-logout)
says a client holding `M_USER_LOCKED` "cannot obtain a new access token until
the account has been unlocked."

The audit went further and asked for the same refusal for **suspended**
accounts. That is the part this record is about, because it is the part a future
reader is likely to "fix" back.

## Decision Drivers

* The spec deliberately leaves a suspended account's permitted actions to the
  implementation, but SHOULD-lists among them: "Log in and create additional
  sessions (which are also suspended)" and "See and receive messages,
  particularly through `/sync` and `/messages`."
* This server's own suspension allowlist (`action_allowed_while_suspended`)
  already permits `POST /login`, which mints unlimited fresh access tokens with
  no rate of decay.
* Access tokens expire. A suspended user whose token expires and cannot refresh
  loses the `/sync` and `/messages` access the spec asks servers to preserve.

## Considered Options

* Refuse both locked and suspended accounts at `/refresh`, as the audit's
  acceptance criteria state.
* Refuse locked accounts only, and treat a rotation as equivalent to the login
  the suspension allowlist already permits.
* Route `/refresh` through the request-path moderation gate.

## Decision Outcome

Chosen option: **refuse locked accounts only.**

`refresh_local_session` returns `401 M_USER_LOCKED` with `soft_logout: true` for
a locked account and serves a suspended one normally.

The reasoning is that refusing `/refresh` while permitting `/login` buys nothing
— the suspended user obtains a fresh access token either way, one round trip
later — and costs the reads the spec asks servers to preserve. A refusal that an
attacker can trivially route around, and that only inconveniences the legitimate
user, is not a security control.

The third option was rejected because the gate authenticates by access token and
`/refresh` does not present one; the check belongs where the refresh token is
resolved to a user.

**The rule this sets for future code: suspension is enforced per action, against
`action_allowed_while_suspended`, and credential *rotation* is not an action in
that sense — it is part of keeping a session the spec says a suspended user may
keep. If suspension is ever meant to end sessions, that is a change to the
allowlist and to `/login`, not a change to `/refresh` alone.**

### Positive Consequences

* Locking is now effective everywhere, which is what the spec requires.
* A suspended user retains the read access the spec SHOULD-lists, without a
  special case that contradicts the server's own `/login` behaviour.

### Negative Consequences

* Departs from the audit's stated acceptance criteria for the suspended case.
  Recorded here so the departure is visible rather than looking like an
  oversight.

## Links

* Implements the moderation gate described in [`docs/auth-identity.md`](../auth-identity.md).
