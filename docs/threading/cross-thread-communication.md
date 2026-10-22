---
type: Design
title: "Cross-thread communication"
description: "Sending to a player's socket is the one legitimate cross-thread operation; everything else goes through the player's mailbox, the group's mailbox or a copy-on-write snapshot."
tags: [threading]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Purpose

[The mutation rule](mutation-rule.md) says a group's state belongs to its thread. These are the
ways another thread reaches it without breaking that.

# Moving parts

- **Finder lock, then send.** Cross-thread packet handlers (SG/LG/GG, on the manager threads)
  reach player creatures through the player-creature finder (`de::gameContext().playerCreatures()`)
  under **its** critical section (`getCreature_LOCKED`), then use `pPlayer->sendPacket(...)`.
  Sending to a player's socket is the main legitimate cross-thread operation.
- **The player's mailbox** (`src/server/Mailbox.h`, owned by `GamePlayer`). Anything beyond
  sending (gold, guild id, kick flags, a zone broadcast) goes through
  `de::postToPlayer(name, command[, ifGone[, scope]])` (`src/server/gameserver/PlayerMailbox.h`),
  which finds the player under the PCFinder lock and queues the command; the manager that owns the
  player drains it from the one place it processes its players, each tick, before the player's own
  packets. The box belongs to the player rather than the group because a player's owner changes
  over its life (the main thread's `IncomingPlayerManager` during login and zone transfer, a
  `ZonePlayerManager` on a zone thread in between) and the creature's zone pointer does not say
  which: it is set as soon as the character loads and stays on the old zone during a transfer, so
  the box follows the player and keeps posting order across group changes. The zone manager, under
  the group mutex, runs everything (for a player whose creature is in another group's zone, the
  listing mismatch it already logs as ZPMCheck, only the player-scoped ones); the main thread runs
  only `Scope::Player` commands (the kick flags), because the zone the creature points at is
  ticking elsewhere, so `Scope::Zone` commands wait in order for a zone thread. A player who logs
  out with commands pending runs their `ifGone` handlers instead (`~GamePlayer`, right after the
  PCFinder removal, so exactly one of command/`ifGone` runs), for the handlers whose offline branch
  matters, like charging a guild fee in the database. Commands capture by value only (a
  `Guild*`/`GuildMember*` may be deleted before they run) and run without the PCFinder lock, so
  they may post to other players.
- **The group's mailbox.** `ZoneGroup::post()`, drained at the top of the tick under the group
  mutex, carries group-level work from other threads; its producer is dynamic zone recycling,
  handing an instance's `init()` to its owner.
- **Snapshots.** Tables every thread reads and one thread occasionally extends, a group's zone map
  and the `ZoneInfoManager` lookups, are published copy-on-write through `de::Snapshot`
  (`src/server/Snapshot.h`): readers load an immutable `shared_ptr<const T>` without waiting on
  a writer and may iterate it for a whole tick; a writer copies, changes and swaps the pointer
  under a leaf mutex held for nothing else. That is what lets a zone thread create a dynamic zone
  in another group's map while that group iterates it. The writer mutex is held while the change
  runs, so a change must be pure work on the copy (map inserts and erases today); the snapshot
  protects the table, not the objects it points to, which stay raw pointers with their old
  lifetime rules.
- **Entry.** Players enter a zone group through the `ZonePlayerManager` under its lock; the zone
  thread integrates them on its next tick.
- **War work** from the main thread, a castle zone's thread or a GM is posted to the owning group
  through `war/WarZoneWork.h`, and a broadcast to another group's zones captures the packet's
  body first (`war/CapturedPacket.h`) ([closed violations](closed-violations.md)).
