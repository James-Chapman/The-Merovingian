# Merovingian Security Audit Report — September 2026

**Scope:** Full security audit across high-risk surfaces.
**Method:** 7 parallel specialist auditors (auth, client-server HTTP/DoS, federation,
crypto, database, observability, event engine) followed by 7 adversarial verifiers.
**Result:** 39 findings proposed; 33 confirmed after verification (7 refuted or
adjusted to residual risk).
**Authority:** Matrix spec v1.19, project `AGENTS.md` files, `docs/security-coding-rules.md`.
**Branch context:** produced on `codex/fix-log-filtering-noise`.

---

## How another LLM should use this report

1. **Read the whole issue first.** Each issue includes the exact file and line, the
   verifier's confidence, and the spec/rule reference. Do not assume the title is
   enough.
2. **Write the test first.** Every issue has a GIVEN/WHEN/THEN acceptance block and a
   suggested test approach. Create the test in the appropriate `tests/` directory
   before editing production code.
3. **Make the minimal correct fix.** Prefer fixing the root cause over the symptom.
   If a finding mentions two linked defects (e.g. construction + use), fixing the
   root cause is usually enough, but add a defense-in-depth guard at the use site if
   the data can still arrive mis-sized.
4. **Run the new test and the relevant suite.** Read the real summary from
   `build-wsl/meson-logs/testlog.txt`; do not rely on `build.py` exit codes.
5. **Update docs.** If the change invalidates guidance in `docs/`, update it. If the
   fix involves a non-obvious rejected alternative or a constraint future code must
   keep, record an ADR in `docs/adr/`.
6. **Mark the issue done.** Check the box in this file once the test passes and the
   fix is committed. Do not mark it done from a run you did not read.

---

## Severity guide

- **High:** Spec violation, authentication bypass, remote DoS, secret leakage, or
  cryptographic/data-integrity defect that is reachable in production today.
- **Medium:** Real defect that is partially mitigated, latent in currently unused
  code paths, or a significant spec/robustness gap that will become exploitable as
  the feature set expands.
- **Low:** Defense-in-depth gap, operator-tooling usability issue, or residual risk
  that is documented and bounded today but should still be closed.

---

## High-severity findings (7)

### H-01 — `/refresh` endpoint bypasses the locked-account moderation gate

- **Surface:** Authentication & session management
- **Location:** `src/homeserver/auth_service.cpp:1236`
  (handler entry at `src/homeserver/client_server.cpp:9603-9627`)
- **Severity:** high
- **Verifier confidence:** certain
- **Spec reference:** Matrix v1.19 Client-Server API §Account locking: "When an
  account is locked, servers MUST return a 401 … M_USER_LOCKED … on all but the
  following Client-Server APIs: POST /logout, POST /logout/all." §Soft logout: a
  locked client "cannot obtain a new access token until the account has been
  unlocked."
- **Project rule:** Moderation gate at `src/homeserver/client_server.cpp:9991-10034`;
  `src/auth/AGENTS.md` token lifecycle rules.

#### Description

`POST /_matrix/client/v3/refresh` is dispatched **before** the access-token and
account-state moderation gate. `refresh_local_session` checks only whether the
account is `deactivated`; it never inspects `user->locked` or `user->suspended`. A
locked account can therefore keep refreshing an existing refresh token and receive
fresh access tokens indefinitely, defeating `M_USER_LOCKED` for every endpoint
except logout.

#### Evidence

- `src/homeserver/client_server.cpp:9603-9627` dispatches `/refresh` before the
  moderation gate.
- `src/homeserver/auth_service.cpp:1236` checks `deactivated` only.
- `src/homeserver/client_server.cpp:9991-10034` enforces locked/suspended checks
  only for routes reached after authentication.

#### Impact

A locked user remains able to use any Client-Server API that accepts an access
token, because `/refresh` continuously mints new ones. This is a direct spec
violation and renders account locking ineffective.

#### Recommended fix

Route `/refresh` through the same moderation gate as other endpoints, or add an
explicit `locked`/`suspended` check inside `refresh_local_session` before any
new token is issued. The response must be `401 M_USER_LOCKED` with
`soft_logout: true` when the account is locked.

#### Acceptance criteria

- **GIVEN** a locked account with a valid refresh token,
  **WHEN** the client calls `POST /_matrix/client/v3/refresh`,
  **THEN** the server responds with `401` and `M_USER_LOCKED` with `soft_logout: true`,
  and no new access or refresh token is issued.
- **GIVEN** a suspended account with a valid refresh token,
  **WHEN** the client calls `POST /_matrix/client/v3/refresh`,
  **THEN** the server responds with the same suspended-account error used by other
  authenticated endpoints, and no token is issued.
- **GIVEN** a normal locked-account test that already covers `/sync` and `/logout`,
  **WHEN** the suite runs,
  **THEN** it also covers `/refresh`.

#### Suggested test approach

Add a conformance test in `tests/conformance/test_client_server.cpp` (or extend
existing locked-account tests) that refreshes a token after locking the account.
Verify the error code, HTTP status, and absence of new token fields in the JSON
response. Also add a unit test for `refresh_local_session` that passes a locked
`LocalUser` and expects the locked error.

---

### H-02 — Refresh-token rotation does not verify the target device still exists

- **Surface:** Authentication & session management
- **Location:** `src/homeserver/auth_service.cpp:1227`
  (device recreation at `src/homeserver/client_server.cpp:9616-9618`)
- **Severity:** high
- **Verifier confidence:** high
- **Spec reference:** Matrix v1.19 Client-Server API §Login: "The returned access
  token must be associated with the device_id supplied by the client or generated
  by the server. The server may invalidate any access token previously associated
  with that device."
- **Project rule:** `src/auth/AGENTS.md` token lifecycle; always hash tokens before
  storing.

#### Description

`refresh_local_session` validates only that `device_id` is syntactically valid. It
never checks that the device still exists in `runtime.database.persistent_store.devices`
or `runtime.devices`. The refresh handler then lazily recreates the runtime device
row (with `display_name == device_id`) if it is missing. If a device's row is
deleted but its refresh token somehow survives (leaked token, revocation race,
partial deactivation), an attacker can refresh and resurrect a session for a
non-existent device, obtaining a fresh access token bound to a "zombie" device.

#### Evidence

- `src/homeserver/auth_service.cpp:1224-1230` checks `find_user` and
  `device_id_is_valid` but not device existence.
- `src/homeserver/client_server.cpp:9616-9618` recreates the device if
  `find_device` returns `nullptr`.

#### Impact

Defense-in-depth fail-open: the token lifecycle assumes tokens are tied to real
devices. A stale or leaked refresh token can reanimate a deleted device and
continue minting access tokens.

#### Recommended fix

Inside `refresh_local_session`, look up the device in the persistent store and in
the runtime device map. If the device does not exist, reject the refresh with
`M_UNKNOWN_TOKEN` (or the same error used for a fully revoked session) and do
not recreate it. If the device was deleted, any associated refresh tokens must be
considered revoked.

#### Acceptance criteria

- **GIVEN** a user with a valid refresh token for device `D`,
  **WHEN** device `D` is deleted and the client calls `/refresh`,
  **THEN** the server returns an error and issues no tokens, and the runtime device
  map is not recreated for `D`.
- **GIVEN** the same scenario but the device still exists,
  **WHEN** `/refresh` is called,
  **THEN** a new access token and a rotated refresh token are returned normally.

#### Suggested test approach

Unit test `refresh_local_session` with a user whose device has been removed from
the mock persistent store. Integration test: register, log in, delete the device,
then attempt `/refresh` and assert failure. Confirm that a normal refresh still
works when the device exists.

---

### H-03 — Plain-HTTP accepted sockets remain blocking on `send`

- **Surface:** Client-server HTTP / DoS
- **Location:** `src/homeserver/http_server.cpp:220`
  (socket creation at `src/homeserver/http_server.cpp:1605`)
