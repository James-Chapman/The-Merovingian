# Use real dependencies in integration tests

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

Integration tests verify behaviour across module boundaries — auth plus
database plus HTTP. Those boundaries can be exercised against mocks or against
the real components.

## Considered Options

* No mocks: a real in-process SQLite store, or a PostgreSQL test database
* Mock the store and the transport

## Decision Outcome

Chosen option: "No mocks". A mock encodes the author's belief about how the
dependency behaves, so a test built on one passes precisely when that belief is
self-consistent — including when it is wrong.

### Negative Consequences

* Integration tests are slower and need a PostgreSQL instance for the
  `postgres-integration` job.
* Test isolation is the author's responsibility rather than a property of the
  fixture.

### Positive Consequences

* Fuzz targets take the complementary rule for the same reason: they must
  **not** assert a specific output, because fuzzing is about stability rather
  than correctness. Between them, the two rules keep each test type honest
  about what it can actually prove.

## Links

* [`tests/integration/AGENTS.md`](../../tests/integration/AGENTS.md)
* [`tests/fuzz/AGENTS.md`](../../tests/fuzz/AGENTS.md)

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
