# TLS sockets stay non-blocking for the life of the connection

* Status: accepted
* Date: 2026-09-06

Technical Story: security audit of 0.12.6, findings M-06 and M-07.

## Context and Problem Statement

`accept_tls_connection` sets the client socket non-blocking so the handshake can
be driven with `poll()` against a timeout, and on success restored the socket's
original flags — putting it back into blocking mode for the rest of the
connection. `TlsConnection::read` then called `SSL_read_ex` on a blocking socket.

The HTTP layer's `recv_with_timeout` guards every read with
`poll(POLLIN, receive_timeout_milliseconds)` and treats that as its timeout. For
a plaintext socket it is one. For a TLS socket it is not: `POLLIN` proves that
*TCP bytes* are available, never that a *complete TLS record* is. A peer that
sends the first byte of a record and then stops makes the socket readable, so
`poll` returns immediately, `SSL_read_ex` is entered, OpenSSL needs the rest of
the record, and the call blocks in the kernel — past the poll deadline, past the
request-head deadline, indefinitely. One connection, one worker thread, held for
as long as the attacker likes.

The related finding M-06 (no total deadline on request-body reads) is bounded by
the same mechanism, which is why the two were fixed together: a body deadline is
worthless while a single `SSL_read` beneath it can block forever.

## Decision Drivers

* Every timeout in the HTTP layer is expressed in terms of `poll()` on the raw
  descriptor. That abstraction is only sound if no call beneath it can block.
* The failure is silent and asymmetric: plaintext listeners behave correctly, so
  the bug is invisible in any test that does not use TLS.
* Thread-pool exhaustion by a single peer is a denial of service against the
  whole server, not a degraded connection.

## Considered Options

* Restore blocking mode after the handshake and set `SO_RCVTIMEO`/`SO_SNDTIMEO`
  on the socket.
* Keep the socket non-blocking for the connection's whole life and have
  `TlsConnection` drive `SSL_ERROR_WANT_READ`/`WANT_WRITE` against its own
  deadline.
* Leave the socket blocking and have the HTTP layer pre-read whole TLS records
  itself before calling into OpenSSL.

## Decision Outcome

Chosen option: **the socket stays non-blocking for the life of the connection.**
`accept_tls_connection` no longer calls `restore_flags` on the success path, the
handshake timeout is stored on the connection as `m_io_timeout_ms`, and both
`TlsConnection::read` and `TlsConnection::write` route through one private
`pump()` that retries the SSL call and, on `WANT_READ`/`WANT_WRITE`, polls for
readiness against that deadline — returning `-1` when it expires.

**The rule this sets for future code: nothing may put a TLS client socket back
into blocking mode, and no code below the HTTP layer may perform a blocking I/O
call on a connection descriptor.** The layer above expresses all of its timeouts
as `poll()` on the descriptor, and that is only a timeout if the call beneath it
cannot block. A future `restore_flags` on this path would reintroduce the bug
with no test failure on a plaintext-only suite.

Note also that `pump()` retries with the *same* buffer and length, as OpenSSL
requires after `WANT_READ`/`WANT_WRITE`; advancing the buffer across iterations
would be a protocol violation, not an optimisation.

### Positive Consequences

* A peer that opens a TLS record and stops now costs one bounded timeout instead
  of a permanently parked worker thread.
* The HTTP layer's deadlines — including the new request-body deadline from
  M-06 — mean what they say on TLS listeners as well as plaintext ones.
* `read` and `write` share one retry loop, so the two paths cannot drift.

### Negative Consequences

* `TlsConnection` now carries a timeout, so its I/O is no longer a thin pass-through
  to OpenSSL; a reader must know the deadline exists to reason about the return
  value. `-1` now means "error, clean shutdown, or timed out" without
  distinguishing them, which the HTTP layer treats identically but a future
  caller might not.
* The deadline is inherited from the handshake timeout rather than configured
  separately. That is adequate today and is the kind of thing that should become
  a distinct configuration key if the two ever need different values.

## Pros and Cons of the Options

### Blocking socket with `SO_RCVTIMEO`

* Good, because it is a small change and keeps `TlsConnection::read` a
  one-liner.
* Bad, because a socket timeout bounds one `recv` beneath OpenSSL, not the
  `SSL_read` call as a whole; OpenSSL may reissue and the total remains
  unbounded.
* Bad, because `SO_SNDTIMEO` interacts poorly with partial `SSL_write`, which
  must be retried with identical arguments.

### Non-blocking for the connection's life

* Good, because it makes the `poll()`-based timeout model that the rest of the
  server already assumes actually true.
* Good, because the deadline is enforced where the retry decision is made.
* Bad, because it adds a retry loop that must respect OpenSSL's same-arguments
  rule, which is easy to get subtly wrong.

### Pre-read whole records above OpenSSL

* Good, because it would need no change to the TLS layer.
* Bad, because it requires parsing TLS record framing outside the TLS library —
  duplicating protocol knowledge in the HTTP layer, which is precisely the sort
  of thing this codebase keeps behind the crypto boundary.

## Links

* Implements the fix for security audit findings M-06 and M-07 (0.12.7).
* Related: `docs/crypto-boundary.md` — OpenSSL protocol knowledge stays inside
  the TLS module.