- **Severity:** high
- **Verifier confidence:** high
- **Project rule:** `docs/http-transport.md` TLS listener boundary; ADR-0054
  non-blocking-for-life rule; `docs/security-coding-rules.md` accepted-socket rules.

#### Description

`PlainConnectionStream::write` calls `::send()` directly with `MSG_NOSIGNAL` but no
timeout and no non-blocking mode. The accept loop creates client sockets with
`accept4(..., SOCK_CLOEXEC)` only; it never sets `O_NONBLOCK`. A peer that stops
reading can park a main-pool worker indefinitely while the server sends a large
response (media download, `/sync`, admin metrics). ADR-0054 fixed this for TLS
sockets, but the plain-HTTP path was left blocking.

#### Evidence

- `src/homeserver/http_server.cpp:220-223`: `::send(m_fd, data.data(), data.size(), MSG_NOSIGNAL)`.
- `src/homeserver/http_server.cpp:1605`: `accept4(..., SOCK_CLOEXEC)` only.
- `src/homeserver/tls.cpp:280`: TLS sockets explicitly set non-blocking.

#### Impact

Remote DoS against any deployment that exposes a plain-HTTP listener (e.g.
reverse-proxy setups, internal health listeners, or operator-disable of TLS). One
slow client can exhaust the worker pool.

#### Recommended fix

Set `O_NONBLOCK` on accepted plain-HTTP sockets and make
`PlainConnectionStream::write` poll for send readiness before calling `::send`,
mirroring the TLS pump design. Add a send deadline so the connection is closed if
send readiness does not arrive in time.

#### Acceptance criteria

- **GIVEN** a plain-HTTP listener,
  **WHEN** a client accepts a response but stops reading,
  **THEN** the worker returns to the pool after the send deadline instead of blocking
  indefinitely.
- **GIVEN** the same server under a slow-client flood,
  **WHEN** legitimate clients make requests,
  **THEN** they are served without the worker pool being exhausted.

#### Suggested test approach

Add an integration test that connects to the plain-HTTP listener, requests a large
payload, and then stops reading. Assert that the server does not hang forever and
that the connection is eventually closed. A unit test can verify the socket is
non-blocking after acceptance.

---

### H-04 — Per-PDU failure trust-accounting reset bypasses backoff/circuit breaker

- **Surface:** Federation security
- **Location:** `src/federation/inbound_request.cpp:2767`
  (reset at lines 2915-2916)
- **Severity:** high
- **Verifier confidence:** certain
- **Spec reference:** Matrix v1.19 Server-Server API §Transactions.
- **Project rule:** `src/federation/AGENTS.md` rule 2; trust-boundary note in
  `src/federation/security.cpp`.

#### Description

When individual PDUs inside a transaction are rejected, the handler increments a
**local copy** of `remote.trust.consecutive_failures`. After processing the whole
transaction it unconditionally sets the counter to 0 and persists it. A peer can
therefore send endless transactions full of invalid/forged PDUs that each return
HTTP 200 with per-PDU errors, while never tripping the consecutive-failures
backoff or circuit-breaker logic.

#### Evidence

- `src/federation/inbound_request.cpp:2767`: increments `remote.trust.consecutive_failures`.
- `src/federation/inbound_request.cpp:2915-2916`: `remote.trust.consecutive_failures = 0;` then persist.
- The copy is made at line 2425: `auto remote = inbound_resolution.remote;`.

#### Impact

Forged/invalid PDU floods from a malicious server evade the trust-accounting
penalty mechanism. The victim wastes resources rejecting each PDU but never
circuit-breaks the peer.

#### Recommended fix

Persist the incremented `consecutive_failures` after each PDU rejection, or at least
preserve the count when failures occurred, instead of resetting it to zero
unconditionally at transaction completion. Ensure the persisted value is the one
that reflects the number of consecutive failures observed.

#### Acceptance criteria

- **GIVEN** a federation peer sends a transaction containing only rejected PDUs,
  **WHEN** multiple such transactions are received in sequence,
  **THEN** `remote.trust.consecutive_failures` increases monotonically and the
  circuit breaker / backoff eventually triggers.
- **GIVEN** a transaction containing only rejected PDUs,
  **WHEN** the response is returned,
  **THEN** the persisted failure count is greater than it was before the transaction.

#### Suggested test approach

Unit test the inbound transaction handler with a mock `remote` record and a
provider that rejects every PDU. Assert that after N transactions the persisted
`consecutive_failures` equals N (or the per-PDU count, depending on intended
gapN semantics), not 0. Add a regression test that confirms the circuit breaker
fires after the configured threshold.

---

### H-05 — Event signer logs full signing payloads and signed event bodies

- **Surface:** Event engine & observability
- **Location:** `src/events/event_signer.cpp:370-377`
- **Severity:** high
- **Verifier confidence:** certain
- **Project rule:** `src/observability/AGENTS.md`: "Never log secret material. No
  tokens, passwords, private keys, or full request bodies.";
  `docs/security-coding-rules.md` §Secrets and logging.

#### Description

`sign_event_for_server` calls `log_diagnostic("sign_event.accepted", ...)` with
unredacted fields named `signing_payload` and `signed_json`, both marked
`sensitive=false`. The observability redactor keys on markers such as `body`,
`signature`, `event_content`, `content_json`, etc.; it does not recognize these
key names, so the full canonical signing payload and the complete signed event
JSON are written to debug logs. This leaks request-body/event-content data.

#### Evidence

- `src/events/event_signer.cpp:370-377` emits `{"signing_payload", payload.output, false}`
  and `{"signed_json", signed_json.output, false}`.
- `src/observability/observability.cpp:72-98` lists the redaction markers.

#### Impact

An operator capturing debug logs retains full event content and signing payloads,
which is exactly the data the project's logging rules forbid. If logs are shipped
to a SIEM or shared during incident response, confidential federation data leaks.

#### Recommended fix

Either mark these fields `sensitive=true` so the redactor removes them, or add
`signing_payload` and `signed_json` to the redactor's sensitive-key list. Better
still, do not log signing payloads or signed JSON at any level; if diagnostics
are needed, log only a hash or an event ID.

#### Acceptance criteria

- **GIVEN** debug logging is enabled,
  **WHEN** `sign_event_for_server` accepts a signing request,
  **THEN** the emitted diagnostic contains neither the full signing payload nor the
  full signed event JSON.
- **GIVEN** a test that captures the emitted diagnostic,
  **WHEN** it inspects the fields,
  **THEN** `signing_payload` and `signed_json` are absent or redacted.

#### Suggested test approach

Add a unit test that calls `sign_event_for_server` with a sample event while a
test audit/diagnostic sink captures all emitted fields. Assert that no emitted
field contains the raw signing payload or signed JSON content. If only a redacted
placeholder is present, accept it.

---

### H-06 — V2 state resolution omits `authorising_user_member` for restricted joins

- **Surface:** Event engine & room state
- **Location:** `src/events/state_resolution.cpp:270-327`
  (authorization requirement at `src/events/authorization.cpp:1170-1184`)
- **Severity:** high
- **Verifier confidence:** certain
- **Spec reference:** Matrix v1.19 `rooms/v8.md` (MSC3083 restricted joins);
  `server-server-api.md` authorization-rules restricted join rule.

#### Description

`build_auth_event_map_from_state()` populates `create`, `power_levels`, `join_rules`,
`sender_member`, `target_member`, and `third_party_invite`, but never assigns
`result.authorising_user_member`. The restricted-join authorization branch
requires that member event to validate `content.join_authorised_via_users_server`.
Consequently, valid restricted-room join events in the conflicted set are
rejected during v2 state resolution, producing divergent room state across servers
and locking out authorized joiners.

#### Evidence

- `src/events/state_resolution.cpp:270-327`: no assignment to
  `result.authorising_user_member`.
- `src/events/authorization.cpp:1170-1184`: requires the authorising user's
  membership for restricted/restricted_v2 joins.

#### Impact

Divergent room state across federation for restricted rooms; authorized joiners are
rejected; state resolution produces different outcomes on different servers.

