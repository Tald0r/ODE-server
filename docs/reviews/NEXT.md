---
type: Record
title: "Next work plan"
description: "The ranked, self-contained tasks left in the server after the 2026-10-25 merges, each written so an agent with no other context can take it: the recorded-not-fixed entries of the fix log, the fuzzers' two unread paths, and what stays blocked or with the owner."
tags: [review, plan]
---

# Next work plan

Written 2026-10-26, after the merges of PRs #374-#383. The [restructuring plan](RESTRUCTURING.md)
has two unticked tasks (5.5 in progress, 6.2 blocked); ratchets R1-R5 and R7-R19 are at 0 and
R6a-j at their baselines (line counts that can still shrink); the [fix log](FIXES.md) holds 416
`fixed` Status lines and 38 `recorded` ones. The `recorded` entries are the backlog: each was
found while restructuring and left as it was, by the rule that a move does not change behaviour.
What is left is below, ranked by the harm it closes, with a cheap fix ahead of a dearer one of
the same harm. Each task names the code, the test it wants and the rule it must keep, so it can
be taken cold. One task per PR.

## How to take a task

1. Read `CLAUDE.md`, then the concept each task links. The rules there bind; this page points.
2. A behaviour change is written test-first. The runtime tests link the production objects
   without `main.cpp`, with supplied repositories ([test coverage](../testing/test-coverage.md));
   a new runtime target is added in `tests/CMakeLists.txt` next to the one it resembles.
   Comments under `src/` and `tests/` describe behaviour in English: no task numbers, dates,
   fix-log references or review history
   ([dead sources and source encoding](../architecture/dead-sources-and-encoding.md)).
3. A wire change is a protocol change: the client repo ships the identical change and the
   goldens are re-recorded ([wire contract](../protocol/wire-contract.md)). A change under
   `src/domain` is done only when `bash tests/tools/decore_client_diff.sh <client-root>` is
   clean against a resynced client ([de-core parity](../protocol/decore-parity.md)).
4. A new ratchet, or a move in R1-R19, changes `tests/ratchet/ratchets.sh` and the plan's
   ratchet table in the same commit ([ratchets](../testing/ratchets.md)). Work that advances
   task 5.5 or 6.2 updates that task's Status line in the same commit.
5. Every `Status:` line of the fix-log entry the task names (one per bullet where an entry has
   several) becomes `fixed (<branch>)` in the same commit
   ([fix log and task status](../testing/fix-log-and-status.md)). Format only the files you
   touched ([formatting](../build/formatting.md)).
6. Verify with `make dev-test` before pushing: CI runs on master pushes and manual dispatch
   only, to conserve Actions minutes ([CI](../build/ci.md)). The owner's standing instruction,
   not written in a concept: every PR gets a two-reviewer adversarial review of its whole diff
   before merge.
7. Update this page in the same commit: move the task to *Done* with its PR, or narrow it. A
   changed concept gets a `docs/log.md` entry; moving a task here needs none. Run
   `perl tests/tools/check_knowledge_bundle.pl .` for any docs change.
8. Do not decide an item under *Blocked or with the owner*; they are the repository owner's
   calls.

## Tasks

### 4. Three gameplay defects kept by the compiler-warning cleanup

**What:** the *Compiler-warning cleanup (2026-09-27)* entry's `recorded, not fixed` bullets are
live defects, kept because the cleanup removed only tautologies:

- `CGLearnSkillHandler`'s range check can never fire (`>= SKILL_MAX && < SKILL_DOUBLE_IMPACT`);
  `CGAddItemToItemHandler` reaches `makeGCCreateItem` with X = 255 when the pet item is missing;
- the overrides that do not override (`processCommand()` without the base's `bool`);
- the smaller findings bullet: `MotorcycleLoader::load` and `~PlayerCreature` leak, and
  `EffectSatelliteBombAim` uses a skill slot with no null check.

**How:** one PR per bullet, each a one-line fix with a test that pins the intended behaviour.
Read the line's history first (`git log -p`, and the recovered source `3fccc6c6`, whose GBK
comments mark operator edits): the wolf-form `||` looked like a typo and was a 2006 hotfix. The
999999 durations and the `ExpireDate` in the same bullet are balance and data questions; leave
them recorded.

### 6. Count gates the login and guild reads still lack

