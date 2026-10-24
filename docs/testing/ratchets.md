---
type: Rule
title: "Ratchets"
description: "Ratchet numbers only go down and fail in both directions; a drop is tightened in ratchets.sh and the plan's table in the same commit, and every baseline comes from running its command."
tags: [testing, process]
enforced_by: [ratchets]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Ratchets

**Ratchet numbers only go down.** `tests/ratchet/ratchets.sh` (ctest `ratchets`) fails in both
directions: above the baseline is new debt, below it is progress nobody wrote down. The only rise
is a widening: a measure widened to count what it missed rises in the commit that widens it, by
exactly what the new scope adds (the old scope's count unchanged), with the numbers, date and
reason beside its command and in the plan's row. When a number
drops, tighten it in `tests/ratchet/ratchets.sh` **and** in the table in
[the restructuring plan](../reviews/RESTRUCTURING.md) in the same commit. Every baseline comes from
running its command, never from an estimate. The script also carries the generated-file pins:
the factory registrations, `AllPacketFactories.inc`, the per-code encrypter goldens
([factory registrations](../protocol/factory-registrations.md), [wire contract](../protocol/wire-contract.md))
and the seed-data rules ([seed-data rules](../database/seed-data-rules.md)).

The nineteen ratchets, by what each counts (the plan's table has each command and the history of
every move):

| # | Metric |
|---|---|
| R1 | `g_p*` global-singleton extern declarations ([contexts, not globals](../architecture/contexts-not-globals.md)) |
| R2 | Files with inline SQL in the gameserver root ([repository seams](../architecture/repository-seams.md)) |
| R3 | Files with inline SQL outside `database/` and any `repository/` |
| R4 | Packet headers with `execute()` still on the packet ([packet system](../protocol/overview.md)) |
| R5 | `__BEGIN_TRY` control-flow macro sites in the gameserver |
| R6 | Line count of god files, each tracked separately |
| R7 | Files using parenthesized `throw(...)` syntax |
| R8 | Non-comment lines using `__PRETTY_FUNCTION__` |
| R9 | Hand-written length-prefixed string reads left in `src/Core` |
| R10 | Throws of a pointer into a local string, and the handlers that caught one ([exceptions and outcomes](../architecture/exceptions-and-outcomes.md)) |
| R11 | Bare string-literal throws left in `src` |
| R12 | Throw messages carrying non-ASCII text |
| R13 | Duplicated include guard names across `src/**/*.h` |
| R14 | Mentions of macros nothing defines ([preprocessor macros](../architecture/preprocessor-macros.md)) |
| R15 | `src/**/*.cpp` that no target compiles ([dead sources](../architecture/dead-sources-and-encoding.md)) |
| R16 | Headers under `src/` that nothing includes |
| R17 | Source lines carrying a non-ASCII byte |
| R18 | Commented-out code lines inside multi-line `/* */` blocks |
| R19 | `catch (Throwable&)` clauses in the gameserver that drop the exception ([exceptions and outcomes](../architecture/exceptions-and-outcomes.md)) |