#### Recommended fix

Populate `result.authorising_user_member` from the resolved `m.room.member` state
for the user named by `content.join_authorised_via_users_server` before
auth-checking restricted joins.

#### Acceptance criteria

- **GIVEN** a restricted room and a conflicted set containing a valid restricted join,
  **WHEN** v2 state resolution runs,
  **THEN** the join is authorized and accepted into resolved state.
- **GIVEN** the same room but the authorising user is not a member,
  **WHEN** v2 state resolution runs,
  **THEN** the restricted join is rejected.
- **GIVEN** a test vector from the Matrix spec for restricted joins,
  **WHEN** it is fed through the v2 resolver,
  **THEN** the output matches the spec reference.

#### Suggested test approach

Add a conformance test in `tests/conformance/test_state_resolution.cpp` (or the
appropriate events conformance file) with a restricted-join scenario that
includes the authorising user's membership. Assert the join survives resolution.
Add a negative test where the authorising user is absent and assert rejection.

---

### H-07 — State resolution ignores string-encoded power levels in room v1–v9

- **Surface:** Event engine & room state
- **Location:** `src/events/state_resolution.cpp:186`
- **Severity:** high
- **Verifier confidence:** certain
- **Spec reference:** Matrix v1.19 `rooms/v10.md` (v1–v9 accepted string power-level
  values); `server-server-api.md` room-state-resolution reverse topological power
  ordering.

#### Description

`extract_user_power()` reads `content.users` entries with `integer_member()` only.
For room versions 1–9 the spec permits power levels encoded as JSON strings, and
the authorization path already parses them via
`power_level_value(..., allow_string_values)`. The resolver treats string-encoded
entries as absent and falls back to `users_default`, corrupting sender-power
comparisons in reverse-topological power ordering and mainline depth.

#### Evidence

- `src/events/state_resolution.cpp:164-188`: line 186 uses `integer_member()` only.
- Authorization path uses `power_level_value(..., allow_string_values)`.

#### Impact

A lower-power sender's event can win over a higher-power sender's event in state
resolution, corrupting privilege-derived resolution outcomes and potentially
allowing unauthorized state transitions to dominate.

#### Recommended fix

Respect the room-version policy's `power_levels_require_integers` flag and parse
string-encoded levels in v1–v9 rooms, matching the authorization path. Use the
same helper (`power_level_value` with `allow_string_values`) or equivalent logic.

#### Acceptance criteria

- **GIVEN** a v1–v9 room with a power-levels event containing string-encoded user levels,
  **WHEN** v2 state resolution computes sender power,
  **THEN** the string values are parsed and used, not ignored.
- **GIVEN** a v10+ room with the same content,
  **WHEN** v2 state resolution computes sender power,
  **THEN** only integer values are accepted and string values are treated as absent
  (per that room version's policy).
- **GIVEN** the same state set, the resolver output matches the spec reference vectors.

#### Suggested test approach

Add a unit test that constructs a `content.users` map with both integer and string
values and calls `extract_user_power()` under different room-version policies.
Add a conformance state-resolution test where a high-power string-valued user
wins over a low-power integer-valued user in a v1–v9 room.

---

## Medium-severity findings (11)

### M-01 — Registration token file hash is cached indefinitely by path without invalidation

- **Surface:** Authentication & session management
- **Location:** `src/homeserver/auth_service.cpp:832`
- **Severity:** medium
- **Verifier confidence:** certain
- **Project rule:** `docs/security-coding-rules.md` §Secrets and logging; cached
  credentials whose backing secret changes must not remain valid.

#### Description

`load_hashed_registration_token` stores the computed Argon2id hash in a static
`std::unordered_map` keyed by the token file path. It returns the cached hash on
every subsequent call without re-reading the file, checking mtime, or providing any
invalidation path. If an operator rotates the token on disk, the server continues
to accept the old token until restart.

#### Evidence

- `src/homeserver/auth_service.cpp:829-830`: static cache keyed by
  `registration.token_file`.
- `src/homeserver/auth_service.cpp:832`: returns cached hash.

#### Recommended fix

Re-read the file and recompute the hash when the file's identity changes (mtime +
size, or a content hash), or provide an explicit invalidation mechanism such as a
SIGHUP handler or config reload hook. Do not cache by path alone.

#### Acceptance criteria

- **GIVEN** a running server with a registration token file,
  **WHEN** the operator writes a new token to that file,
  **THEN** the old token is rejected and the new token is accepted without restarting
  the server.
- **GIVEN** the file has not changed,
  **WHEN** a registration is attempted,
  **THEN** the existing hash is reused (performance is preserved).

#### Suggested test approach

Integration test: set a token file, start the auth service, register with the
old token, then overwrite the file with a new token and assert the old token is
rejected while the new one works. Unit test: mock the file system so mtime/size
changes trigger a re-read.

---

### M-02 — Rate-limit bucket tables use attacker-influenceable `std::unordered_map` with `std::hash`

- **Surface:** Client-server HTTP / DoS
- **Location:** `include/merovingian/http/rate_limit.hpp:403-406`
  (key assembly in `src/homeserver/client_server.cpp:2659-2661` and `2679-2681`)
- **Severity:** medium (downgraded from high by verifier due to bounded input and
  table size)
- **Verifier confidence:** medium
- **Project rule:** `docs/security-coding-rules.md` robust resource guards.

#### Description

`RateLimitEngine` stores per-IP and per-user buckets in
`std::unordered_map<std::string, Bucket>` using the default `std::hash<std::string>`.
Per-IP keys include `effective_client_ip + '|' + normalized_target`. Because
`std::hash<string>` is not collision-resistant, the DoS defense itself is
vulnerable to HashDoS: a flood of distinct keys that hash to the same bucket
degrades insert/lookup. The verifier noted that `effective_client_ip` is validated
as an IP address and `kMaxBucketsPerTable=100'000` bounds total entries, so the
practical severity is medium rather than high.

#### Evidence

- `include/merovingian/http/rate_limit.hpp:403-406`: map declarations.
- `src/homeserver/client_server.cpp:2659-2661` and `2679-2681`: key assembly.
- `include/merovingian/http/rate_limit.hpp:381`: insertion.

#### Recommended fix

Use a collision-resistant hash (SipHash-2-4 or `crypto_generichash`) for
attacker-influenceable bucket keys, or switch to a structure whose worst-case
lookup is bounded (e.g. a tree, or an open-addressing table with a cryptographic
hash). Keep the existing eviction policy.

#### Acceptance criteria

- **GIVEN** a HashDoS probe that sends many valid IP strings,
  **WHEN** the rate-limit table is stressed,
  **THEN** lookup time remains near O(1) and the table still enforces rate limits.
- **GIVEN** a benchmark measuring lookup/insert time under adversarial keys,
  **WHEN** it is compared against random keys,
  **THEN** the performance ratio is bounded by a small constant.

#### Suggested test approach

Add a unit test or benchmark that constructs adversarial `std::string` keys and
measures insertion/lookup time. Replace the hash and rerun; assert no pathological
degradation. A simpler test: assert that the bucket map's hash function is not the
default `std::hash`.

---

### M-03 — Transport-layer error responses omit CORS headers

- **Surface:** Client-server HTTP
- **Location:** `src/homeserver/http_server.cpp:920-924`
- **Severity:** medium
- **Verifier confidence:** high
- **Spec reference:** Matrix v1.19 Client-Server API §10.5: "supply Cross-Origin
  Resource Sharing (CORS) headers on all requests."
- **Project rule:** `docs/http-transport.md` CORS on client responses.

#### Description

`write_error_response` formats parser errors, timeouts, and head-too-large errors
without applying CORS headers. Only the body-too-large path manually adds them.
Consequently, 400, 408, and 413 responses from the transport layer on the
client-server listener lack `Access-Control-Allow-Origin`, causing browsers to
surface them as CORS failures rather than the real HTTP status.

#### Evidence

