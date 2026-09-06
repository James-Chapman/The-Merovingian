# Use Markdown Architectural Decision Records

Adapted from the
[Opinionated Digital Center's ADR repository](https://github.com/opinionated-digital-center/architecture-decision-records),
whose format and layout this project follows.

* Status: accepted
* Date: 2026-09-06

## Context and Problem Statement

[ADR-0000](0000-record-architecture-decisions.md) decided to record
architectural decisions. Which format and structure should these records
follow?

## Considered Options

* [MADR](https://adr.github.io/madr/) 2.1.2 — the Markdown Architectural
  Decision Records template, as laid out by the Opinionated Digital Center
* [Michael Nygard's template](http://thinkrelevance.com/blog/2011/11/15/documenting-architecture-decisions)
  — the first incarnation of the term "ADR"
* A single append-only register file, `docs/decisions.md`, with numbered
  entries
* Formless — no conventions for file format and structure

## Decision Outcome

Chosen option: "MADR 2.1.2", because

* it names the parts that are easy to omit and expensive to lose — the
  *considered options* and the *negative* consequences. A decision recorded
  without its rejected alternatives is a fact, not a decision, and the next
  author re-derives the alternatives from scratch.
* one immutable file per decision beats one growing page: an ADR can be linked
  to by number from code comments and other ADRs, and superseding it leaves the
  original readable rather than rewritten.
* the structure is comprehensible and lean enough that filling it in is not a
  deterrent.

## Positive Consequences

* The layout matches the Opinionated Digital Center's: `docs/adr/`, filenames
  `NNNN-title-in-lowercase-with-dashes.md`
  ([MADR-0005](https://github.com/adr/madr/blob/2.1.2/docs/adr/0005-use-dashes-in-filenames.md)),
  no numbers in headings
  ([MADR-0002](https://github.com/adr/madr/blob/2.1.2/docs/adr/0002-do-not-use-numbers-in-headings.md)),
  a table of contents in [index.md](index.md), the template in
  [template.md](template.md), and `.adr-dir` at the repository root so
  `adr-tools` and `pyadr` find the directory.
* Status and date are mandatory, not optional, following the Opinionated
  Digital Center's
  [ADR-0006](https://github.com/opinionated-digital-center/architecture-decision-records/blob/master/docs/adr/0006-always-fill-status-and-date-in-adr.md).

## Negative Consequences

* Novice writers may force themselves to fill every section, which is *not*
  what is intended — sections marked optional in
  [template.md](template.md) are optional.
* Two of the Opinionated Digital Center's *process* rules are deliberately not
  adopted, because they suit an ADR-only repository rather than a project one:
  * **One ADR per feature branch.** Merovingian branches carry the code change
    and its ADRs together; issue #487 produced seven at once, and splitting
    them across seven branches would have separated each decision from the
    change that justifies it.
  * **`docs(adr):` conventional-commit prefixes.** This repository does not use
    Conventional Commits.
* `index.md` is maintained by hand. The Opinionated Digital Center generates it
  with `pyadr`, which is not part of this project's toolchain.

## Links

* Supersedes the single-register `docs/decisions.md` introduced earlier on the
  same branch, which is removed by this change.
