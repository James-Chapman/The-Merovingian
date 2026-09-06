# Parse appservice registrations as a bounded YAML subset

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

The module parsed application-service registration files as JSON only,
reasoning that JSON is a subset of YAML 1.2. That is true, and it was not
sufficient: the spec's own example and every shipped bridge use block-style
`registration.yaml`, so the server could load no real bridge. That was a live
bug.

## Considered Options

* A strict, bounded YAML subset written in-tree, tried first with a JSON
  fallback
* A full YAML parsing dependency

## Decision Outcome

Chosen option: "A strict, bounded YAML subset". `registration_yaml.cpp`
accepts 2-space-indented block maps and lists, plain and quoted scalars, `#`
comments, and `null`/`~`. Anchors, aliases, tags, block scalars, tabs, document
markers and unknown keys are **parse failures, never silently coerced**.
`load_registration_file()` tries YAML first and falls back to JSON.

Bounds fail closed: 256 KiB file, 4 KiB line, 64 entries per namespace list, 32
protocols.

### Positive Consequences

* No general YAML parser in the dependency set, and no YAML feature surface
  (aliases, merge keys) reachable from an operator-authored file.
* Booleans are `true`/`false` only; the YAML 1.1 spellings `yes/no/on/off` are
  rejected because real YAML parsers disagree about them.

### Negative Consequences

* A registration file that a general YAML parser accepts may be rejected here.
  Only the empty flow sequence `[]` is accepted — the spec's own spelling for
  an absent namespace — and a non-empty flow sequence is rejected rather than
  parsed partially or dropped silently.

## Links

* [`src/appservice/AGENTS.md`](../../src/appservice/AGENTS.md)
* `CHANGELOG.md`, 0.12.1

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
