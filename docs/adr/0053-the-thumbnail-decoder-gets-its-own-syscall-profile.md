# The thumbnail decoder gets its own syscall profile

* Status: accepted
* Date: 2026-09-06

Technical Story: security audit of 0.12.6, finding M-08.

## Context and Problem Statement

The thumbnail worker exists to run libpng and libjpeg-turbo over attacker-supplied
image bytes in a separate process, so that a decoder memory-safety bug is
contained rather than fatal. `media/thumbnail_worker_main.cpp::harden()` confines
it on each platform: OpenBSD with `pledge("stdio")`, FreeBSD with `cap_enter()`.

Both of those grant the process no filesystem namespace, no sockets, and no
process creation — the worker keeps the descriptors it already holds and loses
everything else. On Linux, `harden()` called `platform::apply_seccomp_filter()`,
the *general server* allowlist, which permits `socket`, `connect`, `sendto`,
`open`/`openat`, `unlinkat`, `execve` and `clone`. On the primary production
platform the decoder was therefore effectively unconfined, while the file's own
comment claimed a decoder exploit "has nothing to reach for".

A second, stricter profile already existed: `apply_worker_seccomp_filter()`, added
for the federation worker. It is a natural thing to reach for, and it is the
wrong one — it removes only `execve`/`execveat` and still permits sockets,
`openat` and `clone`, because the federation worker legitimately needs network
and threads.

## Decision Drivers

* An isolation boundary that differs by platform is not a boundary; it is a
  boundary on the platforms nobody deploys and a comment on the one they do.
* The confinement a media decoder needs is materially narrower than the one any
  other worker needs, because a decoder's entire job is a pure function from
  bytes on stdin to bytes on stdout.
* Sandbox profiles are read far more often than they are written, and a profile
  named "worker" invites reuse by anything that happens to be a worker.

## Considered Options

* Call `apply_worker_seccomp_filter()` from the thumbnail worker.
* Add a third, decoder-specific profile whose allowlist is the seccomp
  equivalent of `pledge("stdio")`.
* Tighten `apply_worker_seccomp_filter()` itself and have both workers share it.

## Decision Outcome

Chosen option: **a third, decoder-specific profile**, added as
`platform::apply_decoder_seccomp_filter()` with the allowlist
`k_decoder_allowed_syscalls`, and called from `harden()` in place of
`apply_seccomp_filter()`.

The allowlist admits I/O on already-open descriptors, memory management, the
per-thread syscalls glibc issues from inside `malloc`, signal return, clock
reads, and exit. It denies every socket call, every path-based filesystem call,
`execve`/`execveat`, and `clone`/`clone3`/`fork`/`vfork`. That is the same
boundary `pledge("stdio")` draws, expressed in seccomp-bpf, so the three
platforms now confine the decoder equivalently.

**The rule this sets for future code: sandbox profiles are named for the threat
they contain, not for the kind of process that installs them.** A new isolated
child gets its own profile unless its syscall needs are genuinely identical to an
existing one. "It is also a worker" is not a reason to share `k_worker_*`.

### Positive Consequences

* A libpng or libjpeg-turbo compromise on Linux can no longer open files,
  connect out, exfiltrate media, or spawn a process — matching the containment
  the BSD branches already provided and the documentation already claimed.
* `tests/unit/test_seccomp_hardening.cpp` asserts the decoder profile is
  strictly tighter than the worker profile on sockets, `openat` and `clone`, so
  a later "simplification" onto the shared profile fails the suite rather than
  silently reopening this hole.

### Negative Consequences

* Three allowlists now need maintaining, and a syscall glibc starts issuing from
  `malloc` in a future release will kill the decoder under
  `SECCOMP_RET_KILL_PROCESS` rather than degrade. This is the fail-closed
  trade-off taken deliberately; `apply_decoder_seccomp_filter_with_default()`
  exists so a `SECCOMP_RET_TRAP` run can name the offending syscall instead of
  dying opaquely.
* The profile is unavoidably tied to glibc's internals (`rseq`, `membarrier`,
  `getcpu`). A different libc would need its own review.

## Pros and Cons of the Options

### Reuse `apply_worker_seccomp_filter()`

* Good, because it is a one-line change and adds no new allowlist.
* Bad, because it leaves `socket`, `connect`, `openat` and `clone` permitted,
  which is the substance of the finding — the fix would look complete while
  changing almost nothing that matters.
* Bad, because it entrenches the idea that "worker" is a security category.

### Decoder-specific profile

* Good, because it matches the `pledge("stdio")` boundary the design already
  committed to on the BSDs.
* Good, because the denied set is exactly the set a decoder can never need,
  which makes the profile easy to review against its purpose.
* Bad, because it is a third allowlist to keep current.

### Tighten the shared worker profile

* Good, because it would leave only two profiles.
* Bad, because the federation worker genuinely needs sockets and threads, so the
  shared profile could only ever be as loose as its loosest consumer — which is
  how the original problem arose.

## Links

* Implements the fix for security audit finding M-08 (0.12.7).
* Related: [ADR-0052](0052-revocation-of-credentials-is-one-way.md) — same audit,
  same theme of removing an unsafe-but-attractive reuse.
