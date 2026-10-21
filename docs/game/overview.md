---
type: Design
title: "Gameserver"
description: "The gameplay process: a composition root per packet, handler, fill, repository and module directories, zone-group threads, and the Exchange's startup check."
tags: [game, architecture]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Purpose

The gameserver runs the game for one world or zone group: zones, creatures, skills, items, wars,
quests and the social systems. Several run at once against one loginserver and one sharedserver
([server architecture](../architecture/overview.md)).

# Moving parts

- **Layout** under `src/server/gameserver/`: `GamePacketDispatch.cpp` is the composition root
  (packet id to handler, with `kReceivedDirections` narrowing the GC link to what a client
  sends, [client link](../protocol/client-link.md)); `handler/` holds the CG/GC/LG/GG/SG handler
  bodies; `packetfill/` the server-side fill helpers moved out of Core; `repository/` the 36
  persistence seams ([repository seams](../architecture/repository-seams.md)); `gm/` the GM and
  console commands behind a router (`gm_command_router_tests`, `gm_console_command_tests`);
  `skill/`, `item/`, `war/`, `quest/` (Lua-scripted, `quest/luaScript/`) the game modules;
  `guild/`, `party/`, `trade/`, `couple/`, `exchange/` the social and trading decisions;
  `ctf/`, `mission/`, `mofus/` capture the flag, missions and game events.
- **Threads.** One `ZoneGroupThread` per zone group owns its zones; the manager threads own the
  links to the login and shared servers ([threads](../threading/overview.md)). Every CG handler
  runs on a zone thread with the group mutex held ([the mutation rule](../threading/mutation-rule.md)).
- **Frames.** `GamePlayer::processCommand` runs every frame through `GameFrameGate`
  (`GameFrameGate.h`), which checks the sequence byte where the frame is consumed
  ([frame gate](../protocol/frame-gate.md)).
- **Decisions** are `Outcome` values with isolated tests: `exchange_decision_tests`,
  `guild_join_decision_tests`, `party_invite_decision_tests`, `trade_prepare_decision_tests`,
  `trade_table_decision_tests`, `speed_hack_decision_tests`, `castle_tax_balance_tests`,
  `concrete_item_tests`, `race_skill_slot_tests`, `skill_slot_tests`
  ([exceptions and outcomes](../architecture/exceptions-and-outcomes.md)).
- **The Exchange's point tables** (`AccountPoint`, `PointLedger`) are reached from the game
  connection by the account schema's name (`UI_DB_DB`), so a purchase is one transaction on one
  connection. The gameserver checks that at startup (`ExchangeService::openPointLedger`); when it
  fails it starts anyway with Exchange purchases refused, and says why on stderr and in
  `DBError.log` ([configuration](../database/configuration.md)).
- **Runtime tests.** `game_server_runtime_tests` links the production `GameServerRuntime`
  objects without `main.cpp`; its groups own [connections and players](connections-and-players.md)
  and exercise the production dispatch table.