- `src/homeserver/http_server.cpp:920-924`: `format_response(status, body)` with no CORS.
- Used at lines 1024, 1033, and 1049 for head-too-large, timeout, and parser errors.
- `src/homeserver/http_server.cpp:1105-1125`: body-too-large path adds CORS.

#### Recommended fix

Route transport-layer client-server errors through the same CORS header
application used for application-layer responses, or ensure `write_error_response`
applies the runtime CORS policy before formatting the response.

#### Acceptance criteria

- **GIVEN** a CORS-enabled client-server listener,
  **WHEN** any transport-layer error (parser error, timeout, head too large, body too
  large) is returned,
  **THEN** the response includes the configured `Access-Control-Allow-Origin` header
  and, when credentials are enabled, the correct `Access-Control-Allow-Credentials`
  handling.

#### Suggested test approach

Add unit tests for `write_error_response` with a runtime CORS policy configured.
Assert that 400, 408, 413, etc. all carry `Access-Control-Allow-Origin`. Add an
integration test that triggers each error from a browser-like request with an
`Origin` header and inspects the response headers.

---

### M-04 — v1 invite endpoint defaults to room version `12` for signature verification and event-ID computation

- **Surface:** Federation security
- **Location:** `src/federation/membership_endpoints.cpp:263`
- **Severity:** medium (downgraded from high by verifier; spec requires v1/v2
  assumption)
- **Verifier confidence:** certain
- **Spec reference:** Matrix v1.19 Server-Server API §Versioned rooms; Room
  v10/v11/v12 specifications.

#### Description

For v1-style invites, `parse_invite_body` leaves `room_version` empty, and
`handle_invite` falls back to hard-coded room version `12` when computing the
reference-hash event ID and verifying the Ed25519 signature. For rooms whose
actual version is not 12, the signature and hash use the wrong rules. The verifier
notes that the spec expects v1 invites to be treated as room version 1 or 2, not
12, so this is a real spec violation but the severity is medium because v1 invite
endpoints are a narrower path than general federation.

#### Evidence

- `src/federation/membership_endpoints.cpp:263` and surrounding code: hard-coded `12`.
- `parse_invite_body` returns an empty `room_version` for v1 invites.

#### Recommended fix

Extract the room version from the room's `m.room.create` state via the existing
`room_version_resolver` for v1 invites, exactly as `send_join` does, instead of
defaulting to `12`. If the room does not exist yet, fall back to the spec-mandated
v1/v2 default, not v12.

#### Acceptance criteria

- **GIVEN** a v1 invite for a non-v12 room,
  **WHEN** the server computes the event ID and verifies the signature,
  **THEN** it uses the actual room version's rules, not v12.
- **GIVEN** a v1 invite for a room whose create event is unavailable,
  **WHEN** the server must pick a default,
  **THEN** it uses v1 or v2 (per spec), not v12.

#### Suggested test approach

Add federation conformance tests for v1 invites targeting v1, v2, v10, and v12
rooms. Assert that the accepted/rejected outcome and computed event ID match the
spec reference for each version.

---

### M-05 — `signature_verified` flag in main verifier creates fail-open bypass if set prematurely

- **Surface:** Federation security
- **Location:** `src/federation/inbound_request.cpp:2394`
- **Severity:** medium
- **Verifier confidence:** high
- **Spec reference:** Matrix v1.19 Server-Server API §Request authentication (X-Matrix).
- **Project rule:** `src/federation/AGENTS.md` rule 1.

#### Description

`verify_inbound_federation_signature` skips the Ed25519 signature check entirely
when `request.signature_verified` is `true`. The flag is meant for worker-bound
requests already verified by main, but the verifier is exported and the flag is a
simple bool. Any caller that accidentally sets it before verification bypasses
X-Matrix crypto authentication.

#### Evidence

- `src/federation/inbound_request.cpp:2394`: early return when `signature_verified`.

#### Recommended fix

Do not honor `signature_verified` inside `verify_inbound_federation_signature`;
reserve the bypass for `handle_inbound_federation_request`, which is the worker
entry point, and document that the main verifier always performs cryptographic
verification. Alternatively, require a separate internal-only verifier for the
worker path and keep the public verifier strict.

#### Acceptance criteria

- **GIVEN** a request with `signature_verified = true` but an invalid X-Matrix signature,
  **WHEN** `verify_inbound_federation_signature` is called,
  **THEN** the signature is checked and the request is rejected.
- **GIVEN** a worker-proxied request that main has already verified,
  **WHEN** it reaches the worker entry point,
  **THEN** the worker can still accept it without re-verifying.

#### Suggested test approach

Unit test: call `verify_inbound_federation_signature` with `signature_verified = true`
and a bad signature; assert it returns the signature-failure error. Ensure the
worker entry point has a separate test covering the valid bypass path.

---

### M-06 — Exported well-known delegation overload uses a literal/test discovery network

- **Surface:** Federation security
- **Location:** `src/federation/server_discovery.cpp:481`
- **Severity:** medium
- **Verifier confidence:** certain
- **Spec reference:** Matrix v1.19 Server-Server API §Resolving server names.
- **Project rule:** `src/federation/AGENTS.md` SSRF boundary.

#### Description

The overload `discover_server(server_name, well_known_server)` instantiates
`LiteralDiscoveryNetwork`, whose `lookup_addresses` returns the documentation IP
`203.0.113.10` for every non-numeric delegated host. The overload is exported in
the public header. Although current production code does not call it, any future
federation path that uses it would bypass real DNS resolution and SSRF checks and
be directed to a test IP.

#### Evidence

- `src/federation/server_discovery.cpp:481`: `LiteralDiscoveryNetwork` overload.
- Header exports the two-argument overload.

#### Recommended fix

Remove or hide the literal-network overload from the public API, or make it
require an explicit test-network argument so it cannot be mistaken for production
discovery. If kept for tests, move it into a test-only helper.

#### Acceptance criteria

- **GIVEN** production federation code,
  **WHEN** it calls `discover_server`,
  **THEN** only the production network path is available and a compile-time error
  occurs if the test overload is used.
- **GIVEN** the test overload is needed,
  **WHEN** it is invoked,
  **THEN** it requires an explicit opt-in token or type and cannot be called by
  accident.

#### Suggested test approach

Add a compile-time / static-analysis check (e.g. grep or build test) that no
`src/` call site uses the two-argument `discover_server` overload. Add a unit
test verifying the overload is only reachable through a test-only type.

---

### M-07 — `RuntimeEd25519Provider::sign` calls libsodium without validating secret key size

- **Surface:** Cryptographic boundary
- **Location:** `src/crypto/runtime_ed25519_provider.cpp:18-28`
- **Severity:** medium (downgraded from high; production callers validate upstream)
- **Verifier confidence:** certain
- **Project rule:** `src/crypto/AGENTS.md` rule 4: validate external key material
  before use.

#### Description

`RuntimeEd25519Provider::sign` passes `secret_key_.bytes().data()` directly to
`crypto_sign_detached` without first checking that the held `SecretBuffer` is
exactly 64 bytes. The low-level primitive `ed25519_sign_detached` in the same
module validates size, and `RuntimeMultiKeyEd25519Provider::sign` also validates
size, but the single-key provider bypasses this. The verifier notes the production
path mitigates this upstream, so the severity is medium, but the provider contract
is still broken.

#### Evidence

- `src/crypto/runtime_ed25519_provider.cpp:18-28`: no size check.
- `src/crypto/ed25519.cpp:75`: low-level primitive checks `secret_key.size() != 64U`.
- `src/crypto/runtime_multikey_ed25519_provider.cpp:27`: multi-key provider checks size.

#### Recommended fix

Add a size precondition check in `RuntimeEd25519Provider::sign` (or in the
constructor, see L-07) and fail closed if the secret is not exactly 64 bytes.
Delegating to `ed25519_sign_detached` would reuse the existing validation.

#### Acceptance criteria

- **GIVEN** a `RuntimeEd25519Provider` constructed with a SecretBuffer whose size is
  not 64 bytes,
  **WHEN** `sign()` is called,
  **THEN** the call returns an error and does not invoke libsodium.
