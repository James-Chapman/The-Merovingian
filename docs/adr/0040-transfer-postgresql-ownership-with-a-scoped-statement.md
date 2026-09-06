# Transfer PostgreSQL ownership with a scoped statement

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

Migrations must run as a role that owns the schema objects — PostgreSQL permits
`ALTER TABLE` only to a table's owner, and migrations 007 and 011 use it. An
existing database has those objects owned by the login role, so upgrading needs
an ownership transfer.

## Considered Options

* A scoped transfer of just the `public` schema's tables and sequences, in
  `provision-roles.sql`
* `REASSIGN OWNED`

## Decision Outcome

Chosen option: "A scoped transfer". `REASSIGN OWNED` operates on *everything*
the role owns, including pinned system objects, and so fails outright when the
login role is the cluster bootstrap superuser — which is exactly the common
single-role deployment being migrated from.

Relatedly, `open_postgresql_persistent_store()` **refuses to open** if the
migration role cannot be assumed, rather than falling back to the login role
and silently restoring full privileges.

### Negative Consequences

* **Breaking for existing databases**: one manual step (`provision-roles.sql`)
  is required before upgrading. The `postgres-integration` CI job runs the same
  statements, so the operator sequence is at least exercised.
* A misconfigured `migration_role` breaks an upgrade rather than a routine
  restart — the failure is loud and at the right moment, but it is a failure.
* Leaving `database.migration_role` unset keeps single-role deployments working
  unchanged.

## Links

* [`docs/database-persistence.md`](../database-persistence.md)
* `CHANGELOG.md`, 0.12.5

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
