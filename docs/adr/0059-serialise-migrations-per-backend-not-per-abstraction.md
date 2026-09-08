# Serialise migrations per backend, not per abstraction

* Status: accepted
* Date: 2026-09-08

Technical Story: security audit of 0.12.8, finding M-10.

## Context and Problem Statement

`apply_pending_migrations` runs a migration plan step by step, each step in its
own transaction, in both backends. Neither took any lock spanning the whole
plan. Two server processes starting simultaneously against the same database
could therefore interleave their DDL: a step applied twice, duplicate ledger
inserts, or a chain that fails half-way and strands the schema at an
intermediate version.

The window that needs protecting is *between* the per-step transactions, so a
transaction-scoped lock does not help. The obvious move — one shared locking
mechanism behind the store abstraction — does not survive contact with the two
backends.

## Decision Drivers

* PostgreSQL is the production backend; SQLite is documented in
  [`src/database/AGENTS.md`](../../src/database/AGENTS.md) as development and
  single-node.
* [`migrations/AGENTS.md`](../../migrations/AGENTS.md) requires the runner to be
  idempotent and ordered.
* A migrator that crashes mid-plan must leave the database recoverable by
  restarting it, not by an operator clearing a lock by hand.
* SQLite already takes a cross-process `RESERVED` lock for each step's
  `BEGIN IMMEDIATE`. Any plan-wide SQLite lock has to coexist with those.

## Considered Options

* One mechanism behind the abstraction: a `migration_lock` row claimed for the
  duration of the plan in both backends.
* PostgreSQL `pg_advisory_lock` for the plan; SQLite `PRAGMA locking_mode =
  EXCLUSIVE` for the plan.
* PostgreSQL `pg_advisory_lock` for the plan; SQLite re-checks the ledger inside
  each step's existing `BEGIN IMMEDIATE` and skips a step already applied.

## Decision Outcome

Chosen option: **the third — a real plan-wide lock on PostgreSQL, and
in-transaction idempotence on SQLite.**

PostgreSQL takes `pg_advisory_lock` before the first step and releases it after
the plan completes, held by an RAII lease so every early return in the plan loop
releases it. The lock is session-scoped, so a crashed migrator's lock disappears
when its connection does — recovery needs no operator action. Failing to acquire
it fails the migration closed rather than proceeding unserialised.

SQLite gets no plan-wide lock. A second connection holding `BEGIN IMMEDIATE`
open for the duration of the plan would block the migrating process's *own*
per-step `BEGIN IMMEDIATE` — a self-deadlock. `PRAGMA locking_mode = EXCLUSIVE`
holds the file lock until the connection closes or the mode is reset, and the
migrating connection goes on to become the runtime connection, so it would lock
out the whole server. Instead, each step re-reads the `schema_migrations` ledger
*inside* its own `BEGIN IMMEDIATE` and skips a step the ledger already records.
`BEGIN IMMEDIATE` serialises the check against the apply, so the loser of a race
observes the winner's row and skips rather than re-running DDL that would fail.

**The rule this sets for future code: do not "unify" this behind the store
abstraction. The two backends are not solving the same problem with different
syntax — PostgreSQL is excluding a concurrent migrator, SQLite is tolerating
one. A shared lock-row abstraction would give SQLite a lease it cannot hold
without deadlocking against its own step transactions.**

### Positive Consequences

* Two production servers starting together no longer race DDL; one waits.
* A crashed PostgreSQL migrator recovers on the next start with no operator step.
* SQLite's runner is genuinely idempotent under concurrency, which is what
  `migrations/AGENTS.md` asks for, without adding a lock it cannot hold.

### Negative Consequences

* Two different mechanisms to understand, and only the PostgreSQL one is an
  actual mutual exclusion. On SQLite, two concurrent migrators both make
  progress; they simply cannot corrupt each other.
* The advisory-lock key is a fixed constant
  (`postgresql_migration_lock_key`). Any other application using the same
  advisory-lock key space on the same database would contend with it.

## Links

* Refines the migration rules in [`migrations/AGENTS.md`](../../migrations/AGENTS.md).