- **GIVEN** a 64-byte secret,
  **WHEN** `sign()` is called,
  **THEN** a valid detached signature is produced.

#### Suggested test approach

Unit test: construct the provider with 63-byte, 64-byte, and 65-byte secrets and
assert `sign()` errors for the wrong sizes and succeeds for 64 bytes. If the
constructor is also fixed, add a construction-time test.

---

### M-08 — Plaintext storage of identity-server unbind credentials in `account_threepids`

- **Surface:** Database persistence
- **Location:** `src/database/persistent_store.cpp:2787-2788`
- **Severity:** medium (downgraded from high; documented threat-model trade-off)
- **Verifier confidence:** high
- **Spec reference:** Matrix v1.19 Identity Service API §`POST /_matrix/identity/v2/3pid/unbind`
  defines auth mode 2 as `sid` + `client_secret`.
- **Project rule:** `docs/security-coding-rules.md` §Secrets and logging;
  `src/database/AGENTS.md` rule 2; CWE-312.

#### Description

The `account_threepids` table stores the Matrix Identity Service unbind
authentication pair (`client_secret` and `sid`) as plaintext `TEXT` columns.
`store_account_threepid` binds both values as `public_value` (`sensitive=false`).
A database compromise exposes credentials sufficient to drive IS API unbind auth
mode 2, which can detach 3PIDs and interfere with account recovery or
deactivation flows. The verifier notes this is a deliberate, documented trade-off
in `docs/threat-model.md` §IS-delegated bind/unbind/requestToken (v0.11.10), with
mitigations including DB role gating, but the risk remains real.

#### Evidence

- `migrations/007_account_threepids_columns.sql:3-5`: `client_secret TEXT`, `sid TEXT`.
- `include/merovingian/database/persistent_store.hpp:406-412`: stored as
  `std::optional<std::string>`.
- `src/database/persistent_store.cpp:2787-2788`: bound as `public_value`.

#### Recommended fix

Treat these values as authentication secrets: bind them as `sensitive_value`,
restrict their logging, and remove the plaintext-storage trade-off by deriving
or recovering them through a homeserver-signed unbind (auth mode 1) or by
encrypting the column. At minimum, mark the bindings `sensitive_value` so they are
not logged by the query tracer.

#### Acceptance criteria

- **GIVEN** a 3PID bind/unbind operation,
  **WHEN** `client_secret` and `sid` are persisted or bound to a prepared statement,
  **THEN** they are treated as sensitive values and never emitted in diagnostic
  logs or query traces.
- **GIVEN** a database dump,
  **WHEN** the `account_threepids` table is inspected,
  **THEN** `client_secret` and `sid` are not plaintext (if the encryption/mode-1
  fix is implemented) or are at least documented as a residual risk.

#### Suggested test approach

Unit test: call `store_account_threepid` and assert that the bound values are
marked sensitive. Add a diagnostic-log test that confirms no `client_secret` or
`sid` appears in query traces or structured logs. If implementing mode-1 unbind,
add an end-to-end IS mock test.

---

### M-09 — PostgreSQL backend sends `BLOB`/`BYTEA` parameters as null-terminated text

- **Surface:** Database persistence
- **Location:** `src/database/postgresql_store.cpp:241-243`
- **Severity:** medium
- **Verifier confidence:** high
- **Project rule:** `docs/database-persistence.md:108-118`: binary payload columns
  are `BLOB`/`BYTEA` and must round-trip byte-exactly.

#### Description

`execute_prepared_statement` passes every bound value to `PQexecParams` with
`format=0` (text) and null parameter-length/format arrays, so values are
interpreted as C strings. Binary columns declared as `BLOB` (translated to
`BYTEA`) such as `media_blobs.bytes` therefore truncate at any embedded NUL byte
and corrupt non-text bytes. SQLite uses length-based `sqlite3_bind_text` and
preserves binary exactly, so the two backends are inconsistent for binary payloads.
The verifier notes `server_signing_keys.secret_key` is base64 text and not
affected, but `media_blobs.bytes` is raw binary.

#### Evidence

- `src/database/postgresql_store.cpp:233-243`: builds `parameter_values` from
  `parameter.value.c_str()` and calls `PQexecParams(..., nullptr, nullptr, 0)`.
- `migrations/001_initial_schema.sql:67`: `media_blobs.bytes BLOB`.

#### Recommended fix

Pass parameter lengths and use binary format (`format=1`) for `BYTEA` parameters,
or at minimum length-prefixed text, so NUL bytes and arbitrary binary round-trip
identically on PostgreSQL. Introduce a parameter metadata flag that distinguishes
text from binary columns.

#### Acceptance criteria

- **GIVEN** a media upload containing embedded NUL bytes (e.g. a crafted PNG or
  arbitrary binary),
  **WHEN** it is stored via the PostgreSQL backend and read back,
  **THEN** the bytes round-trip exactly, including after NULs.
- **GIVEN** the same upload via the SQLite backend,
  **WHEN** it is read back,
  **THEN** both backends return identical bytes.

#### Suggested test approach

Extend `tests/unit/test_database_persistence.cpp` with a PostgreSQL-specific
binary round-trip test for `media_blobs.bytes`. Include a payload with NULs and
high bytes. Assert byte-for-byte equality after read-back.

---

### M-10 — No cross-process lock prevents concurrent migration runs

- **Surface:** Database persistence
- **Location:** `src/database/sqlite_store.cpp:323-387`
  (PostgreSQL equivalent at `src/database/postgresql_store.cpp:1205-1237`)
- **Severity:** medium
- **Verifier confidence:** medium
- **Project rule:** `migrations/AGENTS.md:31-35`: migrations run automatically,
  idempotent, in order.

#### Description

`apply_pending_migrations` in both backends executes the migration plan step by
step, with each step in its own transaction, but neither acquires a process-level
lock or lease that spans the entire plan. If two server instances start
simultaneously against the same database, their DDL steps can race, duplicate
ledger inserts, or fail mid-chain and leave the schema at an intermediate version.

#### Evidence

- `src/database/sqlite_store.cpp:323-387`: loops over `plan.steps` and commits each
  step independently.
- `src/database/postgresql_store.cpp:1205-1237`: does the same via
  `connection.execute_transaction`.

#### Recommended fix

Serialize migration execution across processes using a backend-specific lease:
- PostgreSQL: `pg_advisory_lock` for the entire plan.
- SQLite: a dedicated `migration_lock` row with `BEGIN IMMEDIATE`, or exclusive
  `PRAGMA locking_mode=EXCLUSIVE` during migration.

Release the lease only after the entire plan is complete and the target version
is recorded.

#### Acceptance criteria

- **GIVEN** two server processes start against the same database at the same time,
  **WHEN** migrations need to run,
  **THEN** one process runs the plan to completion while the other waits, and the
  schema ends at the target version with no duplicate ledger entries.
- **GIVEN** a migration process that crashes mid-plan,
  **WHEN** a new process starts,
  **THEN** it detects the incomplete plan and either resumes or fails safely with a
  clear error.

#### Suggested test approach

Integration test: start two migration runners simultaneously against a
PostgreSQL/SQLite database one version behind. Assert that one waits and the
schema version ends correct. Add a negative test where a lock-holder process is
killed mid-plan and the next runner handles the leftover state.

---

### M-11 — Legacy `LOG_*` and `LOGF_*` macros bypass structured redaction helpers

- **Surface:** Observability
- **Location:** `include/merovingian/observability/logger.hpp:733`
- **Severity:** medium (downgraded from high; no active secret leak proven)
- **Verifier confidence:** high
- **Project rule:** `src/observability/AGENTS.md`: "Never log secret material.";
  `docs/security-coding-rules.md`: logging paths must not bypass redaction.

#### Description

