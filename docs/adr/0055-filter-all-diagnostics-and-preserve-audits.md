# Filter all diagnostics and preserve audits when removing duplicate warnings

* Status: accepted
* Date: 2026-09-07

## Context and Problem Statement

Direct logging macros bypassed module filtering, so an info configuration
could still emit debug messages when the console debug switch was enabled.
Client rate-limit rejection also emitted two warnings for one HTTP 429,
each associated with an existing policy audit event.

## Considered Options

* Filter all diagnostic entry points centrally and retain both audit events
  while emitting only the detailed rate-limit warning.
* Fix only the observed WorkerPool call site and remove the second audit
  event along with its warning.
* Disable console debugging or hide entire warning-producing modules.

## Decision Outcome

Chosen option: central diagnostic filtering with independent audit
persistence. Module/default levels apply before sink thresholds for every
logging entry point. Console debugging does not override explicit config.
Legacy macros retain their function-name logger identifiers and therefore
use the wildcard default unless that name has an explicit override.

The limiter retains its detailed warning. The HTTP 429 response path keeps
the existing request-rejection audit event without a duplicate diagnostic.
This preserves audit consumers and enforcement while reducing redundant
operator output. A call-site-only filter would leave other bypasses; deleting
the companion audit event would change persisted observability semantics.

### Positive Consequences

* An info threshold consistently suppresses debug diagnostics.
* Each denied request retains its audit history even when logs are off.

### Negative Consequences

* Legacy macro logger names remain function names rather than subsystem names.
* Audit storage still records two events for a rate-limit rejection.
