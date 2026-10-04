# Production logging

Production diagnostics should explain service health and failed operations. A
successful packet, quest condition, cache revalidation or TCP health probe is
not an operational incident.

## Current controls

`DARKEDEN_TRACE` is off by default. Set it to exactly `1` before starting a
server to enable the migrated packet, login, admission, quest, party and
startup catalogue/table diagnostics. The Compose server service forwards this setting. Recreate the
container after changing it and turn it off after diagnosis. Trace formatting
is lazy and best-effort; a failing diagnostic sink cannot change packet bytes
or poison the shared error stream. Packet traces contain direction, numeric ID,
body size and sequence, never packet descriptions or credentials.

Login/shared startup acknowledges configuration without printing its values.
Login routing/population, common/shared server catalogues, and selected game
server group/castle/PK-zone/variable/event-quest tables are opt-in diagnostics.
Failed optional catalogue output does not abort a validated replacement; real
repository or validation failures retain their existing behavior.
An orderly TCP close still retires the login connection, but emits no error.
Connection resets, malformed packets and independent cleanup failures retain
their existing diagnostics. Item and money audit records remain unchanged.

This is a migration of selected high-volume paths, not a global severity
filter: legacy `cout`, `cerr`, `filelog` and other startup output still exist.
The gateway already uses `RUST_LOG` (default `info`). The client repository owns
nginx access logging and its native/browser diagnostic logger.

## Severity policy for new and migrated events

| Level | Keep in production | Examples |
|---|---|---|
| ERROR | Yes | Failed persistence, unusable startup dependency, exhausted retries, corrupt state. |
| WARN | Yes | Recoverable unexpected failure, invalid protocol, degraded backend. |
| INFO | Yes, at significant transitions | Service ready/stopped, configuration accepted, recovery from an outage. |
| DEBUG / TRACE | Opt in temporarily | Packet framing, quest evaluation, routing and internal progress. |

An expected game result such as an unsuccessful quest or insufficient item
space is not a server error. Ordinary health probes and disconnects need no
per-event operational log. Successful HTTP assets should be counted as metrics;
retain failed requests and proxy errors for diagnosis. Security/audit events
such as authentication outcomes, privileged changes and item/money transfers
have separate access and retention needs and must not depend on DEBUG being on.

Prefer one structured event per failure at its owning boundary, with severity,
service, stable event name, operation and a non-secret correlation identifier.
Include a numeric error code or exception category and enough context to act.
Escape untrusted text and keep fields bounded. Do not print passwords, login
keys, tokens, full configuration, SQL values, packet payloads or raw launch
arguments, including in temporary debug mode. Avoid account/IP identifiers as
metrics labels: they create unbounded series.

Bound repeat reporting by a fixed failure category. Emit the first failure,
periodic suppressed counts, and meaningful recovery/shutdown summaries. A
successful request must not reset the emission budget, otherwise alternating
success/failure recreates a flood. Counts, active connections, latency and error
rates belong in metrics rather than one log line per request.

Bound local/container diagnostic storage by size and retained generations.
Logging output failures must remain observable without recursion or repeated
floods, and must not replace the operation's original failure. Audit storage is
a separate contract; do not apply diagnostic dropping rules to audit records.

## Migration and deployment

The source audit also identified raw SQL/driver diagnostics, per-player packet
files, client launch/key output, unbounded local diagnostic files, and repeated
listener/gateway/UDP reports. Those paths require their own reviewed changes;
this document describes the production policy, not a claim that every legacy
site has already migrated. Track actual fixes in `docs/FIXES.md` and their PRs.

Deploy and check the changed application images before removing collector
workarounds. The DarkEden-specific Alloy keyword allowlist guesses severity
from text and can hide useful records; it should be retired after source-side
controls are in place. The successful-request and normal-close drop rules then
become redundant. Alloy still provides collection and delivery to Loki.

References: [OpenTelemetry severity model](https://opentelemetry.io/docs/specs/otel/logs/data-model/),
[OWASP application logging guidance](https://cheatsheetseries.owasp.org/cheatsheets/Logging_Cheat_Sheet.html),
and [Google SRE monitoring](https://sre.google/sre-book/monitoring-distributed-systems/).
