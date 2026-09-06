# Match appservice namespaces against whole identifiers

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

Application-service namespace patterns are POSIX ERE. A regex can be applied as
a whole-string match or as an unanchored substring search, and the choice
decides what an *exclusive* namespace actually claims.

## Considered Options

* `std::regex_match` — the pattern must match the whole identifier
* `std::regex_search` — the pattern may match any substring

## Decision Outcome

Chosen option: "`std::regex_match`, never `std::regex_search`". A namespace
regex is a claim over a whole identifier, not a substring.

Under an unanchored search, the spec's own example pattern `@_irc_.*` also
claims `@evil@_irc_bob:example.org` — letting one appservice's exclusive
namespace swallow unrelated local users and block their registration.

### Positive Consequences

* A malformed operator-supplied regex fails closed to "no match", never
  crashing and never matching everything.

## Links

* [`src/appservice/AGENTS.md`](../../src/appservice/AGENTS.md), "Namespaces"

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
