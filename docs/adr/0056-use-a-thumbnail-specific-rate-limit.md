# Use a thumbnail-specific rate limit

* Status: accepted
* Date: 2026-09-07

## Context and Problem Statement

Opening a room can make a client request many thumbnails in one burst. Media
IDs normalize into one per-IP thumbnail bucket, so the general media limit of
20 requests per minute rejected ordinary rendering traffic. Raising the whole
media tier would also broaden upload and full-download limits.

## Considered Options

* Give thumbnail endpoints a built-in 60 request-per-minute refinement.
* Raise the default for the entire media tier.
* Keep the 20 request default and require every operator to configure an
  override.

## Decision Outcome

Chosen option: a 60 request-per-minute refinement for both supported thumbnail
endpoint forms. Uploads and full downloads retain the 20 request-per-minute
media-tier default. Operator tier and path-prefix overrides continue to take
precedence, so installations can tune the policy from observed traffic.

### Positive Consequences

* Normal room rendering is less likely to receive HTTP 429 responses.
* Expensive and state-changing media operations retain their existing cap.

### Negative Consequences

* Thumbnail generation can consume more resources during a one-IP burst.
* Large installations may still need a path-prefix override.