The `LOG_*` macros pass a caller-built `std::string` directly to `SingleLog`, and
`LOGF_*` macros build the string via `string_format`. Neither path calls
`redact_log_value` or `contains_sensitive_marker`, so they bypass the only
secret-redaction boundary that structured diagnostics use. The verifier notes the
cited call sites log file paths, not secret values, so no active secret leak is
proven, but the enforcement gap is real.

#### Evidence

- `include/merovingian/observability/logger.hpp:733`: macro definitions bypass
  redaction.
- `src/federation_worker/main.cpp` and `worker_event_loop.cpp` use these macros
  for file paths.

#### Recommended fix

Route all legacy macros through the structured redaction helpers, or deprecate
them in favor of `log_diagnostic` / `StructuredLogField`. If kept, add a
compile-time or static-analysis check that no `LOGF_*` call site formats a value
derived from user input or secret storage.

#### Acceptance criteria

- **GIVEN** a call site using `LOG_*` or `LOGF_*` with a string containing a secret
  marker (e.g. "access_token=..."),
  **WHEN** the macro is expanded,
  **THEN** the secret portion is redacted before reaching the console/file queue.
- **GIVEN** a static-analysis or build check,
  **WHEN** it scans `src/`,
  **THEN** it flags any new `LOGF_*` call site that formats a non-literal string.

#### Suggested test approach

Unit test: define a test sink, call `LOG_INFO` with a string containing a known
secret marker, and assert the sink receives a redacted version. If the macros are
deprecated, add a lint rule that fails CI for new usages outside of test code.

---

## Low-severity / residual findings (14)

### L-01 — Registration UIAA uses a hardcoded session ID and ignores the configured stage list

- **Surface:** Authentication & session management
- **Location:** `src/homeserver/client_server.cpp:9322`
- **Severity:** low (downgraded from medium; only one stage is implemented today)
- **Verifier confidence:** high
- **Spec reference:** Matrix v1.19 Client-Server API §User-Interactive Authentication
  API.

#### Description

When `require_token` is true, the registration endpoint returns a static UIAA
challenge with session id `merovingian-ui-auth` and a single hardcoded flow
containing only `m.login.registration_token`. It does not consult the configured
UIAA stage list, does not create an entry in `auth_sessions`, does not track
completed stages, and does not verify the client-supplied session id. The verifier
notes there is no configured UIAA stage list in the codebase today, so the
practical bypass does not exist yet, but the design is not spec-compliant.

#### Recommended fix

Generate a unique session id per registration attempt, store it in `auth_sessions`,
and track completed stages. When additional UIAA stages are added, consult the
configured stage list and enforce stage order.

#### Acceptance criteria

- **GIVEN** a registration requiring UIAA,
  **WHEN** the server sends a challenge,
  **THEN** the session id is unique and a session row exists.
- **GIVEN** a client submits an auth block,
  **WHEN** the server validates it,
  **THEN** it checks the session id matches and the completed stages follow a configured
  flow.

#### Suggested test approach

Add a conformance test for the registration UIAA flow asserting a unique session
id and session persistence. Add a test that submits a mismatched session id and
expects `401`.

---

### L-02 — Device-deletion UIAA uses hardcoded session IDs and lacks server-side stage tracking

- **Surface:** Authentication & session management
- **Location:** `src/homeserver/client_server.cpp:2759`
- **Severity:** low (downgraded from medium; only password stage used)
- **Verifier confidence:** high
- **Spec reference:** Matrix v1.19 Client-Server API §User-Interactive Authentication
  API.

#### Description

Both device-deletion endpoints build their UIAA challenge via
`device_delete_uia_challenge` with hardcoded session ids (`delete_device` /
`delete_devices`). The server does not generate unique per-attempt sessions, does
not record them in `auth_sessions`, and does not track completed stages. The
verifier notes the replay claim is misleading because the auth block contains the
user's password, but the hardcoded session id is still not spec-compliant.

#### Recommended fix

Use unique session ids and server-side stage tracking for device deletion, same as
recommended for registration UIAA (L-01).

#### Acceptance criteria

- **GIVEN** a device-deletion request,
  **WHEN** the server issues a UIAA challenge,
  **THEN** the session id is unique per request and stored server-side.
- **GIVEN** two concurrent deletion requests from the same user,
  **WHEN** each completes one stage,
  **THEN** the server tracks them independently.

#### Suggested test approach

Add a unit test that calls the device-deletion challenge endpoint twice and
asserts different session ids. Add a test that attempts to complete a stage with a
mismatched session and expects failure.

---

### L-03 — Device ID validator permits colon, a reserved separator in Matrix key identifiers

- **Surface:** Authentication & session management
- **Location:** `src/auth/identity.cpp:363`
- **Severity:** low
- **Verifier confidence:** high
- **Spec reference:** Matrix v1.19 Client-Server API §End-to-end encryption / Key
  upload; Appendices §Identifier Grammar.

#### Description

`device_id_is_valid` accepts any printable ASCII character except space, including
`:`. Matrix key identifiers use `<algorithm>:<device_id>` (e.g. `ed25519:<device_id>`),
so a device ID containing `:` produces an ambiguous key ID such as
`ed25519:foo:bar`. This could break key parsing, cross-signing, and device-key
matching elsewhere and in federation partners.

#### Recommended fix

Restrict device IDs to the unreserved character set allowed by the Matrix
identifier grammar, excluding `:` explicitly. Update `device_id_is_valid` and add
a conformance test.

#### Acceptance criteria

- **GIVEN** a device ID containing `:` or other reserved characters,
  **WHEN** it is validated,
  **THEN** it is rejected.
- **GIVEN** a device ID containing only allowed characters,
  **WHEN** it is validated,
  **THEN** it is accepted.

#### Suggested test approach

Extend `tests/unit/test_auth_identity.cpp` (or the relevant identity test) with
positive and negative device-id cases, including `:`, `/`, `?`, `#`, and spaces.

---

### L-04 — TLS cipher list allows non-forward-secret suites

- **Surface:** Client-server HTTP
- **Location:** `src/homeserver/tls.cpp:232`
- **Severity:** low
- **Verifier confidence:** certain
- **Project rule:** `docs/http-transport.md` TLS listener boundary.

#### Description

`SSL_CTX_set_cipher_list` is configured with `HIGH:!aNULL:!MD5:!RC4:!3DES`. This
excludes anonymous and broken ciphers but still permits RSA key exchange (no
forward secrecy) and CBC/HMAC suites. A passive attacker who later compromises the
server's RSA private key can retrospectively decrypt recorded client-server
traffic. The verifier notes TLS 1.3 is enabled and modern clients will prefer
forward-secret suites, so severity is low.

#### Recommended fix

Restrict the cipher list to authenticated ephemeral key-exchange suites, e.g.:
`ECDHE+AESGCM:ECDHE+CHACHA20:DHE+AESGCM:!aNULL:!MD5:!RC4:!3DES:!RSA:!SHA1`.
Prefer AEAD-only ciphers.

#### Acceptance criteria

- **GIVEN** the server's cipher list,
  **WHEN** queried with `openssl ciphers`,
  **THEN** no plain-RSA or non-forward-secret suite is listed.
- **GIVEN** a client that only supports plain RSA,
  **WHEN** it connects,
  **THEN** the handshake fails.

#### Suggested test approach

Add a unit or integration test that loads the TLS context and asserts the
effective cipher list excludes `TLS_RSA_*` and includes only `TLS_ECDHE_*` /
`TLS_DHE_*` AEAD suites.

---

### L-05 — CORS wildcard origin can be emitted together with `Access-Control-Allow-Credentials`

- **Surface:** Client-server HTTP
- **Location:** `src/homeserver/client_server.cpp:161-163` and `235-237`
- **Severity:** low
- **Project rule:** W3C CORS specification; Matrix v1.19 Client-Server API §10.5.

#### Description

`resolve_allow_origin` returns `*` when the operator allow-list contains `*`.
`apply_cors_headers` then unconditionally adds `Access-Control-Allow-Credentials: true`
if `cors.allow_credentials` is true. The CORS specification forbids combining a
wildcard `Access-Control-Allow-Origin` with credentials; compliant browsers reject
such responses, but non-browser or misbehaving clients could be coerced into
sending cookies/tokens to a malicious origin.

