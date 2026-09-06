# Detect namespace conflicts by comparing pattern strings

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

At boot, two application-service registrations may claim overlapping exclusive
namespaces. Detecting that in general means deciding regex intersection, which
a boot-time check cannot do.

## Considered Options

* Compare pattern strings, catching only textually identical patterns
* Attempt real regex-intersection analysis

## Decision Outcome

Chosen option: "Compare pattern strings", accepting a known gap.

**A false conflict refusing to start a correct deployment would be worse than
the gap.** The check catches the realistic operator error: two bridges shipping
the same pattern.

### Negative Consequences

* **Two different regexes that happen to match overlapping identifiers are not
  reported.** This is a documented, accepted gap, not an oversight — it is
  recorded under "Known gaps" in the module's own guidance.

### Positive Consequences

* Where a conflict *is* found — duplicate id, duplicate `as_token`, or an
  identical namespace pattern — `validate_registrations()` fails the whole
  registration set closed, rather than resolving ambiguity by consultation
  order. Load order is not a security boundary.

## Links

* [`src/appservice/AGENTS.md`](../../src/appservice/AGENTS.md), "Known gaps"

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
