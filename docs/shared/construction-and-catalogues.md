---
type: Rule
title: "Construction and catalogues"
description: "The server owns every manager before publishing its graph, catalogue loads prepare owned rows before publication and keep the previous table on failure, and accepted connections keep an owner through registration."
tags: [shared]
enforced_by: [shared_server_runtime_tests, accepted_server_connection_tests, socket_construction_tests]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Construction and catalogues

The fixtures are test groups in `shared_server_runtime_tests` ([sharedserver](overview.md)).

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| Shared-server construction owns every manager before publishing its completed graph; quiescent destruction joins the worker and restores previous context bindings | `SharedServerConstruction` in `shared_server_runtime_tests` over the production server and real sockets | leaked partial managers/listeners, stale bindings, unsafe destruction before init, broken allocation retry, damaged nested scopes or accepted players surviving scoped teardown |
| Shared server/group catalogue loads prepare owned rows before publication, validate IDs before narrowing, and retain the previous catalogue on failure | `SharedCatalogue` in `shared_server_runtime_tests` over the production loaders with supplied repository rows | unsafe failed initialization, leaked reloads or world-zero rows, partial replacement, wrapped dimensions/IDs, accepted duplicates, damaged world filtering or failed allocation retry |
| Shared resurrection/string loads replace complete tables only after successful preparation; failed loads preserve paired locations, exact text and borrowed string pointers | `SharedStartupData` in `shared_server_runtime_tests` over explicit repository inputs | mismatched race positions, stale removed zones, partial publication, wrapped IDs/coordinates, lost strings or borrowed pointers, changed empty/missing behavior, leaked reloads or failed allocation retry |
| Accepted shared-server connections retain an owner through preparation and registration; manager destruction releases listeners and players after joining | `accepted_server_connection_tests` over `AcceptedServerConnection`, `socket_construction_tests` for pending socket errors, and `SharedConnection` cases with allocation faults | ignored `SO_ERROR`, wrong options, leaked acceptance/refusal resources, occupied table slots overwritten, failed retry or an owned listener/player surviving manager destruction |

The socket replacement cases run through `GameServerPlayer` here as well
([sockets and connections](../startup/sockets-and-connections.md)).