**What:** three `recorded` entries of 2026-10-02 (*World-list decoding leaves refused records
unowned*, *Login server-list decoding still needs owned records and a count gate*, *Guild-info
decoding and roster budgets need separate ownership and bounds checks*) are half stale: PR #381
(`cb96dc5f`, `058cec4a`) put every row of `LCWorldList::read`, `LCServerList::read`,
`SGGuildInfo::read` and `GuildInfo2::read` in a `std::unique_ptr`, and `SGGuildInfo::read`
refuses a count above `GuildInfo2::kMaxCount`. Left: `LCWorldList::read` and `LCServerList::read`
accept a count past the 37 of `WorldInfo.h` and `ServerGroupInfo.h`, `GuildInfo2::read` past
the 220 members of `GuildInfo2.h`, and each `push_back(p.release())` leaks if `push_back`
throws. No server receive path reads the two LC packets (the loginserver sends them; the login
fuzz target admits only CL ids) and only `fuzz_replay_game_any_id` reads `SGGuildInfo`, which is
why this ranks low.

**How:** refuse the count with an `InvalidProtocolException` before the first row is read;
`push_back` the owning pointer before releasing it. Rewrite the three entries' Status lines to
say what #381 did and what this PR does.

**Test:** one case per packet in `tests/packet_read_bounds_test.cpp` (`wire_tests`); a
regression input under `tests/fuzz/regressions/` for `SGGuildInfo` where the game target's
any-id replay reaches it. Leak detection runs only in the hand-built `DARKEDEN_BUILD_FUZZERS`
ASan tree, so a leak claim is checked there.

### 7. Fuzz the loginserver's GM datagrams and the stream's wrap-around

**What:** task 5.5's *Next reader*: a UDP target for the loginserver's GM datagrams is not
written, and `tests/fuzz/StreamFuzz.h` says the ring buffer's wrap-around branches in `read()`,
`peek()`, `skip()` and `fill()`'s growth never run, because every input is delivered from the
start of a buffer sized to hold it.

**How:** (a) a third target, `tests/fuzz/fuzz_login_datagram.cpp`, that feeds the input to
`Datagram::read` through `src/Core/DatagramFactoryRead.cpp` as `GameServerManager::run`
(`src/server/loginserver/GameServerManager.cpp`) does after `receive()`, with the client repo's
`tests/fuzz/fuzz_client_datagram.cpp` as the model. `DARKEDEN_FUZZ_TARGETS` in
`tests/fuzz/CMakeLists.txt` lists the targets and its `foreach` makes one `fuzz_replay_<name>`
ctest per target, assuming `StreamFuzz.h`'s input format; a datagram has no stream, so give it
its own replay main or a format switch. (b) a fourth delivery in the two stream targets: place
the input across the buffer's end with `SocketInputStreamTestAccess::PreloadAt`, the helper
`FrameBoundTest.aWrappedFrame*` in `tests/packet_frame_test.cpp` already uses, and assert the
same packets as the whole delivery. A difference is a stream finding, fixed test-first in
`tests/packet_frame_test.cpp`.

**Keep:** 5.5 stays `in progress` for its CI fuzz job, which is blocked like 6.2 (the libFuzzer
runs are manual, [CI](../build/ci.md)); remove only the *Next reader* sentence and record the
first run on the [fuzzing](../protocol/fuzzing.md) page.

### 8. Three login flows that cannot be resumed after a partial write

**What:** `recorded` entries of 2026-10-02: *Character creation can leave partial rows after a
later failure* (`CLCreatePCHandler`), *A failed deletion after Slayer retirement cannot be
resumed by the same request* and *A partial registration cannot be restarted and failed write
acknowledgements retain effects*. Each is a persistence design gap: independent repository
writes with no transaction or compensation, pinned as they are by
`tests/login_character_creation_test.cpp`, `login_character_deletion_test.cpp` and
`login_registration_test.cpp`.

**How:** one design, not three fixes. Add a repository operation per flow that runs its writes
in one transaction (`MySQL*Repository` impls behind the `*Repository.h` seams,
[repository seams](../architecture/repository-seams.md)) or a compensation the retry can finish.
Write the design as a short section in
[character and account flows](../login/character-and-account-flows.md) first and have it
reviewed before the code; `make integration-test` (MySQL 5.7 in docker) is where a rollback is
proven. Keep the phase rules those pages state: a reply is sent before session state is published.

### 9. Smaller recorded items

Each is one PR, with a fix-log entry naming the branch that found it:

- *Live guild creation advances word-sized counters without checking exhaustion*:
  `GSAddGuildHandler` (sharedserver) assigns `getMaxGuildID() + 1` to a word, so 65535 wraps to
  0; refuse at the maximum, with a handler test (`tests/shared_guild_loading_test.cpp` covers
  only startup loading).
