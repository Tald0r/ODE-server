---
type: Rule
title: "Critical sections"
description: "An __ENTER/__LEAVE block is owned by a scoped guard, so a hand-written unlock() inside it is a double unlock; the audit script finds one."
tags: [threading]
enforced_by: [critical_section_audit, game_server_runtime_tests]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Critical sections

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| A critical section is never unlocked by hand | `tests/tools/critical_section_audit.pl`, ctest `critical_section_audit` | the file and line of the hand-written `unlock()` |
| A destructor takes no lock of its own object | the `GameDestructor` cases in `game_server_runtime_tests`, for `GuildManager` and `HolyLandManager` | the case that deletes the object under its own lock: the lock failure on `std::cerr`, and for `GuildManager` its guilds left unfreed |

`__ENTER_CRITICAL_SECTION` / `__LEAVE_CRITICAL_SECTION` delimit a **block** owned by a scoped
`CriticalSection` guard (`src/Core/Exception.h`), so the lock is released on every exit: end of
block, `return`, `goto` or `continue` out of it, and any thrown type. Consequently a
hand-written `x.unlock()` inside a section is a **double unlock** of a non-recursive mutex,
undefined behaviour that is invisible in a diff. To run work unlocked, use
`__CRITICAL_SECTION_LOCK.unlock()` / `.lock()`, the guard's own name.

`tests/tools/critical_section_audit.pl` (ctest `critical_section_audit`) fails on a hand-written
unlock, naming the file and line, on a `__LEAVE_CRITICAL_SECTION` whose argument disagrees with
its `__ENTER`'s, on an unclosed section, and on finding no sections at all, so a wrong working
directory cannot read as a pass. It scans `./src`, which is why ctest runs it at the repo root.

**A destructor takes no lock of its own object.** Nothing else may use an object while it is
destroyed, so the lock guards nothing, and the mutexes are error-checking: when the deleting thread
already holds the lock, the destructor's own `lock()` fails with `EDEADLK` and throws an `Error` the
destructor cannot let out. `Guild`, `GuildManager`, `Party`, `ParkingCenter` and `HolyLandManager`
used to lock and drop that failure, skipping their cleanup. The `GameDestructor` cases in
`game_server_runtime_tests` delete a `GuildManager` and a `HolyLandManager` while holding their
lock; the other three keep their mutex private, so no test can hold it.

A critical section is also the wrong tool when the lock is a leaf held for one map access, such as
the database manager's connection tables under a `std::shared_mutex`
([threads](overview.md)) or a guild's string accessors under `Guild::m_Mutex`
([closed violations](closed-violations.md)).