#### Recommended fix

Refuse to set `Access-Control-Allow-Credentials` when the resolved origin is `*`,
or reject wildcard-plus-credentials at configuration-validation time.

#### Acceptance criteria

- **GIVEN** `cors.allow_origin` includes `*` and `cors.allow_credentials` is true,
  **WHEN** a request with an `Origin` header is handled,
  **THEN** the response either refuses the wildcard or omits
  `Access-Control-Allow-Credentials`.

#### Suggested test approach

Unit test `apply_cors_headers` with wildcard origin and credentials enabled;
assert the credentials header is absent.

---

### L-06 — Backfill query parser accepts unbounded `limit` and forwards it to the provider

- **Surface:** Federation security
- **Location:** `src/federation/membership_endpoints.cpp:213`
- **Severity:** low
- **Verifier confidence:** certain
- **Spec reference:** Matrix v1.19 Server-Server API §Backfilling and retrieving
  missing events.

#### Description

`parse_backfill_query` parses the `limit` query parameter with `std::strtoull`,
validates it is a clean decimal string, and casts it to `std::size_t`, but never
enforces a maximum before passing the value to the injected `backfill_provider`. If
that provider does not impose its own cap, a remote can request an arbitrarily
large backfill.

#### Recommended fix

Clamp the parsed limit to a reasonable maximum (e.g. the Matrix default of 100 or a
configured limit) before invoking the provider. Return `400 M_INVALID_PARAM` if the
client requests more.

#### Acceptance criteria

- **GIVEN** a backfill request with `limit=999999`,
  **WHEN** it is parsed,
  **THEN** the server returns `400 M_INVALID_PARAM` or clamps to the configured max.
- **GIVEN** a backfill request with `limit=50`,
  **WHEN** it is parsed,
  **THEN** it is accepted.

#### Suggested test approach

Add unit tests for `parse_backfill_query` with zero, valid, and oversized limits.
Add an integration test that asserts the response code for an oversized limit.

---

### L-07 — `RuntimeEd25519Provider` constructor accepts any-size `SecretBuffer`

- **Surface:** Cryptographic boundary
- **Location:** `src/crypto/runtime_ed25519_provider.cpp:13-16`
- **Severity:** low (linked to M-07; construction alone is not a read primitive)
- **Verifier confidence:** certain
- **Project rule:** `src/crypto/AGENTS.md` rule 3 (fail closed) and rule 4 (validate
  key material).

#### Description

The constructor simply moves the supplied `core::SecretBuffer` into `secret_key_`
without asserting it equals `ed25519_secret_key_bytes` (64 bytes). This is the
trust boundary where forgery-capable secret material enters the provider.

#### Recommended fix

Validate the `SecretBuffer` size at construction time and fail closed (throw or
set an error state) if it is not exactly 64 bytes. This closes the root cause of
M-07.

#### Acceptance criteria

- **GIVEN** a SecretBuffer whose size is not 64 bytes,
  **WHEN** `RuntimeEd25519Provider` is constructed,
  **THEN** construction fails cleanly.

#### Suggested test approach

Unit test construction with wrong-sized secrets; assert exception or error state.

---

### L-08 — `db-migrate` CLI silently treats invalid version arguments as a no-op plan

- **Surface:** Database migrations / operator tooling
- **Location:** `src/db_migrate.cpp:79`
- **Severity:** low
- **Verifier confidence:** high

#### Description

`parse_u32` returns 0 for any non-numeric argument and for an empty string, and
`main` never re-validates the parsed values. Running e.g.
`merovingian-db-migrate --plan abc def` builds a `current=0 target=0` plan with no
steps and exits 0, misleading an operator.

#### Recommended fix

Validate that `argv[2]` and `argv[3]` are non-empty decimal integers within the
supported version range, and return a non-zero exit code with a clear error
message when they are not.

#### Acceptance criteria

- **GIVEN** invalid version arguments,
  **WHEN** the CLI runs,
  **THEN** it exits non-zero with a descriptive error.

#### Suggested test approach

Add unit tests for `parse_u32` and for the CLI argument validation. Add an
integration test running the binary with invalid arguments.

---

### L-09 — Audit sink function pointer is read and written without synchronization

