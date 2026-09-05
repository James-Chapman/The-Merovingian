# src/homeserver/ — Homeserver Orchestration

The top-level module that wires all services together and handles the client-server HTTP surface.
This is the largest and most complex module — read this file carefully before making changes here.

## Key files

| File | Responsibility |
|---|---|
| `client_server.cpp` | `handle_client_server_request()` — main dispatch for all `/_matrix/client/...` and `/_matrix/media/...` endpoints |
| `http_server.cpp` | Binds sockets, accepts connections, dispatches to federation or client-server handlers |
| `local_http_router.cpp` | In-process HTTP router for inter-module calls (media, auth, sync); uses pipe-delimited internal format |
| `local_services.cpp` | Wires local service instances (auth, media, sync, rooms) into the runtime |
| `runtime.cpp` | `Runtime` — holds all live service references; passed by reference to every handler |
| `auth_service.cpp` | Auth service entry point (delegates to `src/auth/`) |
| `media_service.cpp` | Media service entry point (delegates to `src/media/`) |
| `room_service.cpp` | Room service entry point (delegates to `src/rooms/`) |
| `tls.cpp` | TLS connection setup and cert loading |

## Architecture boundaries

```
Real HTTP client
    ↓ (raw bytes + headers)
handle_client_server_request()   ← client_server.cpp
    ↓ (internal pipe format: declared_mime|sniffed_mime|scanner_clean|bytes for media)
handle_local_http_request()      ← local_http_router.cpp
    ↓
Media / Auth / Sync / Room services
```

**Never** call `handle_local_http_request()` with a real client body. The internal format
is `declared_mime|sniffed_mime|scanner_clean|bytes` — the pipe-delimited wrapper is built by
`client_server.cpp` before the call.

## Adding a new client-server endpoint

1. Add the route match in `client_server.cpp` (find the block matching the path prefix)
2. Build the response using `dispatch_resp()` or `dispatch_err()`
3. Add a conformance test in `tests/conformance/test_client_server_conformance.cpp`
4. Add a unit test in `tests/unit/test_client_server.cpp`
5. Update `docs/matrix-v1.19-client-server-api.md` with the new endpoint

## The runtime lock and blocking calls

`handle_client_server_request` and `handle_local_http_request` each take
`HomeserverRuntime::mutex` for the whole request, and that same mutex guards
inbound federation handling. Anything holding it blocks every other client and
every inbound `/send`.

Never make a blocking network call while holding it. Wrap the call — and only
the call — in a `homeserver::RuntimeLockRelease` scope
(`merovingian/homeserver/request_lock.hpp`):

```cpp
auto const result = [&]() {
    auto const unlocked = RuntimeLockRelease{};   // runtime.mutex released here
    return runtime.outbound_client->perform(request);
}();                                          // and re-acquired here
```

Both entry points publish their guard through `homeserver::RequestLockScope`,
which is what the default-constructed form above finds. When the guard is in
hand instead — a service function that took its own — pass it:
`RuntimeLockRelease{guard}`. **Either form releases every recursion level this
thread holds**, so a self-locking service function called from a dispatcher
that already holds the mutex still frees it outright. That is not a nicety:
releasing a single level shipped as a server-wide stall three times
(`create_room` 0.12.1, `leave_room` 0.12.3, `invite_user_by_threepid` 0.12.6).

Keep every read and mutation of runtime state outside the scope. In particular
request signing stays under the lock, because `OutboundCall::secret_key` borrows
a span into the runtime's `SecretBuffer`.

When the released region produces values the code after it consumes, return
them from an immediately-invoked lambda — or a named function, as `join_room`
does with `perform_federated_join` — rather than declaring them above the
release. The scope boundary is then the lock boundary, and nothing has to be
default-constructed and assigned in order to survive it.

See [`docs/http-transport.md`](../../docs/http-transport.md) "Request lock and
blocking network calls".

## Body size limits

- **Default cap**: `rt.limits.max_body_bytes` (64 KiB) — applied at the top of the dispatch function
- **Media uploads**: bypass the default cap; use `config::parse_size_limit(rt.homeserver.config.security().media.max_upload_size)`
- Any new endpoint that accepts large bodies must explicitly opt out of the default cap

## Media upload boundary

`client_server.cpp` is responsible for:
1. Matching `/_matrix/media/v3/upload` and `/_matrix/client/v1/media/upload` (with and without `?filename=...`)
2. Extracting `Content-Type` header → `declared_mime`
3. Building `declared_mime|declared_mime|clean|<body>` before calling `call_local()`
4. Mapping internal `202` (quarantined) responses to `200` toward the client with `content_uri`

## Key docs

- `docs/http-transport.md` — HTTP handling, TLS, rate limiting
- `docs/media-repository.md` — media upload/download flow
- `docs/auth-identity.md` — token validation and session flow
