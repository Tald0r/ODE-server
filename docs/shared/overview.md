---
type: Design
title: "Sharedserver"
description: "The process that holds what every game server shares, guilds above all: a heartbeat on the main thread, a worker that owns the TCP listener, and GS/SG packets."
tags: [shared, architecture]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Purpose

The sharedserver keeps the data several gameservers read and change together: guilds, guild
unions and the server catalogue. Gameservers connect to it over TCP; the gameserver side of that
link is the `SharedServerManager` thread ([threads](../threading/overview.md)).

# Moving parts

- **Threads.** The main thread runs `HeartbeatManager::run()`; the `GameServerManager` worker owns
  the TCP listener plus `GuildManager::heartbeat()`. Each registers its own DB `Connection`
  keyed by `Thread::self()`.
- **Packets.** GS packets arrive from gameservers and SG packets go back; the handlers are bound
  in `src/server/sharedserver/SharedPacketDispatch.cpp` ([packet system](../protocol/overview.md)).
  `SharedServer.h` defines `__SHARED_SERVER__` itself, so including it makes a translation unit
  the shared server's ([preprocessor macros](../architecture/preprocessor-macros.md)).
- **Persistence.** Two seams under `src/server/sharedserver/repository/`,
  `SharedConfigRepository` and `SharedGuildRepository`, prefixed `Shared` because the integration
  binary links every implementation ([repository seams](../architecture/repository-seams.md)).
- **Guild model.** `Guild`, `GuildManager` and `GuildDecision` in `src/server/sharedserver/`; a
  deleted guild or member is retired, never freed, so a raw pointer handed out under a lock stays
  readable ([closed violations](../threading/closed-violations.md)). `shared_guild_decision_tests`
  owns the decisions.
- **Tests.** `shared_server_runtime_tests` links the production `SharedServerRuntime` objects
  over real sockets and supplied repositories; its groups are the owners in
  [construction and catalogues](construction-and-catalogues.md) and [guilds](guilds.md).
