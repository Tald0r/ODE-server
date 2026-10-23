---
type: Rule
title: "Connections and players"
description: "A game connection keeps an owner through authorization and registration, a player handoff keeps the source until the destination accepts it, and queued broadcasts own their bytes."
tags: [game]
enforced_by: [game_server_runtime_tests, socket_stream_setup_tests, game_frame_gate_tests, player_connection_key_tests]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Connections and players

The fixtures are test groups in `game_server_runtime_tests` ([gameserver](overview.md)); the
socket-level rules they build on are in [sockets and connections](../startup/sockets-and-connections.md).

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| Game connections retain ownership through authorization, diagnostics and registration; scoped incoming managers release their listener, table and queues and restore the connection-info binding | Game runtime `IncomingGameConnection` and `GameAdmission` cases plus encrypted `socket_stream_setup_tests` | leaked admission or teardown resources, skipped unattached-player cleanup, wrong stream types/sizes, lost context bindings, duplicate cleanup or failed refusal/retry/I/O without database startup |
| Game-player table/queue handoffs retain the source until destination insertion succeeds; quiescent managers release each local player once in terminal state | Game runtime `GamePlayerHandoff` and `GamePlayerTransfer` cases over the shared handoff helpers, real `CGReady`/heartbeat paths, allocation faults, socket I/O and cleanup | an unowned player after failed insertion or diagnostics, failed retry, starved batch entries, stale table/poll state, duplicate destruction or cleanup stopped by one failed logout |
| Queued game broadcasts own encoded bytes and optional filter clones; synchronized dispatch consumes each started message and retains later entries after uncaught failure | Game runtime `GameBroadcast`, `GameBroadcastQueue` and `GameBroadcastDelivery` cases over allocation faults, failure/retry, real filtered TCP delivery and concurrent producers | a null-filter crash, leaked preparation/queued resources, changed framing or selection, retained borrowed inputs, lost later messages or replayed dispatch |
| Outbound connections return owned sockets only after connect and option setup; player adoption and Mofus stream setup release partial resources on failure | `outbound_server_connection_tests`, `socket_construction_tests` and the `GameConnection` cases | incorrect socket options, a leaked descriptor/allocation, partial Mofus publication, failed retry after allocation failure or lost ownership while constructing a player |

Per-player packet diagnostic files contain event, packet ID and body size, never payload
descriptions (`game_packet_diagnostics_tests`); the frame gate's own cases are in
[frame gate](../protocol/frame-gate.md).
