---
type: Record
title: "Closed violations"
description: "The known violations of the mutation rule, each closed, and the rules they left behind: guild retirement, tile gateways, snapshot zone maps, GDRLair locking, posted war work and the CGReady drain."
tags: [threading, history]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Closed violations

Each of these once broke [the mutation rule](mutation-rule.md). Each is closed, and the rule it
left behind is stated here because the code alone does not say why it is shaped that way.

- **SG/LG/GG handlers must not mutate creature state under the `PCFinder` lock alone.** The six
  guild handlers and `LGKickCharacter` post their gold / guild-id / kick-flag / zone-broadcast
  work through `de::postToPlayer` ([cross-thread communication](cross-thread-communication.md)).
  `GuildManager` and `Guild` lock their maps on both sides: `getMembers_NOLOCKED()` is the
  writer thread's, readers copy the member names under the guild mutex, and deleting a guild
  empties its map through `retireAllMembers()`. The per-member flags and counters and the guild's
  own integral fields are atomics, the rank change and its counter update going under the mutex
  together; its strings are copied in and out under `Guild::m_Mutex`, a non-recursive leaf the
  string accessors take and nothing else, so code already holding it touches the members directly.
  Lifetime is handled by not freeing: `getGuild()` and `getMember()` return raw pointers after
  releasing their locks, so a deleted guild or member is *retired* (`GuildManager::m_RetiredGuilds`,
  `Guild::m_RetiredMembers`; the whole-table `clear()` a sharedserver resync triggers retires too,
  never frees) and stays readable, stale, until the managers are destroyed. A retired object says
  so: an atomic flag set where the retirement happens, under the lock that already covers it, and
  the answers a stale pointer gives enforce it, so a reader that decides on a rank or a state needs
  no check of its own; the readers that decide membership alone test `isRetired()`.
  `GuildMember::getRank()` reads `GUILDMEMBER_RANK_LEAVE` once retired, `Guild::getState()` reads
  `GUILD_STATE_BROKEN` and `Guild::getMember()` answers NULL for every name, while the guild's own
  counters, teardown list and database rows take `getStoredRank()` / `m_State` and still carry
  what the member and the guild had. `GuildUnionManager` has the same shape: its tables are a
  `GuildUnionRegistry` locked on both sides, a union it takes away is retired and names no member
  guild, and the lock order is in `GuildUnion.h`. A union exists for its member guilds and its
  pending join offers, and an offer lives ten days: every path that takes a member or an offer away
  applies that one rule (`dissolveIfAbandoned_LOCKED`), and the expired offers are purged before an
  offer is read to be acted on and at every load (`purgeOffers`, stated in `GuildUnion.h`).
- **No creature is written to a `Tile` outside `ZoneSpawn.cpp` and `ZoneMove.cpp`.** PC swaps go
  through `Zone::replacePC`; the move-mode swaps, the knockback and NPC-warp moves, and the corpse
  paths that take a dead creature off the map while its manager keeps it go through
  `Zone::addCreatureToTile` / `Zone::deleteCreatureFromTile`, all gated.
- **A zone map may be extended from another group's thread**, because the group's zone map and
  the `ZoneInfoManager` tables are `de::Snapshot`s: the iterating heartbeat and every concurrent
  `getZone()` keep the map they loaded. A recycled instance's `init()` is posted to the owning
  group (`ZoneGroup::post()`), `DynamicZoneGroup` serialises selection and creation under its own
  mutex, and the instance status flag is atomic. `addZone` is not a gateway, so the assert does
  not see this path.
- **`GDRLairManager` takes the owning group's mutex at every site** that walks its PCManager,
  sweeps effects or writes inventory and registries. None of those sites hits a gated gateway.
- **A war changes no zone from the thread that starts or ends it.** The war heartbeat (main
  thread), a castle's scheduler (the castle zone's thread), a GM starting the race war and the flag
  war (`FlagManager`'s schedule, on the main thread) post every zone change to the owning group
  through `war/WarZoneWork.h`: `postToZone`/`postToZones` (one command per group, the zone looked
  up by id when it runs), `postToEveryZoneGroup`, and `postItemReturn` for a castle symbol, blood
  bible, dragon eye or flag, which reads the item's row and posts the step that takes it out to the
  zone or player (`de::postToPlayer`) holding it; that step hands the item on through
  `Zone::transportItemToCorpse`, or destroys a flag. The flag war's drops, pole sweeps, status
  broadcasts and newbie-war summons are `postToZone`/`postToZones` commands, its values in
  `ctf/FlagWarPlan.h`. The castle owner change goes through `CastleInfoManager::postCastleWarEnd`.
  A posted war command asserts its group, and `Zone::killAllMonsters`, the shrine shields and the
  siege set-up and reset assert it too. Reading a zone's player list is zone-group state as well,
  so a broadcast to another group's zones is posted: `postBroadcast` captures the packet's body on
  the calling thread (`war/CapturedPacket.h`; each player's stream frames it with its own sequence
  and encryption) and each group's command sends it. The holy land's, a castle's and a level war's
  bonus-zone broadcasts (`HolyLandManager::broadcast`, `CastleInfo::broadcast`,
  `LevelWarZoneInfoManager::broadcast`) go this way from any thread;
  `ZoneGroupManager::broadcast` stays direct, walking each group's players under its
  `ZonePlayerManager` mutex and only sending.
- **Packets pipelined behind `CGReady` are drained by the zone thread.** `GamePlayer::processCommand`
  stops the main-thread (`IncomingPlayerManager`) drain once the status flips to `GPS_NORMAL`, so
  nothing reaches a gateway without the group mutex.