- *Character-list HP/MP fields use reversed database-column arguments*: the assembler passes
  `(HP, CurrentHP)` to `setHP(cur, max)`. Fixing it changes what the client's character list
  shows; check the client's `LCPCList` reader expects current then max before swapping.
- *A dragon eye has no place in the relic handlers*: `CGRelicToObjectHandler` has no
  `ITEM_CLASS_WAR_ITEM` branch, so offering a dragon eye disconnects the sender; add the refusal
  (`GCCannotAdd`) and the missing `break` in `deleteRelicEffect`.
- *A dragon eye sent home across groups can outrun its return's three attempts*: the return
  captures the row when it is posted (`war/WarZoneWork.cpp`). Fix: have the retry read the row
  when it runs, or have the delayed add carry the row's save.
- *A war relic's row outlives the run that made it*: a boot sweep of this server's relic rows
  (ids are partitioned per server); nothing reads them, so lowest.
- Not tasks: the *CGPortCheck* registration (`recorded, left as it is`; the entry says the
  registration is not the defect), the native-macOS entries (platform quirks with workarounds),
  and the shared-rules entries, which keep the server's own balance rules; two of them are
  listed under the owner below.

## Blocked or with the owner

- **6.2 Integration tier in CI** is `blocked (no Actions minutes)` in the plan. `make
  integration-test` is the local stand-in; run it before a repository change, and do not add a
  workflow.
- **Shared rules residuals.** Task 3.6 is done; what its slices left over is listed in the
  client repo's `docs/reviews/RESTRUCTURING.md` task 4.12 and needs the owner: the
  advancement-class equip check, a separate castle-tax wire field (the fix log's *Shared castle
  tax* second bullet: the shop reports a ratio of 100 or below as a discount where the handler
  taxes only above 100), pets for any race. Also from the fix log: *Shared equip rules*' wrap
  (a level requirement wraps at 256, so a level-150 item with a +120 option asks for level 14;
  likely unreachable from the seed data, which an agent may check and record), and the party
  bonus that is never granted, a balance decision.
- **Transport encryption**: the native TCP link is unencrypted, login credentials included (the
  WebSocket gateway serves `wss://` with TLS in front). A design decision for the owner, taken
  with the client.

## Done

- **1. `*command LevelWar` edits a zone's schedule from another thread** (PR #385,
  `fix/level-war-command-post`): `opLevelWar` posts `LevelWarManager::manualStart` to the zone's
  group through `de::war::postToZone`; `tests/game_level_war_command_test.cpp` runs the command
  from another thread and sees the queued war pulled forward only when the group drains its
  mailbox.
- **2. `*CTF` races the main thread's flag-war heartbeat** (PR #386, `fix/flag-schedule-lock`):
  `FlagManager` pops the due schedule under a leaf schedule mutex and runs it after releasing it,
  every add and `manualStart` takes the mutex, and `startFlagWar` is handed the war's length;
  `tests/game_flag_schedule_test.cpp` heartbeats and pulls forward on two threads.
- **3. The loginserver's handler dispatch is not wrapped** (PR #387, `fix/login-dispatch-wrap`):
  `LoginPlayer::processCommand` passes a handler's `ProtocolException` and `ConnectException` on
  and turns anything else into a logged `DisconnectException`; `tests/login_handler_failure_test.cpp`
  throws each kind from a handler over a loopback connection.
- **4, one bullet: wolf form rejects every skill** (PR #388, `fix/wolf-form-skills`): the four
  `isAbleToUse*Skill` guards have their `&&` allow-lists again, and the object, tile and inventory
  skill handlers pass the skill being used, so a wolf howls and attacks and casts nothing else;
  `tests/game_wolf_form_skill_test.cpp` pins each guard per skill.
- **5. A slayer's shoulder armour leaving its shape is broadcast as the trousers** (PR #389,
  `fix/shoulder-takeoff-slot`): `Slayer::takeOffItem` and `Slayer::removeShape` take the
  `GCTakeOff` slot from one lookup, `slayerTakeOffSlot`, which names `ADDON_SHOULDER` for the
  shoulder armour; `tests/game_slayer_take_off_test.cpp` pins its slot per item class.
- **4, one bullet: `getRandomMysteriousItem` gives female vampires item type 1 for every class**
  (PR #390, `fix/mysterious-item-class`): the vampire fallback is
  `vampireMysteriousFallbackItemType`, type 1 only for a woman's vampire coat;
  `tests/game_mysterious_item_test.cpp` pins it.
- **4, one bullet: `Tile::deleteCreature(MoveMode)` picks the wrong `deleteObject`** (PR #391,
  `fix/tile-delete-by-mode`): nothing called it, so the overload is declared deleted.
