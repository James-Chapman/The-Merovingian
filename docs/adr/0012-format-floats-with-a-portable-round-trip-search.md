# Format floats with a portable round-trip search

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

`serialize_canonical()` must emit a double that reads back as the same value.
An earlier version used fixed-precision `std::to_string` formatting, which does
not round-trip.

## Considered Options

* An escalating-precision `snprintf`/`strtod` round-trip search
* `std::to_chars`'s floating-point overload
* Fixed-precision `std::to_string` (the earlier behaviour)

## Decision Outcome

Chosen option: "escalating-precision `snprintf`/`strtod` round-trip search",
because **`std::to_chars`'s floating-point overload is not implemented on every
supported toolchain** — NetBSD's libstdc++ build ships only the integer
overloads. Correctness on Tier 1 BSD platforms outranks using the newer API.

### Negative Consequences

* **`snprintf` and `strtod` both consult `LC_NUMERIC`.** The formatter must
  normalize the active locale's decimal separator back to `.`, or float output
  stops being valid JSON the moment any library in the process calls
  `setlocale()` (#435). That is a real defect this approach introduced and now
  guards against; `std::to_chars` would not have had it.
* Slower than a single formatting call, since it may try several precisions.

### Positive Consequences

* Builds and behaves identically on every supported platform, which is the
  property `platform-support.md` exists to protect.

## Links

* [`docs/canonical-json.md`](../canonical-json.md), "Numeric policy"

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
