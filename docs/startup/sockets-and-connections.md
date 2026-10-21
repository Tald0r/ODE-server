---
type: Rule
title: "Sockets and connections"
description: "Listener startup retries only bind failures, every socket keeps an owner through construction, adoption and replacement, and connection keys never leave the process."
tags: [startup, protocol]
enforced_by: [listener_startup_tests, outbound_server_connection_tests, socket_construction_tests, accepted_server_connection_tests, socket_stream_setup_tests, player_connection_key_tests]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Sockets and connections

The libraries are described in [startup composition](overview.md); the per-process connection
handoffs are in [login](../login/account-ownership-and-kicks.md), [shared](../shared/construction-and-catalogues.md)
and [game](../game/connections-and-players.md).

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| Listener startup retries only bind failures, observes shutdown during waits and releases each failed attempt's socket | `listener_startup_tests`, over `ListenerStartup` and real TCP/UDP sockets in subprocesses | an unreported or retried non-bind failure, a bind after shutdown, an uninterruptible retry wait, lost failure state or a leaked descriptor |
| Outbound connections return owned sockets only after connect and option setup; player adoption and Mofus stream setup release partial resources on failure | `outbound_server_connection_tests`, `socket_construction_tests` and the game runtime's `GameConnection` tests | incorrect socket options, a leaked descriptor/allocation, partial Mofus publication, failed retry after allocation failure or lost ownership while constructing a player |
| Accepted sockets retain an owner through wrapper construction; failed reconnects release the replacement and leave safe closed state | `socket_construction_tests`, over real accepts/reconnects and isolated allocation faults | an abandoned accepted descriptor/implementation, a dangling member after reconnect, a failed attempt left open, closing a reused descriptor or a failed subsequent retry |
| Accepted shared-server connections retain an owner through preparation and registration; manager destruction releases listeners and players after joining | `accepted_server_connection_tests` over `AcceptedServerConnection`, `socket_construction_tests` for pending socket errors, and shared runtime `SharedConnection` cases with allocation faults | ignored `SO_ERROR`, wrong options, leaked acceptance/refusal resources, occupied table slots overwritten, failed retry or an owned listener/player surviving manager destruction |
| Plain socket streams are prepared together; player socket replacement preserves old buffers on failure and closes the old socket only after success | `socket_stream_setup_tests` over `SocketStreams` and `Player`, the same replacement cases through `GameServerPlayer` in `shared_server_runtime_tests`, and Mofus allocation tests in the game runtime | a partial stream leak, changed buffer sizes or absent-stream mode, lost buffered bytes after failure, an incoming socket left unowned, a same-socket double close or a failed retry |
| Connection keys have one player owner and update stream borrowers only after preparation; received keys never exit the process | `player_connection_key_tests` over `ConnectionKey` and `Player`, plus production game/login dispatch cases | changed table bytes or stream transforms, lost state on allocation failure, leaked replacement/destruction storage, stale stream pointers after socket replacement or process exit during a handshake |

`Player(Socket*)` adopts its socket even when stream construction fails, and the Mofus client
publishes socket, input and output together after complete setup; socket constructors keep
temporary implementation ownership through creation, and managers keep temporary ownership
through socket configuration.
