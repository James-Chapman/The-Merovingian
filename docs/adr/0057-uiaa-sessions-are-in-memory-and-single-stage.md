# UIAA sessions are in-memory and single-stage

* Status: accepted
* Date: 2026-09-08

Technical Story: security audit of 0.12.8, findings L-01 and L-02.

## Context and Problem Statement

Both User-Interactive Authentication challenges this server issues — registration
(`m.login.registration_token`) and device deletion (`m.login.password`) — carried
a compile-time constant session id: `merovingian-ui-auth`, `delete_device`,
`delete_devices`. The spec's [User-Interactive Authentication
API](../matrix-v1.19-spec/client-server-api.md#user-interactive-authentication-api)
treats the session id as the server's handle on one in-flight attempt. A
constant is not a handle: every client and every attempt shared it, and an
attacker knew its value in advance.

The audit's recommended fix was the full article — generate a unique session id,
persist it in an `auth_sessions` table, track completed stages, and consult a
configured stage list. There is no `auth_sessions` table in this codebase and no
configured UIAA stage list; the audit's own verifier noted the practical bypass
"does not exist yet" and downgraded both findings to low.

## Decision Drivers

* A guessable, shared session id is wrong on its own terms and cheap to fix.
* Stage *ordering* is only meaningful once there are two stages to order. Both
  flows here have exactly one.
* A UIAA session in these flows carries no authority. The credential the stage
  actually checks — a registration token, or the account password — travels
  inside the same request's `auth` block. Holding a session id grants nothing.
* The spec permits completing a single-stage flow in one shot, and this
  project's own test fixtures and real clients do exactly that. Requiring a
  prior challenge would break them for no security gain.
* A schema migration for state that must not survive a restart is the wrong
  shape.

## Considered Options

* Persist sessions in a new `auth_sessions` table with completed-stage tracking,
  as the audit recommended.
* Mint a unique per-attempt session id, hold outstanding challenges in memory,
  and validate a client-supplied session against them.
* Mint a unique id and do not validate the echo at all.

## Decision Outcome

Chosen option: **a unique per-attempt session id, held in memory, validated when
supplied.**

`issue_uia_session()` mints a random id per challenge, scoped to the endpoint
that issued it, held in `ClientServerRuntime::uia_sessions` — bounded (512
entries) and TTL-pruned (10 minutes), because every 401 an unauthenticated
caller provokes mints one. `uia_session_is_valid()` accepts a client-supplied
`auth.session` only when this server issued it, issued it *for that endpoint*,
and it has not expired. A request that omits `session` entirely is still served.

**The rule this sets for future code: the moment a second UIAA stage is added,
this is no longer sufficient.** Multi-stage flows need server-side
completed-stage tracking, and because a partially completed flow must survive
across requests that may land on different processes, that tracking needs to be
persisted — which is when the `auth_sessions` table the audit asked for becomes
the right answer. Do not add a second stage without it.

### Positive Consequences

* An attacker can no longer predict a session id, and a session minted for a
  device deletion cannot satisfy a registration challenge.
* No schema change, and no restart-surviving state for something that must not
  survive a restart.
* Single-shot registration keeps working, so no client or fixture breaks.

### Negative Consequences

* Sessions are per-process. In a multi-process deployment a client that gets its
  challenge from one process and completes against another will have its session
  rejected and receive a fresh challenge. Harmless for a single-stage flow,
  unacceptable for a multi-stage one — see the rule above.
* The bound means a sustained flood of 401s evicts older outstanding challenges.
  An evicted client simply receives a fresh challenge on retry.

## Links

* Extends the token-lifecycle rules in [`src/auth/AGENTS.md`](../../src/auth/AGENTS.md).
