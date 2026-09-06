<!-- Maintained by hand. Add a line here in the same commit as a new ADR. -->
# Architecture Decision Records

## Accepted Records

* [0000 - Record architecture decisions](0000-record-architecture-decisions.md)
* [0001 - Use Markdown Architectural Decision Records](0001-use-markdown-architectural-decision-records.md)
* [0002 - Use an owner-tracking mutex for the runtime lock](0002-use-an-owner-tracking-mutex-for-the-runtime-lock.md)
* [0003 - Drain every recursion level in one release primitive](0003-drain-every-recursion-level-in-one-release-primitive.md)
* [0004 - Release the runtime lock through the mutex, not through a guard](0004-release-the-runtime-lock-through-the-mutex-not-a-guard.md)
* [0005 - Open the lock release scope at the blocking callee](0005-open-the-lock-release-scope-at-the-blocking-callee.md)
* [0006 - Extract released regions that produce values into functions](0006-extract-released-regions-that-produce-values-into-functions.md)
* [0007 - Assert lock behaviour from another thread](0007-assert-lock-behaviour-from-another-thread.md)
* [0008 - Keep reject-unsafe exemptions line-scoped](0008-keep-reject-unsafe-exemptions-line-scoped.md)

## Rejected Records

* None

## Superseded Records

* None

## Deprecated Records

* None

## Records with non-standard statuses

* None
