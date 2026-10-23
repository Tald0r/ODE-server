---
type: Rule
title: "The mutation rule"
description: "Zone-group state is touched only while that group's mutex is held, and the Debug ownership check aborts on a violation at the gateways it covers."
tags: [threading]
enforced_by: ["ZoneGroup::assertOwned() under DE_OWNERSHIP_CHECKS (Debug builds)"]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# The mutation rule

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| Zone-group state is touched only under that group's mutex | `ZoneGroup::assertOwned()` under `DE_OWNERSHIP_CHECKS` (Debug builds only) | `abort()` at the gateway |

**Zone-group state (Zones, the Creatures in them, the group's `ZonePlayerManager`) may only be
touched while that group's mutex is held.** The group's own `ZoneGroupThread` holds it for its
entire tick ([threads](overview.md)); any other thread must take it explicitly, e.g.
`__ENTER_CRITICAL_SECTION((*(pZone->getZoneGroup())))`, which `GDRLairManager` does at every
site that touches zone-group state. Its `addEffect_LOCKING`/`deleteEffect_LOCKING` calls need
no group mutex: they go to the zone's separate locked effect manager, which the tick services only
under `Zone::m_MutexEffect`, the one sub-lock designed for cross-thread adds.

`Zone::m_Mutex` is a **different, narrower** lock some main-thread heartbeats take (war and ctf
via `pZone->lock()`); holding it does not exclude the zone-group tick and does not satisfy this
rule.

# The check

This is mutex-guarded ownership, not pure thread affinity: the guarded region is the contract.
Under `DE_OWNERSHIP_CHECKS`, defined only for Debug builds and compiled away completely elsewhere
([assertions and NDEBUG](../architecture/assertions-never-ndebug.md)), `ZoneGroup::lock()`
records the holding thread (`pthread_equal` plus a valid flag, never a raw `==` or a zero-tid
sentinel) and `ZoneGroup::assertOwned()` **calls `abort()`** on a violation. It must not throw:
an `AssertionError` is a `Throwable`, and the `catch (Throwable&)` blocks sitting on these very
paths would swallow it, turning a detected race into a silently half-applied mutation. The check
is armed by `ZoneGroupThread::run()`, so single-threaded startup and loading are exempt.

`ZoneGroup::drainMailbox` asserts too. On `Zone` the coverage is exactly eight gateways:
`addPC` (both overloads), `replacePC`, `addCreature`, `deleteCreature`, `moveCreature`, and the
tile-only pair `addCreatureToTile`/`deleteCreatureFromTile` that the move-mode swaps and corpse
paths use, so every creature write to a `Tile` outside `ZoneSpawn.cpp` and `ZoneMove.cpp`, where
the gateways live, is gated. `Zone::movePC`/`deletePC`/`pushPC`/`addItem`/`deleteItem` are
**not**: the assert is a tripwire on the main gateways, not a guarantee. The paths that once
broke the rule, and how each was closed, are in [closed violations](closed-violations.md); the
ways to reach a group from another thread are in
[cross-thread communication](cross-thread-communication.md).
