# Store narrow purpose tokens in separate tables

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

An OpenID token authenticates only "who is this user" to one federation
endpoint. An SSO login token is single-use and short-lived. An access token
authenticates the entire client-server API. All three are bearer credentials,
and the obvious reuse target for the first two was the existing `access_tokens`
table.

If an OpenID or login token could ever be presented as an access token, the
narrow credential becomes a full one.

## Considered Options

* Dedicated `openid_tokens` and `login_tokens` tables, with dedicated mint and
  redeem paths
* One `access_tokens` table with a type column and a runtime check

## Decision Outcome

Chosen option: "Dedicated tables with dedicated paths". `authenticated_user`
never reads `openid_tokens` or `login_tokens`; `federation_openid_userinfo`
never reads `access_tokens` or `sessions`; `database::consume_login_token` is
the sole redemption path.

**The separation is structural, not a runtime check that could be bypassed or
forgotten at a new call site.** Neither lookup path can accidentally accept the
other token kind, because neither path ever reads the other table.

### Negative Consequences

* Three tables and three code paths to keep in step, sharing the same
  `issue_token_hash` keyed-hash machinery but nothing else.
* Two extra schema migrations (versions 10 and 12) and their downgrades.

## Links

* [`docs/auth-identity.md`](../auth-identity.md), "OpenID tokens", "SSO login"
* [`docs/database-persistence.md`](../database-persistence.md)

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
