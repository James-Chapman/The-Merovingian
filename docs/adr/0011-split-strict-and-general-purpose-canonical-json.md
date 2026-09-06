# Split strict and general-purpose canonical JSON

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

Matrix canonical JSON forbids floats and out-of-range integers in anything
signed or hashed. But the same serializer also produces ordinary client-server
responses that legitimately contain floats — `m.tag` `order`, account data.

One serializer cannot both reject floats and emit them.

## Considered Options

* Two entry points: a strict one used only on the signing and hashing path, and
  a permissive one for responses
* One general-purpose serializer used for both, with callers responsible for
  not signing anything containing a float

## Decision Outcome

Chosen option: "Two entry points". `serialize_canonical_strict()` fails closed
with `CanonicalJsonError::float_not_allowed` on any double and is the only
entry point `event_signer.cpp`, `event_id.cpp` and `signable.cpp` use.
`serialize_canonical()` accepts floats. The parsers mirror the split:
`parse_lossless()` accepts only JS-safe-range integers, `parse_json()` accepts
doubles and exponent notation.

### Positive Consequences

* The failure is structural, not a convention a new call site can forget.
* A float-formatting bug in the shared serializer cannot silently corrupt a
  value that is then hashed and signed as if it were correct.

### Negative Consequences

* Two functions that look interchangeable and are not. Choosing the permissive
  one on a signing path compiles and produces plausible output.

## Links

* [`docs/canonical-json.md`](../canonical-json.md), "Numeric policy"
* [`docs/security-coding-rules.md`](../security-coding-rules.md)

<!-- Backfilled in 0.12.6 from existing documentation and code. The date above is
the date of the record, not of the decision; the decision predates it. -->
