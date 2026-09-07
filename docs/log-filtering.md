# Operator log filtering

Configured log levels control which diagnostic messages can reach either
output sink. This applies to structured diagnostics, direct `SingleLog`
calls, and the `LOG_*`/`LOGF_*` macros. Audit persistence is independent
of diagnostic filtering.

## `--debug` and the default level

`--debug` lowers the **console sink** threshold to `debug`. It does not
override `log_modules` or change the module default, which is `info`.
A message must meet both its module threshold and its output sink threshold.
An explicit module setting overrides the wildcard default for that module.
`info` suppresses `trace` and `debug`, while retaining `info` and higher
severities, including warnings and errors. `off` suppresses all diagnostic
messages from the selected module.

To suppress debug messages even in a `--debug` run, set:

```conf
# Applies to every module without an explicit override
log_modules.*=info
```

## Silencing a noisy module

`http_server` is the loudest module in steady state — every request
gets a `request.received` line. To silence it while keeping `auth`
verbose:

```conf
log_modules.http_server=info
log_modules.auth=debug
```

Restart the server. The bootstrap runs once at `start_client_server`.

## Bumping a quiet module

To see why a specific 5xx is happening, raise a single module:

```conf
log_modules.client_server=debug
log_modules.rate_limit=debug
```

## Audit-routed failure lines

The following failure events are persisted to `audit_log`, including when
their diagnostic messages are filtered out:

| Logger | Audit event type | Audit category |
|--------|------------------|----------------|
| `rate_limit` | `rate_limit.exceeded` | `policy` |
| `auth` | `login.rejected` | `auth` |
| `auth` | `access_token.rejected` | `auth` |
| `client_server` | `request.rejected` | `policy` |
| `client_server` | `request.user_locked` | `auth` |
| `client_server` | `request.user_suspended` | `auth` |
| `auth` | `registration_policy.denied` | `policy` |

For a client HTTP 429, `rate_limit.exceeded` emits the warning with the
effective IP and cap details. The additional `request.rejected` audit record
is retained without a second diagnostic warning. Other request rejections
still emit their own warning. Rate-limit enforcement and retry responses
are unaffected by this diagnostic deduplication.

## Reading the audit log

```sh
# All rate-limit hits
curl 'http://127.0.0.1:8008/_merovingian/admin/audit?category=policy&event_type=rate_limit.exceeded'

# All login rejections (no category filter)
curl 'http://127.0.0.1:8008/_merovingian/admin/audit?event_type=login.rejected'

# All policy events
curl 'http://127.0.0.1:8008/_merovingian/admin/audit?category=policy'
```

A malformed `category=` value returns 400 with
`unknown audit category: <name>` so typos fail loud.

## Module names

The `log_modules.<name>` keys accept any string — the bootstrap
forwards the name to `SingleLog::set_module_log_level(name, level)`
without a registry. The runtime uses the following conventions:

| Module | What it logs |
|--------|--------------|
| `http_server` | Per-request line and per-request reject |
| `client_server` | Client-server request lifecycle |
| `auth` | Login, session, and access-token decisions |
| `rate_limit` | 429s and engine denials |
| `runtime` | Runtime startup, listener ready, database ready |
| `local_router` | Local HTTP router (audit, health, federation) |
| `dispatch` | Federation PDU/EDU dispatch |
| `migrate` | Database migration progress |

Unrecognised module names are accepted and the level is recorded —
there is no error. Restart the server to apply.

Legacy `LOG_*`/`LOGF_*` macros use the calling function name as their logger
name (for example, `handle` for the WorkerPool reply diagnostic). They obey
that name's explicit setting, or `log_modules.*` when none exists. Function
traces use `FunctionTrace`. Structured diagnostics use the named modules
in the table above.