- **Surface:** Observability
- **Location:** `include/merovingian/observability/logger.hpp:703`
- **Severity:** low (downgraded from high; bounded by issue #420 fix)
- **Verifier confidence:** high
- **Project rule:** RAII / thread-safety is non-negotiable.

#### Description

The audit sink is stored as a plain function pointer returned by reference from
`the_audit_sink()`. `set_audit_sink()` writes to it during thread startup, while
`log_diagnostic_audit()` reads it from arbitrary threads. There is no
`std::atomic`, mutex, or happens-before edge. The verifier notes the write occurs
inside a thread-safe static-local initializer and issue #420 installs the
thread-local database pointer before work begins, so the practical race window is
bounded.

#### Recommended fix

Protect the audit sink pointer with `std::atomic` or a mutex. Ensure every
production thread installs the sink before dequeuing work and that the read path
sees the installed sink.

#### Acceptance criteria

- **GIVEN** multiple threads installing and reading the audit sink,
  **WHEN** TSan or equivalent runs,
  **THEN** no data race is reported.

#### Suggested test approach

Add a thread-safety test that calls `set_audit_sink` and `log_diagnostic_audit`
from multiple threads under a sanitizer build.

---

### L-10 — Audit events silently no-op before the sink is installed and when the thread-local database is null/closed

- **Surface:** Observability
- **Location:** `src/homeserver/local_services.cpp:46`
- **Severity:** low
- **Verifier confidence:** high
- **Project rule:** `src/observability/AGENTS.md`: security-relevant events must be
  durable audit rows.

#### Description

The default audit sink is a no-op, and `local_audit_sink` returns early if the
thread-local `LocalDatabase` pointer is `nullptr` or `!database->opened`. The sink
is installed lazily by `install_local_audit_database()`. Audit events that occur
before installation or after teardown are silently dropped. The verifier notes this
is intentional, documented, and bounded by issue #420.

#### Recommended fix

Make audit persistence fail-closed: either block diagnostic emission until the
audit sink is installed, or queue and flush missed audit events after
installation. If queuing, bound the queue and emit a warning if events are lost.

#### Acceptance criteria

- **GIVEN** an audit event emitted before the sink is installed,
  **WHEN** the sink is later installed,
  **THEN** the event is either queued and flushed, or a warning is emitted that it
  was dropped.

#### Suggested test approach

Add a unit test that emits an audit event before installing the sink, then
installs the sink and asserts the event is delivered or a drop warning is recorded.

---

### L-11 — `AuditLogEvent.append_only` flag is set but never enforced

- **Surface:** Observability
- **Location:** `include/merovingian/observability/observability.hpp:84`
- **Severity:** low
- **Verifier confidence:** certain
- **Project rule:** `docs/observability-audit.md` durable audit rows and tamper-evidence.

#### Description

`AuditLogEvent` carries an `append_only` field initialized to `true` in
`make_audit_event()`, but `audit_log_insert_statement()` and `append_local_audit()`
ignore it. Nothing checks this flag to reject updates/deletes or provide tamper
evidence.

#### Recommended fix

Enforce `append_only` at the persistence layer (e.g. INSERT-only policy, trigger,
or application-level guard) or remove the misleading field.

#### Acceptance criteria

- **GIVEN** an attempt to UPDATE or DELETE an audit_log row,
  **WHEN** the application or database policy runs,
  **THEN** the operation is rejected.

#### Suggested test approach

Add a database test that attempts to update/delete an audit row and asserts
failure.

---

### L-12 — `string_format` helper exposes a latent type-unsafe / format-string API

- **Surface:** Observability
- **Location:** `include/merovingian/observability/logger.hpp:563`
- **Severity:** low (downgraded from medium; unused in `src/` today)
- **Verifier confidence:** high
- **Project rule:** `docs/security-coding-rules.md`: log lines must be structured
  and bounded; CWE-134.

#### Description

`string_format` forwards a `std::string` format argument and variadic args to
`std::snprintf`. The API accepts any `std::string` as the format and does not
constrain it to string literals. The verifier notes the helper and `LOGF_*` macros
are unused in `src/` today, so exploitability is zero, but the latent surface
remains.

#### Recommended fix

Restrict `string_format` to internal/constexpr use, replace it with a type-safe
formatting facility, or delete the unused `LOGF_*` macros. If kept, mark it
`[[deprecated]]` and add a static-analysis rule preventing new usages.

#### Acceptance criteria

- **GIVEN** a scan of `src/`,
  **WHEN** it looks for `string_format` or `LOGF_*` usages,
  **THEN** none exist outside of tests/legacy helpers.

#### Suggested test approach

Add a lint step in CI that fails if `string_format` or `LOGF_*` appears in new
`src/` code.

---

### L-13 — Bounded log queues silently drop messages when full

- **Surface:** Observability
- **Location:** `include/merovingian/observability/logger.hpp:333`
- **Severity:** low
- **Verifier confidence:** high
- **Project rule:** `docs/security-coding-rules.md`: logging must not allocate
  unbounded attacker-controlled memory.

#### Description

`console_log()` and `file_log()` drop messages without notification once their
queues reach `max_log_queue_size` (4096). This is intentional backpressure, but an
operator cannot tell that messages were lost during a flood.

#### Recommended fix

Add a dropped-message counter exposed via metrics/health, and emit a single warning
when messages are discarded.

#### Acceptance criteria

- **GIVEN** a flood of log messages that exceeds the queue bound,
  **WHEN** the queue drops messages,
  **THEN** a counter is incremented and a single diagnostic warning is emitted.

#### Suggested test approach

Unit test: fill the log queue and assert the drop counter increases and a warning
is emitted exactly once.

---

### L-14 — Signing-key ID validator does not enforce the `ed25519:` algorithm prefix

- **Surface:** Event engine
- **Location:** `src/events/event_signer.cpp:190-194`
- **Severity:** low
- **Verifier confidence:** certain
- **Spec reference:** Matrix v1.19 Appendices §Cryptographic key representation.
- **Project rule:** `docs/security-coding-rules.md`: validate external key material.

#### Description

`signing_key_id_is_valid()` returns true for any non-empty, printable key id string
and does not require the `ed25519:` prefix. The crypto boundary already provides
`crypto::ed25519_key_id_is_valid()`, which enforces the prefix and printable
constraints.

#### Recommended fix

Delegate key ID validation to `crypto::ed25519_key_id_is_valid()` or enforce the
`ed25519:` prefix and shape locally.

#### Acceptance criteria

- **GIVEN** a key id without the `ed25519:` prefix,
  **WHEN** `signing_key_id_is_valid` is called,
  **THEN** it returns false.

#### Suggested test approach

Add unit tests in `tests/unit/test_event_signer.cpp` with positive and negative
key id cases.

---

### L-15 — `make_content_hash_id` hardcodes room version 12 policy

- **Surface:** Event engine
- **Location:** `src/events/event_id.cpp:112-117`
- **Severity:** low
- **Verifier confidence:** certain
- **Spec reference:** Matrix v1.19 `rooms/v4.md` §Event IDs.
- **Project rule:** `docs/security-coding-rules.md`: `RoomVersionPolicy` is the
  single authoritative source; do not hard-code version-specific logic.

#### Description

`make_content_hash_id()` unconditionally calls `rooms::find_room_version_policy("12")`
and then `make_reference_hash_event_id(event, *policy)`, ignoring the event's
actual room version. The verifier notes no production code in `src/` calls this
function; it is only used in tests. However, as public API it is a real latent
defect.

#### Recommended fix

Require or accept the event's actual `RoomVersionPolicy` and compute the reference
hash with that policy.

#### Acceptance criteria

- **GIVEN** an event whose room version is not 12,
  **WHEN** `make_content_hash_id` is called,
  **THEN** it uses the event's room version policy, not v12.

#### Suggested test approach

Update `tests/conformance/test_events.cpp` determinism check to cover multiple
room versions.

---

## Refuted or residualized findings (not to be fixed as stated)

The following findings did not survive adversarial verification. They are listed
here only so no one wastes time re-investigating them.

| # | Title | Why it did not survive |
|---|-------|------------------------|
| R-01 | State-resolution v2 mainline walk throws on missing auth-chain ancestor | Refuted: not reachable with valid federation input. |
| R-02 | EDU `room_id` ACL check may disagree with sink on duplicate JSON keys | Refuted: duplicate-key handling is consistent. |
| R-03 | OPTIONS preflight bypasses the rate limiter | Refuted: preflights are handled before routing by design; per-connection cost is bounded. |
| R-04 | Generic-hash piece framing is ambiguous under embedded NUL bytes | Latent only; current callers never pass NUL. |
| R-05 | Broad key-ID character set in `ed25519_key_id_is_valid` | Project decision, documented. |
| R-06 | Migration downgrade catalog drops tables/columns | Refuted: requires explicit operator opt-in in production tooling. |
| R-07 | Generic-hash NUL ambiguity (duplicate) | Same as R-04. |

---

## Checklist for the fixing LLM

Use this to track progress. Do not mark an item done until the new test for that
issue passes and the fix has been reviewed against the spec.

### High severity

- [ ] H-01 `/refresh` respects locked/suspended account state
- [ ] H-02 `/refresh` verifies the target device still exists
- [ ] H-03 Plain-HTTP sockets are non-blocking on send
- [ ] H-04 Per-PDU failures persist consecutive-failures count
- [ ] H-05 Event signer no longer logs signing payloads / signed JSON
- [ ] H-06 V2 state resolution populates `authorising_user_member`
- [ ] H-07 V2 state resolution parses string-encoded power levels in v1–v9

### Medium severity

- [ ] M-01 Registration token file cache invalidated on rotation
- [ ] M-02 Rate-limit buckets use collision-resistant hashing
- [ ] M-03 Transport-layer errors include CORS headers
- [ ] M-04 v1 invite uses correct room version for signature/hash
- [ ] M-05 `signature_verified` flag removed from public verifier
- [ ] M-06 Literal discovery network overload removed from public API
- [ ] M-07 `RuntimeEd25519Provider::sign` validates secret size
- [ ] M-08 `account_threepids` secrets bound as sensitive / encrypted
- [ ] M-09 PostgreSQL binary columns round-trip byte-exactly
- [ ] M-10 Migration runner holds a cross-process lock
- [ ] M-11 Legacy log macros route through redaction

### Low severity

- [ ] L-01 Registration UIAA uses unique sessions and stage tracking
- [ ] L-02 Device-deletion UIAA uses unique sessions and stage tracking
- [ ] L-03 Device ID validator rejects reserved separator characters
- [ ] L-04 TLS cipher list is forward-secret only
- [ ] L-05 CORS wildcard origin never paired with credentials
- [ ] L-06 Backfill `limit` is clamped / rejected when too large
- [ ] L-07 `RuntimeEd25519Provider` constructor validates secret size
- [ ] L-08 `db-migrate` CLI rejects invalid version arguments
- [ ] L-09 Audit sink pointer is synchronized
- [ ] L-10 Audit events are queued or warned when dropped
- [ ] L-11 `append_only` is enforced or removed
- [ ] L-12 `string_format` / `LOGF_*` are deprecated or removed
- [ ] L-13 Dropped log messages are counted and warned
- [ ] L-14 Signing key id validator delegates to crypto boundary
- [ ] L-15 `make_content_hash_id` accepts the event's room version

---

## Audit artifacts

- Workflow run ID: `wf_ca62d200-01b`
- Machine-readable result: `C:\Users\retro\AppData\Local\Temp\claude\C--dev-Merovingian\de182d62-7fbb-4079-b2d2-b5e8e305c8e6\tasks\wd42c92ew.output`
