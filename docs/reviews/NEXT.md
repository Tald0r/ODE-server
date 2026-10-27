---
type: Record
title: "Next work plan"
description: "The ranked, self-contained tasks left in the server after the 2026-10-25 merges, each written so an agent with no other context can take it: the recorded-not-fixed entries of the fix log, the persistence design the login flows need, and what stays blocked or with the owner."
tags: [review, plan]
---

# Next work plan

Written 2026-10-26, after the merges of PRs #374-#383. The [restructuring plan](RESTRUCTURING.md)
has two unticked tasks (5.5 in progress, 6.2 blocked); ratchets R1-R5 and R7-R19 are at 0 and
R6a-j at their baselines (line counts that can still shrink); the [fix log](FIXES.md) holds 432
`fixed` Status lines and 34 `recorded` ones (counted 2026-10-27). The `recorded` entries are
the backlog: each was found while restructuring and left as it was, by the rule that a move
does not change behaviour.
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

- *A dragon eye sent home across groups can outrun its return's three attempts*: the return
  captures the row when it is posted (`war/WarZoneWork.cpp`). Fix: have the retry read the row
  when it runs, or have the delayed add carry the row's save.
- *Any sender can grow `datagram.txt` one line per datagram*: `Datagram::read`
  (`src/Core/DatagramFactoryRead.cpp`) appends an unrotated line for every datagram naming a
  registered id outside the datagram set. Rate-limiting the line, moving it behind
  `DARKEDEN_TRACE` or dropping it is the owner's call; ask before changing what operators see.
- *The three races' guild zone counters share one word with no partition*: a race reaches the
  next one's range after 10000 guilds. Lowest, with the relic rows below.
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

PRs #386-#391 were closed unmerged: their commits reached master as the stack PR #392.

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
- **6. Count gates the login and guild reads still lack** (PR #393, `next-plan/batch-2`): `LCWorldList`,
  `LCServerList` and `GuildInfo2` refuse a count past their factory budget before the first row,
  the four reads append a row before releasing its owner, and the sharedserver's
  `Guild::makeInfo` sends no more members than the read admits; `ListCountTest` in
  `tests/packet_read_bounds_test.cpp` pins the refusals and a full world list. No fuzz regression
  input was added: none fails on master, where the old out-of-bytes refusal and the new gate are
  both protocol errors the target treats as clean.
- **9, one item: a dragon eye has no place in the relic handlers** (PR #393, `next-plan/batch-2`):
  `CGRelicToObjectHandler` refuses a relic it has no branch for with `GCCannotAdd` instead of
  disconnecting the sender, and `deleteRelicEffect`'s `WAR_ITEM` case has its `break`;
  `tests/game_dragon_eye_test.cpp` pins the second.
- **4, the last three bullets** (PR #394, `next-plan/batch-3`): the never-firing checks are not defects
  (the learn-skill handler's implemented-skill list refuses an out-of-range type first, the pet
  item is the one just taken from the inventory, and `opset item_prob_ratio`'s check was turned
  off by an operator in the recovered source). The sharedserver's `GameServerPlayer`, the gameserver's `SharedServerClient` and
  mofus `MPlayer` declare `processCommand(bool) override`, and the unused `DummyQuestStatus` is
  deleted. `MotorcycleLoader::load(Creature*)` drops the motorcycle it parses, and
  `~PlayerCreature` deletes the summoned pet; no test owns the two leaks (no leak checker, and
  neither can be built without a database). The satellite bomb's skill slot cannot be null.
- **9, two items** (PR #394, `next-plan/batch-3`): a full guild id counter founds nothing,
  `decideAddGuild` refuses a race whose next zone id is the last, and only an accepted founding
  claims a guild id (`SharedAddGuild` in `tests/shared_add_guild_test.cpp`, `DecideAddGuild` in
  `tests/shared_guild_decision_test.cpp`).
  The character list sends `(CurrentHP, HP)` and `(CurrentMP, MP)` again, as the original's
  right-to-left argument evaluation most likely did; the client reads current then max and shows
  the maximum (`LoginCharacterList` in `tests/login_character_list_test.cpp`).
- **7. Fuzz the loginserver's GM datagrams and the stream's wrap-around** (`next-plan/batch-4`):
  `tests/fuzz/fuzz_login_datagram.cpp` reads one datagram through `Datagram::read` as
  `GameServerManager::run` does, seeded by `golden2corpus.pl --datagram` and replayed by
  `fuzz_replay_login_datagram`; its first replay found `Datagram`'s raw reads asserting past the
  datagram, which now throw `InsufficientDataException`. The stream targets' fourth delivery
  starts the input short of the ring buffer's end, and their replay found nothing; nor did the
  first five-minute libFuzzer runs of the datagram and login stream targets
  ([fuzzing](../protocol/fuzzing.md)). Task 5.5 stays `in progress` for its CI fuzz job; the
  libFuzzer runs stay manual.
- **9, one item: the first guild founded after a restart reuses a guild zone id**
  (`next-plan/batch-4`): `prepareZoneMaximum` starts each race's counter one past its stored
  maximum (`SharedGuildStartup` in `tests/shared_guild_startup_test.cpp`).
