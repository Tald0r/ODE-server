# Server Restructuring Plan

Living, trackable plan for restructuring the DarkEden server toward the
architecture style of `sidecar` (gcci-labs): strictly layered modules, every
architectural rule owned by a test that fails — never by a memory — and
shrink-only ratchets for legacy debt. No language port; no behavior changes on
the wire or in game rules unless a task says so explicitly.

Modeled on sidecar's `docs/RESTRUCTURING.md` + `CODE_REVIEW` fix-log
conventions.

## How to use this document

- Every task has a checkbox and a `> **Status:**` line. Update the status line
  **in the same commit** as the work it describes (sidecar rule). Allowed
  values: `not started` | `in progress (<what remains>)` | `done (<commit>)` |
  `dropped (<why>)` | `blocked (<on what>)`.
- A status line records the **current** state and what the next reader
  needs to act: conventions, open bugs, what remains. The per-change
  narrative (what moved, what review found, test lists, ratchet deltas)
  belongs in the PR description and commit message, not here.
- A task is only `done` when its **Owner** exists — the test/mechanism that
  keeps the rule true from then on. Landing the change without the owner is
  `in progress (owner missing)`.
- Ratchet numbers (below) may only go **down**. Re-measure with the given
  command before and after a change that claims progress; commit the updated
  number with the change.
- Run the suite locally before every push (`make dev-test` with the pinned
  toolchain). The C++20 workflow also runs the suite and production builds on
  master pushes/merges only; PRs are verified locally to conserve minutes.
  DB-backed integration CI remains deferred.

## Goals / non-goals

**Goals**

1. The client/server wire contract (465 packet factories, shuffle-encryption
   included) pinned by tests so protocol drift is a reviewable diff.
2. Three strictly layered CMake modules — `de-kernel` ← `de-core` ← per-server
   apps — with the layering enforced at build/test time.
3. Game logic testable without a live MySQL or a socket: repositories for
   persistence, an `Outcome` type for gameplay results, pure formula functions.
4. God files decomposed behind routers with declared gating.

**Non-goals**

- Porting to another language. (Analysis 2026-08-29: full rewrite of ~502k
  LOC / 433-packet byte-exact protocol; not economical. The wire pin in
  Phase 1 is the prerequisite for a port anyway, if that ever changes.)
- Rewriting the threading model. One `ZoneGroupThread` per zone group with
  serial in-group processing is sound; Phase 3 codifies it, nothing replaces it.
- Touching the client repo beyond the shared wire-inventory file (Phase 1.4)
  and the vendored de-core subset (task 3.6).

## Ratchets (shrink-only)

Every baseline is what its Command prints; run the commands from the repo
root (bash). R1–R8 were first measured 2026-08-29, R9–R18 as each rule was
introduced, and each cell records what moved the number since.
`tests/ratchet/ratchets.sh` is the enforcing copy of this table — a number
that changes has to change in both places in the same commit.

| # | Metric | Baseline | Command |
|---|--------|---------:|---------|
| R1 | `g_p*` global-singleton extern declarations | 0 | `grep -rE '^extern .*\* g_p' src --include='*.h' --include='*.cpp' \| wc -l` (332→331 on 2026-09-10 with the never-built `EventMonsterNameManager.h`, which redeclared `g_pMonsterNameManager`; 331→327 on 2026-09-10 with the never-built `EventBall.h` (two) and the commented-out `EffectBloodyWallLoader` and `EffectGrayDarknessLoader` declarations; a `default*Repository()` accessor is a function, not a global, so extractions do not move this number; 327→325 on 2026-09-13 with the never-built `item/SubInventory.h` (two); 325→319 on 2026-09-17 with six globals that were declared and never created (`g_pCombatSystemManager`, `g_pItemNumberManager`, `g_pHolyLandRaceBonus`, `g_pObjectRegistry`, `g_pSkillParentInfoManager`, `g_pZonePlayerManager`); 319→315 on 2026-09-17 with the four quest scripting managers, whose only readers were `quest/` and the composition root, and which `ObjectManager` now owns and registers on `de::GameContext`; the remaining nine of the 325→306 span went with the sources no target compiled; 306→285 on 2026-09-17 with the twenty-one `g_pEffect*Loader` globals that were declared and defined but never created and never read; 285→269 on 2026-09-17 with the sixteen `g_pEffect*Loader` globals `EffectLoaderManager` did create, which it now reaches through the `m_pEffectLoaders` table it was already filling; 269→266 on 2026-09-17 with `g_pMonsterNameManager`, `g_pWeatherInfoManager` and `g_pDynamicZoneFactoryManager`, which `ObjectManager` now owns and registers on `de::GameContext`; 266→265 on 2026-09-17 with `g_pBillingPlayerManager`, which went with the billing module; 265→178 on 2026-09-17 with the eighty-seven `g_p*Loader` item globals `ItemLoaderManager` created, which it now reaches through the `m_pItemLoaders` table it was already filling; 178→176 on 2026-09-17 with `g_pVolumeInfoManager` and `g_pDefaultOptionSetInfoManager`, which `ObjectManager` now owns and registers on `de::GameContext`; 176→89 on 2026-09-17 with the eighty-seven per-item-class `g_p<Class>InfoManager` globals `ItemInfoManager` created, which it now reaches through the `m_InfoClassManagers` table it was already filling; 89→88 on 2026-09-17 with `g_pLuckInfoManager`, whose class, global and bodies all sat inside a comment block; 88→72 on 2026-09-17 with sixteen managers `ObjectManager` creates that at most five files read — thirteen of them registered on `de::GameContext`, three that nothing outside `ObjectManager.cpp` reads left as plain members; 72→63 on 2026-09-17 with nine more: `g_pItemLoaderManager`, `g_pCastleSkillInfoManager`, `g_pTimeChecker` and `g_pGameServerGroupInfoManager`, four managers `ObjectManager` creates that at most five files read; `g_pObjectManager`, `g_pThreadManager` and `g_pClientManager`, which `GameServer` creates and now holds as members; `g_pConnectionInfoManager`, which `IncomingPlayerManager` creates and holds; and `g_pGameServer`, which only its own `main()` read and which is now a local there; 63→61 on 2026-09-17 with `g_pLoginServer` and `g_pSharedServer`, each read only by its own `main()` and now a local there; 61→49 on 2026-09-18 with the twelve managers a subsystem now takes from `de::GameContext`: the eleven `ObjectManager` creates that at most nine files read (`g_pItemMineInfoManager`, `g_pTimeManager`, `g_pPriceManager`, `g_pEffectLoaderManager`, `g_pAlignmentManager`, `g_pGlobalPartyManager`, `g_pCombatInfoManager`, `g_pMasterLairInfoManager`, `g_pBloodBibleBonusManager`, `g_pCoupleManager`, `g_pDynamicZoneManager`), now its members, and `g_pIncomingPlayerManager`, which `ClientManager` creates and holds; 49→48 on 2026-09-18 with `g_pLogClient`, which all three `main()`s assigned and only commented-out bodies read, made a `LogClient.cpp` static behind `openLogClient()`/`logClient()` and deleted with the rest of the log client on 2026-09-22; 48→38 on 2026-09-18 with ten globals the login and shared servers retired: the seven the loginserver's `LoginServer`, `ClientManager` and `LoginPlayerManager` now hold as members and register on `de::LoginContext` (`g_pGameServerGroupInfoManager`, `g_pGameServerManager`, `g_pLoginPlayerManager`, `g_pReconnectLoginInfoManager`, `g_pUserInfoManager`, `g_pZoneGroupInfoManager` and `g_pZoneInfoManager`; the first, second and last are the loginserver's own copies of names the shared server or the gameserver declares separately), and three nothing outside their creator reads, left as plain members: `g_pClientManager` and `g_pItemDestroyer` on `LoginServer`, `g_pHeartbeatManager` on `SharedServer`; 38→36 on 2026-09-22 with `g_pItemInfoManager` and `g_pFlagManager`, the two managers `ObjectManager` creates that the rest of the gameserver now takes from `de::GameContext` — the item info manager 130 files read, the flag war manager 14; 36→35 on 2026-09-22 with `g_pSkillInfoManager`, the skill info manager `ObjectManager` creates that the rest of the gameserver now takes from `de::GameContext` — 237 files read it, 221 of them under `skill/`; 35→34 on 2026-09-22 with `g_pZoneInfoManager`, which `ObjectManager` already registered on `de::GameContext` and now holds as a member: its twenty-nine remaining readers take it from the context; 34→33 on 2026-09-22 with `g_pZoneGroupManager`, which `ObjectManager` already registered on `de::GameContext` and now holds as a member: its thirty-six remaining readers take it from the context; 33→32 on 2026-09-22 with `g_pPCFinder`, which `ObjectManager` already registered on `de::GameContext` and now holds as a member: its forty readers take it from the context, and every critical section over it still locks the same object; 32→31 on 2026-09-22 with `g_pCastleInfoManager`, the castle info manager `ObjectManager` creates, now its member and registered on `de::GameContext` at the creation line — forty-one files read it, nine of them quest actions that take it from the context their factory handed them; 31→30 on 2026-09-22 with `g_pWarSystem`, the war system `ObjectManager` creates, now its member and registered on `de::GameContext` at the creation line — thirty-two files read it, three of them quest actions that take it from the context their factory handed them; 30→29 on 2026-09-22 with `g_pItemFactoryManager`, which `ObjectManager` already registered on `de::GameContext` and now holds as a member: its fifty-one remaining readers take it from the context, and the `CREATE_ITEM` macro that hid the global is gone, expanded at the two newbie-kit sites it served; 29→28 on 2026-09-22 with `g_pMonsterInfoManager`, the monster info manager `ObjectManager` creates, now its member and registered on `de::GameContext` in the registration block below the creation lines — twenty-one files read it, three of them quest actions that take it from the context their factory handed them; 28→27 on 2026-09-22 with `g_pOptionInfoManager`, the item option info manager `ObjectManager` creates, now its member and registered on `de::GameContext` in the same block — nineteen files read it, two of them quest actions that take it from the context their factory handed them; 27→26 on 2026-09-22 with `g_pPKZoneInfoManager`, the PK zone info manager `ObjectManager` creates, now its member and registered on `de::GameContext` at the creation line — sixteen files read it, one of them a quest action that takes it from the context its factory handed it; 26→20 on 2026-09-22 with the shared server's six, every one of them created by `SharedServer`, which now holds them as members — `g_pGuildManager`, `g_pGameServerManager` and `g_pStringPool` are registered on `de::SharedContext`, which the eight guild handlers, `GuildStepRunner` and the `GameServerManager` worker read them through now, and `g_pGameServerInfoManager`, `g_pGameServerGroupInfoManager` and `g_pResurrectLocationManager` are plain members, nothing outside `SharedServer.cpp` reading them; each of the six is a name the gameserver, the loginserver or `src/server` declares separately, so no other binary's copy moved; 20→19 on 2026-09-22 with `g_pVariableManager`, which `ObjectManager` already registered on `de::GameContext` and now holds as a member: its seventy-five remaining readers take it from the context; 19→18 on 2026-09-22 with the gameserver's `g_pStringPool`, which `ObjectManager` already registered on `de::GameContext` and now holds as a member: its seventy remaining readers take it from the context, and the sharedserver's separate copy of the name stays; 18→17 on 2026-09-22 with the gameserver's `g_pGuildManager`, the guild manager `ObjectManager` creates, now its member and registered on `de::GameContext` at the creation line — sixty-three files read it, twelve of them quest elements: the eleven actions take it from the context their factory handed them, the reinforcement condition reads the process-wide one, and the sharedserver's separate manager of the same name stays; 17→16 on 2026-09-23 with `g_pSkillHandlerManager`, the skill handler table `ObjectManager` creates, now its member and registered on `de::GameContext` in the registration block below the creation lines — fourteen files read it: the monster and its AI, the PC manager, the bloody warp skill, nine skill and attack handlers, and one quest action that takes it from the context its factory handed it; 16→15 on 2026-09-23 with `g_pSharedServerManager`, the link to the shared server `GameServer` creates, now its member and registered on `de::GameContext` at the creation line, started, stopped and joined in the same order as before — thirteen files read it: the game player, the morph event, the restore skill, the GM guild commands, the connect handler and seven guild handlers, and one quest action that takes it from the context its factory handed it; 15→14 on 2026-09-23 with `g_pParkingCenter`, the motorcycle park `ObjectManager` creates, now its member and registered on `de::GameContext` at the creation line — twelve files read it: the client manager's heartbeat, the inventory, item rack, player creature, slayer and zone item key paths, the shop buy and sell handlers, the use-item and quick-slot potion handlers, and two motorcycle quest actions that take it from the context their factory handed them; 14→13 on 2026-09-23 with `g_pLevelWarZoneInfoManager`, the level war zone table `ObjectManager` creates, now its member and registered on `de::GameContext` at the creation line — twelve files read it: the creature manager, the level war manager, the sweeper bonus table, the three race stat files, the PC manager, the zone spawn and zone utility code, the waypoint handler, and a quest action that takes it from the context its factory handed it with the level war condition beside it, which reads the process-wide one; 13→12 on 2026-09-23 with `g_pLoginServerManager`, the link to the login server `GameServer` creates, now its member and registered on `de::GameContext` at the creation line, started, stopped and joined in the same order as before — eleven files read it: the connection info manager's server-info datagram, the guild union, the incoming player manager, the zone group manager's lock pair, the GM server commands, the guild chat, logout and whisper handlers, the incoming connection and kick handlers, and the siege war record; the kick handler's read sits inside its mailbox command, which captures by value, so it reads the accessor there rather than a reference bound outside; 12→11 on 2026-09-23 with `g_pShrineInfoManager`, the guardian shrine table `ObjectManager` creates, now its member and registered on `de::GameContext` at the creation line — eleven files read it: the castle info table, the creature utility, the blood bible and relic position effects, the reload info event, the zone spawn, the GM console commands, the dissection and relic handlers, the war system and the race war, whose end step reaches it three times and binds one reference; 11→10 on 2026-09-23 with `g_pHolyLandManager`, the holy land table `ObjectManager` creates, now its member and registered on `de::GameContext` at the creation line — nine files read it: the blood bible and relic position effects, the PC manager, the regen zone manager, the relic utility, the guardian shrine table, the zone load, the race war and the war system; no function reaches it three times, so all nine read the accessor inline, and the four commented-out mentions of the name stay as they were; 10→9 on 2026-09-23 with the gameserver's `g_pResurrectLocationManager`, the resurrect location table `ObjectManager` creates, now its member and registered on `de::GameContext` at the creation line — nine files read it: the three race files, the PC manager, the zone and the zone utility, the connect and logout handlers, and one quest action that takes it from the context its factory handed it; the zone utility's transport reaches it three times and binds one reference, the three race files and the zone read the accessor inline so their pinned line counts do not move, and the sharedserver's separate class of the same name, already a plain member, is untouched; 9→8 on 2026-09-23 with `g_pSweeperBonusManager`, the sweeper bonus table `ObjectManager` creates, now its member and registered on `de::GameContext` at the creation line — nine files read it: the reload info event, the level war manager, the monster manager, the three race stat files, the sweeper set, the zone spawn and the waypoint handler; no function reaches it three times, so all nine read the accessor inline; 8→6 on 2026-09-23 with the mofus pair, `g_pMPacketManager` and `g_pMPlayerManager`, which `GameServer` creates where the mofus module is built, now its members and registered on `de::GameContext` at the creation lines, the player manager started, stopped and joined in the same order as before — two files read them: the mofus player, whose command loop reaches the packet table five times and binds one reference, and the power point handler, which reads the player manager's accessor inline inside the block the module's macro guards; the members stay null where the module is not built, as the globals did, and the accessors assert there rather than handing back a null pointer; 6→4 on 2026-09-23 with the packet factory table and the packet validator, the two managers whose classes live in `src/Core` that each server creates a set of for itself — each server object now holds them as members, created, initialised and deleted in the mirrored positions, and registers them on `de::KernelContext`, a registry that is itself a kernel file so that a Core reader can take the factory table from it, which rule K1 would never let a header under `src/server/` offer; seven files read them: the datagram receive path, the three per-link packet loops (the game player, the login player and the game-server player) and the shared-server client, each of which reaches the factory table three or more times and binds one reference, while the two validator reads are inline; `datagram_frame_test` registers its one-factory table on the context instead of assigning the global; 4→3 on 2026-09-23 with `g_pGameWorldInfoManager`, the world table each binary creates one of — the game server's `ObjectManager`, the login server and the shared server now hold it as a member, created, initialised or loaded and deleted in the mirrored positions, and register it on `de::ServerContext`, the registry for the managers ServerCore defines; five files read it: the lottery handler's world name, the login server's client-manager reload, the world-selection topology and the world-list and character-name handlers; no function reaches it three times, so all five read the accessor inline, and the reload's null test is gone because the accessor asserts on a table the startup code has not registered; 3→2 on 2026-09-23 with `g_pDatabaseManager`, the per-thread connection table each binary creates one of — each server object now holds it as a member, created, initialised and deleted in the mirrored positions, and registers it on `de::ServerContext`; sixty-four files read it, all but two of them a `repository/` implementation or the `NEW_STMT` macro in `database/DB.h`, where the table is reached once per statement; only the zone-group thread's run loop and the integration suite's `main` reach it three or more times, the zone thread binding one reference and every other reader reading the accessor inline; the SQL escaper's test for a null table is gone with the global, as is `de::GameContext`'s own registration of it, which no reader ever took; the integration suite registers the table it builds on `de::ServerContext` instead of assigning the global; 2→1 on 2026-09-23 with `g_pConfig`, the configuration each server loads before its server object exists — each `main()` now holds it as a local and registers it on `de::KernelContext` at the creation line, and nothing deletes it, as nothing did before; seventy-three files read it, eighteen of them in a function that reaches it three or more times and binds one reference — the database manager's startup, the four worker-thread run loops, the connection, incoming-player and shared-server heartbeats, the zone load, the monster and variable tables, the SMS queue, the connect and select-PC handlers and the login link's constructor — while the rest read the accessor inline; `de::GameContext` keeps its own configuration accessor, which the ctf subsystem takes from the context handed to it, and registers it from the kernel context now; the non-PK check in `skill/SkillUtil.cpp` names its two property reads so the accessor fits on one line, keeping that file's pinned count; 1→0 on 2026-09-23 with `g_pGameServerInfoManager`, the game-server table the game server and the login server each create one of — each server object now holds it as a member, created, initialised and deleted in the mirrored positions, and registers it on `de::ServerContext`; thirteen files read it: the guild union, the GM world command, the guild-chat, whisper and select-PC handlers, the packet and zone utilities, the player creature, the non-PK check in the skill utilities, the siege war and the login player; the three functions that walk the whole table reach it three times and bind one reference, the rest read the accessor inline. The ratchet holds at zero now: a new `g_p*` extern fails it) |
| R2 | Files with inline SQL in gameserver root | 0 | `grep -lE 'executeQuery' src/server/gameserver/*.cpp src/server/gameserver/*.h \| wc -l` (non-recursive on purpose: a `repository/` MySQL impl does not count — R2 measures SQL *leaving the game logic*. Textual, so a commented-out `executeQuery` still counts. Baseline 104 on 2026-08-29; 7→0 on 2026-09-10, the last two live sites into `PlayRecordRepository::logPlayerTrade` and the new `SMSMessageRepository`, `CreatureUtil.cpp`'s commented-out `addOlympicStat` body deleted, and four never-built stale copies deleted with it. The root is clean; new SQL there fails the ratchet.) |
| R3 | Files with inline SQL outside `database/` and any `repository/` | 0 | `grep -rlE 'executeQuery' src --include='*.cpp' \| grep -v 'server/database' \| grep -v '/repository/' \| wc -l` (18→11 on 2026-09-10 with the seven gameserver-root files R2 counted; 11→0 the same day with the never-built `EventBall.cpp`, the `*notice` command that held the last live statement, and the nine files whose only `executeQuery` sat inside a comment block. `gameserver/repository/` joined the exclusion on 2026-09-01, 317→314: a seam that quarantines four tables from two files would otherwise *raise* a shrink-only ratchet; the loginserver's, sharedserver's and ServerCore's `repository/` directories were admitted on 2026-09-07 before they existed, so the count did not move. Textual — see the comment policy under 3.2. Counts unbuilt files and the other binaries' game logic too.) |
| R4 | Packet headers with `execute()` still on the packet | 0 | `grep -rlE 'void execute\(Player' src/Core --include='*.h' \| wc -l` |
| R5 | `__BEGIN_TRY` control-flow macro sites in de-core candidates | 5,131 | `grep -rE '__BEGIN_TRY' src/server/gameserver --include='*.cpp' \| grep -vE 'gameserver/(gm\|handler\|packetfill)/' \| wc -l` (handler/ and packetfill/ hold 2.4-moved sources from `src/Core`, never counted while they lived there; `gm/` joined them with task 4.1, holding the GM command bodies that moved out of `handler/CGSayHandler.cpp` — the 33 macro pairs in them are the same handler bodies at a new address, so the number did not move. Fold them in with a re-baseline when they become 3.x extraction targets. 5,984→5,980 on 2026-09-02: the four macros inside the guild trio's deleted dead __SHARED_SERVER__ blocks. 5,980→5,899 on 2026-09-02, textual: ItemIDRegistry.cpp's 81 hand-expanded initItemIDRegistry bodies collapsed onto one macro, so the grep sees one #define line instead of 82 matched lines — 81 expansions plus the old macro's own; each method still has its try block. 5,897→5,790 on 2026-09-05: the never-built `gameserver/test/`, `testAlone/`, `mofus/testserver/` and `quest/Squest/` trees were deleted. 5,790→5,788 on 2026-09-08: the never-built `skill/Restore2.cpp`, a stale duplicate of `skill/Restore.cpp`, was deleted. 5,788→5,755 on 2026-09-08: the never-built `Vampire_backup.cpp`, a stale copy of `Vampire.cpp`, was deleted. 5,755→5,737 on 2026-09-10: the never-built `EventMonsterNameManager.cpp` (4), `GameServerInfoManager.cpp` (7) and `GameWorldInfoManager.cpp` (7) were deleted. 5,737→5,719 on 2026-09-10: the never-built `EventBall.cpp` (10) and `EventQuestRewardManager.cpp` (1) were deleted, and seven more sat in commented-out or empty bodies deleted from `mission/`, `skill/` and `war/`. 5,719→5,701 on 2026-09-13: the never-built `item/SubInventory.cpp` (10) and `war/SubInventoryItemPosition.cpp` (8) were deleted. 5,701→5,685 with the 4.3 hoist: 24 sites left `Slayer.cpp`/`Vampire.cpp`/`Ousters.cpp` with the bodies that moved to `PlayerCreature.cpp`, which carries 8 of them now that the three copies are one. 5,685→5,677 with the second 4.3 hoist: 12 sites left the three race files with `setGoldEx`, `getExtraInfo`, `getInventoryInfo`, `canPlayFree` and `isPayPlayAvaiable`, and `PlayerCreature.cpp` gained 4 of them — its own `isPayPlayAvaiable` already had one. 5,677→5,673 with the never-defined region macros: the two in EventShutdown.cpp's deleted branch. 5,673→5,483 on 2026-09-17: the thirty-nine sources no target compiled were deleted, and 190 of the sites sat in them. 5,482 → 5,474 with the never-defined feature macros: eight sat in the commented-out `NPC.cpp` SimpleQuest and `PlayerCreature.cpp` quest bodies that went with them. 5,473→5,437 with the dead billing module (32 sites) and the bodies that fed it: `GamePlayer::sendBillingLogin` (1), `PlayerCreature::isBillingPlayAvaiable` and `canPlayFree` (2), and `SkillUtil.cpp`'s empty `checkFreeLevelLimit` (1). 5,437→5,434 on 2026-09-17 with the deleted `LuckInfo.cpp`, whose three sites sat in its commented-out body, which this textual measure counts. 5,434→5,211 on 2026-09-17: 223 sites sat inside the commented-out bodies the R18 pass removed. 5,211→5,204 on 2026-09-18: seven more sat in the commented-out bodies deleted from `couple/`, `gm/`, `mission/` and `war/`. 5,204→5,201 on 2026-09-22: the vampire and ousters slot constructors, destructor and run-time bodies are three on their shared `skill/RaceSkillSlot.cpp` where they were six across the two race slot files. 5,201→5,193 on 2026-09-22 with the slot-table hoist: 12 sites left the three race files with the bodies that moved to `PlayerCreature.cpp`, which carries 4 of them now that the three copies are one; 5,193→5,170 on 2026-09-22: twenty-three more sat in the commented-out bodies deleted from the gameserver's top-level files; 5,170→5,169 on 2026-09-23 with the dead `__SHARED_SERVER__` blocks the gameserver's `GuildManager.cpp` carried, which no build of that file defines; 5,169→5,168 on 2026-09-23: the deleted WarSystem::isEndCondition, which had no caller; 5,168→5,167 on 2026-09-23 with the exps hoist, which leaves one body on `PlayerCreature.cpp` where the vampire and ousters files each had one; 5,167→5,166 on 2026-09-23: the deleted `CastleShrineInfoManager::isDefenderOfGuardShrine`, which had no caller; 5,166→5,165 on 2026-09-23 with the silver-damage hoist, one body on `PlayerCreature.cpp` where the vampire and ousters files each had one; 5,165→5,163 on 2026-09-23 with the item-load hoist: the three `loadItem(bool)` bodies are one on `PlayerCreature.cpp`, and the per-race hooks it leaves behind carry no try block; 5,163→5,161 on 2026-09-24: the gameserver `StringPool`'s `clear()` and `addString()` went with the piecewise filling they were for, the pool now building a whole table and publishing it in one swap; 5,161→5,160 on 2026-09-24: the deleted `WarSystem::getActiveRaceWar`, whose one caller now asks the race war its question under the war system's own lock rather than carrying the pointer past it; 5,160→5,158 on 2026-09-24: `GuildUnion::create()` and `destroy()` went, the union manager writing the union rows itself now that a `GuildUnion` is an in-memory object only; 5,158→5,155 on 2026-09-24: `IncomingPlayerManager::getPlayer`, `getPlayer_NOBLOCKED` and `getReadyPlayer` went, their only callers, the login link's two incoming-connection handlers, now reaching the player through its mailbox rather than a pointer handed out past the manager's lock; 5,155→5,152 on 2026-09-24: `WarSystem::getActiveWar` and `getActiveWarSchedule`, which handed a running war out past the lock that frees it, went, and `isModifyCastleOwner` asks the war under that lock without a try block; 5,152→5,147 on 2026-09-24: a castle's tax balance is changed and saved in one place, `CastleInfoManager::increaseTaxBalance`/`decreaseTaxBalance`, without a try block, and `CastleInfo`'s two `Ex` forms and the unused whole-row `CastleInfoManager::save` went; 5,147→5,146 on 2026-09-24: `CastleShrineInfoManager::addShrineShield_LOCKED` folded into `addShrineShield`, which a castle war's end now posts to the guard zone's group rather than calling under the zone's mutex; 5,146→5,134 on 2026-09-24: the twelve loops the race war's start and end ran over other threads' zones -- the holy land's four, the castles' three, the shrine shields' two, the regen towers' two and `ZoneGroupManager::removeFlag` -- became posts to the owning groups (`war/WarZoneWork.h`) without a try block; 5,134→5,131 on 2026-09-25: the holy land's, a castle's and a level war's bonus-zone broadcasts became posts of a captured packet (`de::war::postBroadcast`) without a try block) |
| R6 | Line count of god files (each tracked separately) | see table below | `wc -l <file>` |
| R7 | Files using parenthesized `throw(...)` syntax — dynamic specifications plus expressions, see 5.4 | 0 | `grep -rlE 'throw[[:space:]]*\(' src --include='*.h' --include='*.cpp' \| wc -l` (real throw expressions were normalized to `throw expr`, making every future match unambiguously forbidden legacy syntax) |
| R8 | Non-comment lines using `__PRETTY_FUNCTION__` | 0 | `grep -rh '__PRETTY_FUNCTION__' src --include='*.h' --include='*.cpp' \| grep -vcE '^[[:space:]]*//'` (call-site diagnostics take the enclosing function from a defaulted `std::source_location` — see docs/TOOLCHAIN.md, "Diagnostics without location macros". Line-based: a line whose first non-blank text is `//` is a comment, so the comments that explain the equivalence may still name the macro) |
| R9 | Hand-written length-prefixed string reads left in `src/Core` | 0 | `grep -rhE 'iStream\.read\([A-Za-z_][A-Za-z0-9_]*, sz[A-Za-z0-9_]*\);' src/Core --include='*.h' --include='*.cpp' \| grep -vcE '^[[:space:]]*//'` (a string field is a BYTE length then that many bytes; `de::wire::readString`/`writeString` in `src/Core/WireString.h` carry it with the bounds stated once. The read may land in a member or in a local, so any identifier counts. Line-based with R8's comment rule, so `WireString.h`'s own example of the shape it replaces does not count itself) |
| R10 | Throws of a pointer into a local string, and the handlers that caught one | 0 (R10a), 0 (R10b) | R10a: `grep -rnE 'throw [A-Za-z_]+\.c_str\(\)' src \| wc -l` — a `throw x.c_str()` hands the handler storage that dies with the clause it came from; `END_DB`/`END_DB_EX` answer a failed statement with a `DatabaseError` (`src/server/database/DatabaseError.h`) that owns its message, so nothing in the tree has that shape. R10b: `grep -rn 'catch (const char\*' src \| wc -l` — the receiving end. All 34 handlers name `DatabaseError`, so the count is 0 and one left behind would either be dead or be reaching for one of the ~160 bare `throw "literal"` sites, which are a separate defect and are not answered this way. Textual, so a commented-out clause counts (the one inside `CGPortCheckHandler.cpp`'s commented-out retry moved with the live code) |
| R11 | Bare string-literal throws left in `src` | 0 | `grep -rh 'throw "' src --include='*.h' --include='*.cpp' \| grep -vcE '^[[:space:]]*//'` — a `throw "text"` puts a `const char*` on the stack, a type nothing catches (R10b) and neither `__END_CATCH` nor the swallowing `__END_CATCH_NO_RETHROW` matches, so it walks past every handler the surrounding code wrote and lands in a `catch (...)` backstop — or, thrown out of a destructor, in `std::terminate`. `throw Error("text")` reaches the handler written for it. 149→0 over two rounds (see `docs/FIXES.md`): the 62 sites outside `src/server/gameserver/item`, then the 87 item constructors that answered a failed `isPossibleItem` check with the identical `Invalid item type or optionType` literal, one per item class. Line-based with R8's comment rule, so a commented-out throw does not count |
| R12 | Throw messages carrying non-ASCII text | 0 | ``LC_ALL=C grep -rhE $'throw[[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\\([[:space:]]*"[^"]*[^\x01-\x7f]' src --include='*.h' --include='*.cpp' | grep -vcE '^[[:space:]]*//'`` — a throw's message is a diagnostic read in a log or on a console, and the tree's code language is English; the legacy messages were code-page bytes that survived the migration as Korean, mojibake or U+FFFD runs. The class is spelled as the bytes it excludes so that the CR of a CRLF working tree is not counted, the way `[^[:print:]]` would; line-based with R11's comment rule, so a literal that starts on a continuation line is not counted |
| R13 | Duplicated include guard names across `src/**/*.h` | 0 | Each header's **first** `#ifndef` is its guard; the ratchet takes that name from every header (`find -exec awk`, stripping the CR of the CRLF tree) and counts `sort \| uniq -d`. Two headers sharing a guard means whichever one a translation unit reaches first silently swallows the other: the second `#include` expands to nothing and its declarations surface as an unrelated "undeclared identifier" far from the cause, which is how `EffectCallMotorcycle.h` reusing `EffectDecayItem.h`'s guard bit the `Zone` split. 16→0: eighteen headers took a name derived from their path, per-server twins (`sharedserver/SharedGameServerInfo.h` → `__SHARED_SERVER_GAME_SERVER_INFO_H__`) included, because which headers share a binary changes. Not measured on every `^#ifndef` line — ordinary macros are tested that way too (`Core/Types.h` tests `__XMAS_EVENT_CODE__` before defining it), so that count could never reach zero |
| R14 | Mentions of macros nothing defines | 0 | `LC_ALL=C grep -rhE '__((THAILAND\|THIALAND\|CHINA\|CHAINA\|INTERNATIONAL\|NETMARBLE\|TEST)_SERVER\|OLD_GUILD_WAR\|CONNECT_BILLING_SYSTEM\|COUT_BILLING_SYSTEM\|PAY_SYSTEM_(ZONE\|LOGIN\|FREE_LIMIT)\|UNDERWORLD\|ACTIVE_QUEST\|ACTIVE_SERVICE_DEADLINE\|WINDOWS)__' src --include='*.h' --include='*.cpp' \| wc -l` — a macro no build defines makes the block behind it dead text: it never reached the compiler, so the `#else`/`#ifndef` branch beside it was the only code the servers ran, while the block kept reading as live code. A translation-unit-local `#define` arms one again, and one did, giving that TU a `SystemAvailabilitiesManager` with a layout no other TU shared. The region builds went first (`__THAILAND_SERVER__`, `__CHINA_SERVER__`, the misspellings `__CHAINA_SERVER__`/`__THIALAND_SERVER__`, `__INTERNATIONAL_SERVER__`, `__NETMARBLE_SERVER__`, `__TEST_SERVER__`); the feature macros followed — `__OLD_GUILD_WAR__` (guild union/tax handlers that only answered "not supported yet", a one-attacker war schedule where the live read takes five), `__CONNECT_BILLING_SYSTEM__`/`__COUT_BILLING_SYSTEM__` (the external billing link and its console trace), `__PAY_SYSTEM_ZONE__`/`__PAY_SYSTEM_LOGIN__`/`__PAY_SYSTEM_FREE_LIMIT__` (with none defined `GamePlayer::isPayPlaying()` answers true for every player and each gate passes), `__UNDERWORLD__`, `__ACTIVE_QUEST__`, `__ACTIVE_SERVICE_DEADLINE__` and `__WINDOWS__`, whose arm of the platform switch was the only dead one (`__LINUX__` comes from the top-level `CMakeLists.txt`, `__APPLE__` from the compiler). The instrumentation toggles a developer switches on by hand (`__PROFILE_*`, `__FULL_PROFILE__`, `__DEBUG_OUTPUT__`, `__OUTPUT_INIT__`) are deliberately not counted. Comments count too, so a comment about one of these branches states the behaviour instead |
| R15 | `src/**/*.cpp` that no target compiles | 0 | A source no target names is never compiled, so nothing it says is true of a running server: it drifts out of sync with the headers it includes while still reading as live code. Built means a `CMakeLists.txt` names it, `tests/arch/kernel_files.txt` lists it, or another source `#include`s it. A CMake reference is relative to its own `CMakeLists.txt`, so each is resolved against that directory — a basename match would call `gameserver/SocketImpl.cpp` built because the kernel list carries `Core/SocketImpl.cpp` — and comments are stripped first, since two quest sources sat behind a `#` in a source list. The scan is scoped to `src` plus the top-level `CMakeLists.txt` because the container configures its build tree inside the source root, and a `file(GLOB ...)` in the build files fails the check outright, a name-based measure being blind to a globbed target. 39→0 on 2026-09-17 |
| R16 | Headers under `src/` that nothing includes | 0 | A header no translation unit reaches is not part of any build, so nothing it declares is ever checked against the code it describes. Included means some `#include "..."` text under `src/` or `tests/` equals the header's path or the path ends with `/` plus that text. That approximates resolving each include against the including file's own directory and the CMake `-I` list, and it errs only toward calling a header used, never toward calling a live one dead; comments are not stripped, for the same direction. The include-text list is materialised first, as R13 does, so a broken grep fails loudly instead of reading as zero orphans. 5→0 on 2026-09-17 |
| R17 | Source lines carrying a non-ASCII byte | 0 | `LC_ALL=C grep -rhE $'[^\x01-\x7f]' src --include='*.h' --include='*.cpp' \| wc -l` - the tree's code language is English, and these are the lines a reader cannot read. The legacy text survived in three states: readable Korean, mojibake (EUC-KR/CP949 bytes, or the Chinese build's GBK bytes, decoded as Latin-1 or CP949 and re-encoded as UTF-8), and U+FFFD runs where the text is gone and only the adjacent code says what it meant. Comments are translated tree by tree, and the last of them -- the gameserver's `skill`, `quest` and `item` trees -- is done, so no comment carries legacy text any more. String literals were a pass of their own, since changing one changes what the server says, not how the source reads; the last of them went on 2026-09-25, and the count holds at zero: a non-ASCII byte in `src` is new debt. A literal that data is matched against and that must keep foreign bytes (the reserved staff titles a character name may not contain, the chief-monster prefix, the GM chat aliases the command ladder still tests) is written as escaped UTF-8 bytes with an English comment beside it, so the file stays ASCII and the reader still knows the word. The byte class is R12's, so the CR of a CRLF working tree stays out of the count. 578→577 with the blood-drain log call that named the draining monster in Korean, deleted with the rest of the log client's call sites; 576→575 on 2026-09-24 with `WarSchedule::save`'s failure log, which repeated `create()`'s Korean text under `create()`'s name; 575→572 on 2026-09-24 with `CastleShrineInfoManager::canPickupCastleSymbol`'s three Korean failure logs, two translated and one gone with the war-type branch the castle-war test made redundant; 572→569 on 2026-09-24 with `RegenZoneManager::reload`'s three Korean failure logs, translated when the per-tower half moved to the tower's zone thread (`reloadOwner`); 569→457 on 2026-09-25 with the log, debug and player-message literals of the remaining top-level gameserver files, its `skill` and `item` trees, `GameTime::toString` and the login and shared servers, translated after the mojibake among them was recovered, while `kInvalidID`'s Korean staff titles stay as escaped UTF-8 bytes because typed names are matched against them; 457→313 on 2026-09-25 with the CG handlers', war, ctf and mission trees' literals, translated once their mojibake -- much of it the Chinese build's GBK read as CP949 or Latin-1 -- was recovered, the donation nicknames shortened to fit the wire's 22 bytes on the way; 313→173 on 2026-09-25 with the GM commands' lost replies written from the code they answer for, the quest actions' place names given the map files' spellings, and the chief-summon prefix kept as escaped bytes because monster names are matched against it; 173→0 on 2026-09-25 with the variable descriptions' units, the GDR lair's lost state logs written from their transitions, the guild quest elements' messages translated and the unread Santa and Rudolf taunts deleted |
| R18 | Commented-out code lines inside multi-line `/* */` blocks | 5 | A character-level perl scan (see `tests/ratchet/ratchets.sh`) over every `src/**/*.{cpp,h}`: lines inside a multi-line `/* */` comment that read as a statement — ending in `;`, `{` or `}`, a preprocessor directive, opening with a control keyword or type name, or an identifier followed by `(`. Switched-off code names deleted globals and signatures that changed, and reads as if it described the code beside it. The same test over `//` lines called 13 of 50 sampled prose lines code — section banners, end-of-block markers, sentences opening with `delete` — so only `/* */` blocks are counted; `//` commented-out code is removed without being measured. 10639→5281 on 2026-09-17. 5,279→3,301 on 2026-09-18, with `src/Core`, `src/server/database`, the login and shared servers, the files directly under `src/server` and the gameserver's `handler/`, `gm/`, `war/`, `mofus/`, `mission/`, `couple/`, `ctf/`, `packetfill/`, `repository/` and `exchange/` trees. 3,301→5 on 2026-09-22 with the gameserver's top-level files, `src/server/LogClient.cpp`, `src/domain/SkillOutputFormulas.cpp` and three scattered files; the five lines left are the two block comments in `handler/CGWhisperHandler.cpp` that describe the whisper relay field by field, documentation that only reads as code |

God-file baselines (R6):

All rows re-measured 2026-08-31 post-clang-format-18 (the 08-29 numbers
predated that pass) and again 2026-09-05; only the rows `ratchets.sh` names
are enforced so far.

| File | Baseline lines |
|------|---------------:|
| `src/server/gameserver/Zone.cpp` | 1,265 (1,273 before the compiler-warning cleanup; was 9,350 before the 4.2 extractions; enforced by `ratchets.sh` R6g) |
| `src/server/gameserver/skill/SkillUtil.cpp` | 678 (684 before the skill range moved to de-core; was 6,626 before the split by concern into `SkillDamage.cpp` / `SkillExperience.cpp` / `SkillGeometry.cpp`, leaving the mana and HP costs, the slot run-time and zone-level gates, the skill-failure packets and the elemental lookups; under the 2,000-line phase exit criterion, so R6a pins it rather than baselining a god file; enforced by `ratchets.sh` R6a) |
| `src/server/gameserver/InitAllStat.cpp` | 230 (was 4,787 before the split by race into `SlayerStat.cpp` / `VampireStat.cpp` / `OustersStat.cpp`, leaving `PlayerCreature::applyBloodBibleSign` and `Monster::initAllStat`; under the 2,000-line phase exit criterion, so R6b pins it rather than baselining a god file; enforced by `ratchets.sh` R6b) |
| `src/server/gameserver/handler/CGSayHandler.cpp` (moved from `src/Core` in 2.4) | 111 (114 before the compiler-warning cleanup; was 4,720 before the 4.1 command extraction; enforced by `ratchets.sh` R6e) |
| `src/server/gameserver/gm/ConsoleCommands.cpp` | 1,574 (the 61 `*command` sub-command bodies, one function per name; enforced by `ratchets.sh` R6f) |
| `src/server/gameserver/Slayer.cpp` | 2,953 (2,972 before the character list's weapon encoder moved to `SlayerListWeapon.h`, 3,031 before the equip-requirement move to de-core, 3,043 before the compiler-warning cleanup, 3,068 before the item-load hoist, 3,086 before the initial-rank hoist; was 4,046 before the 4.3 hoists, 3,516 before the commented-out code went; enforced by `ratchets.sh` R6h) |
| `src/server/gameserver/Vampire.cpp` | 1,919 (1,954 before the equip-requirement move to de-core, 1,958 before the compiler-warning cleanup, 1,986 before the item-load hoist, 2,002 before the silver-damage hoist, 2,022 before the initial-rank hoist, 2,047 before the exps hoist; was 2,783 before the 4.3 hoists, 2,235 before the commented-out code went; enforced by `ratchets.sh` R6i) |
| `src/server/gameserver/Ousters.cpp` | 1,848 (1,879 before the equip-requirement move to de-core, 1,880 before the compiler-warning cleanup, 1,900 before the item-load hoist, 1,915 before the silver-damage hoist, 1,934 before the initial-rank hoist, 1,954 before the exps hoist, 1,959 before an empty sight override left by the commented-out code went; was 2,548 before the 4.3 hoists, 2,117 before the commented-out code went; enforced by `ratchets.sh` R6j) |
| `src/server/gameserver/skill/SkillFormula.cpp` | 818 (was 3,081 before the 3.3 computeOutput extraction — now thin adapters + the 11 dice-roll formulas; enforced by `ratchets.sh` R6d) |
| `src/server/gameserver/skill/HitRoll.cpp` | 642 (not a god file — an extraction-target pin, locked in with its 3.3 extraction; enforced by `ratchets.sh` R6c) |

Once Phase 1's test harness exists, encode R1–R8 as **ratchet tests**: the
checked-in expected count lives next to the test, the test fails when the
measured count *exceeds* it, and lowering it is part of the shrinking commit
(sidecar's `ConventionScanTest` shrink-only pattern).

---

## Phase 1 — Pin the wire contract

The packet classes are duplicated by hand in the client repo
(`client/Client/Packet/*`); the byte layout is the only contract, and at the
time this phase opened nothing checked it. This phase makes protocol drift visible and reviewable
before anything else moves. Everything later shelters under this pin.

- [x] **1.1 Test harness in CMake.** Add GoogleTest (FetchContent or vendored),
  a `tests/` tree, and a `make test` target that builds and runs it locally.
  Debug build, no MySQL/network required for the unit tier.
  > **Status:** done — GoogleTest via FetchContent (1.12.1 at the time;
  > v1.18.0 since the 5.4 C++20 migration), `tests/` +
  > `make test` (`-DDARKEDEN_BUILD_TESTS=ON`); the `TestPackets` library
  > compiles the full packet set with no server-type macro (7 files needed
  > the standard `#ifdef __GAME_SERVER__` guards they were missing).
  - Owner: the harness itself; `make test` documented in CLAUDE.md.

- [x] **1.2 Packet round-trip tests with golden byte fixtures.** For each
  packet direction (start GC/CG, then LC/CL, then inter-server), a test that
  constructs a canonical instance, `write()`s it, compares against a
  checked-in golden byte file, then `read()`s it back and compares fields.
  Must cover the `__USE_ENCRYPTER__` path for every `code % 3` shuffle branch
  of `SHUFFLE_STATEMENT_*` (`src/Core/EncryptUtility.h`) — the shuffle *is*
  part of the wire format.
  > **Status:** done — every registered packet factory has a checked-in
  > golden byte fixture, a loopback round trip comparing every getter,
  > and a pin of `getPacketSize()` against the bytes `write()` emits and
  > against the factory's `getPacketMaxSize()`. Coverage is proved
  > mechanically rather than claimed: the factory names in
  > `tests/ratchet/factory_registrations.txt`, 449 unique across the
  > three servers, minus the packet names under `tests/golden/`, is
  > empty, and no packet in it is pinned only through another packet's
  > variant golden.
  >
  > The conventions a new packet must follow: fixture values distinct
  > per field and >= 128 in every byte the width allows, with every
  > exception named at the point of use (enumerators, bools, text,
  > range-limited fields — the Debug toolchain traps an out-of-range
  > enum load); a golden per `write()` branch a single fixture cannot
  > reach; goldens recorded at encrypt code 0 for a packet that does not
  > touch the encrypter, with the test also asserting the bytes do not
  > vary with the code, so adopting the encrypter fails loudly instead
  > of silently voiding the pin; per-code goldens at 0..5 for one that
  > does, which `tests/ratchet/ratchets.sh` demands of any new encrypter
  > user (`tests/ratchet/encrypter_exceptions.txt` holds the abstract
  > base); a poisoned-storage constructor pin per file, splitting the
  > set into the packets that initialise every member they write and the
  > packets that do not; and every write/read disagreement stated as a
  > test that FAILS once the packet is fixed, never worked around in the
  > fixture. Goldens are re-recorded deliberately with
  > `UPDATE_GOLDENS=1 ./bin/wire_tests`
  > (`bash tools/devbuild.sh test --record`); a changed golden is a
  > protocol change the client repo must ship identically.
  >
  > The pins are split one file per family (`tests/packet_*.cpp`), each
  > with its own header listing the packets it covers, the exclusions and
  > the findings — that header is where a new packet's family is decided.
  > `tests/packet_encrypter_test.cpp` is the shuffle family: it drives
  > every encrypter user at codes 0..5, which reaches every `code % N`
  > case of every `SHUFFLE_STATEMENT_N`. The datagram half of the
  > inter-server family rides a real `Datagram`
  > (`tests/packet_interserver_test.cpp`, with the socket hop in
  > `tests/datagram_frame_test.cpp`). The four packets pinned first
  > (`GCMoveOK`, `CGMove`, `CGSay`, `CGWhisper`), the framed-bytes golden
  > and the header-width assertions stay in
  > `tests/packet_roundtrip_test.cpp`.
  >
  > Every family turned up write/read disagreements (all recorded in
  > `docs/FIXES.md`). They are fixed and pinned as the behaviour the
  > packets now produce; the one that cannot be fixed — `CGPortCheck`'s
  > registration, which the game server's UDP path needs — is recorded in
  > its file's header and in `docs/FIXES.md`.
  - Owner: the golden files — any layout change is a byte-diff in the commit.
  - Note: packets whose `read`/`write` depend on game-state globals need those
    globals stubbed; list any such packet here as found (they are also the
    first candidates for 2.3). Found so far: `GCStashList::setStashItem`
    (builds wire fields from live `Item*`; now guarded server-only), and the
    stream-level `EncryptData` "viva 2008" layer is dead code (returns
    immediately), so framing + per-field encrypter is the whole transform.

- [x] **1.3 Wire-layout inventory (`tests/wire-layout.txt`).** A test walks
  every packet factory, records packet ID, name and max body size,
  regenerates the file, and compares it to the committed copy (sidecar's
  `WireFieldInventoryTest` / `wire-fields.txt` pattern). A protocol change
  becomes a reviewable diff in the same commit, not a client crash later.
  Field-order pinning is per-packet via the golden fixtures (1.2) — C++
  offers no reflection to enumerate fields generically.
  > **Status:** done — 465 factories inventoried
  > (`tests/generated/AllPacketFactories.inc`, generated by
  > `tests/tools/gen_factory_list.sh`); the test also proves packet-ID
  > uniqueness and factory/packet ID agreement. Curious find: packet ID 0
  > (CGAddSMSAddress) can never pass `writePacket`'s `Assert(packetID != 0)`.
  - Owner: the inventory test + the ratchet script's freshness/subset checks.

- [x] **1.4 Share the inventory with the client repo.** Drop the same
  generator + committed inventory into `client` (its packet copies have
  already diverged textually — verified on `GCMoveOK.cpp`, 135 differing
  lines — while layouts are believed equal). First run produces the
  first-ever ground-truth diff of the two protocol copies; any layout
  mismatch found is a **bug to triage immediately**, recorded here.
  > **Status:** done — `tests/tools/wire_inventory_diff.sh [client-dir]`
  > exits 0. Both repos carry a wire-layout inventory and a generator: the
  > client's is `client/tests/tools/gen_wire_inventory.pl` plus
  > `client/tests/unit/test_wire_layout.cpp`, pinned there by a
  > `wire_inventory_fresh` ctest — it lifts each factory's `getPacketID()` /
  > `getPacketMaxSize()` body into a plain function because the client cannot
  > link its packet `.cpp`s. Packets deliberately present on one side only are
  > named with their reason in `tests/wire-layout-exceptions.txt`; anything
  > else one-sided is a finding to triage here, so never add a line to make
  > the diff green.
  >
  > What the first-ever cross-check found, and the rules it left behind:
  > - **Every id after a missing enumerator shifts.** The server had dropped
  >   `PACKET_GC_USE_SKILLCARD_OK` (485) and its packet together while the
  >   client kept both, so the whole Exchange block disagreed. The enumerator
  >   is back in `src/Core/Packet.h` and the packet is not, which is why
  >   `GCUseSkillCardOK` is in the exceptions file. Inserting an id is a
  >   protocol change both repos must ship; appending is cheap.
  > - **A packet with no factory is invisible to the inventory.**
  >   `GCExchangeList` / `GCExchangeBuy` had none and so were never compared.
  > - **`read()` must be `write()`'s exact mirror, and both must clamp the
  >   same way.** `writePacket` emits the size header *before* calling
  >   `write()`, so a `write()` that truncates a length byte while
  >   `getPacketSize()` counts the untruncated one desynchronises the stream
  >   permanently — and throwing from `write()` is no fix, the header is
  >   already out. Every string field now clamps identically in `write()`,
  >   `getPacketSize()` and `read()` against a named per-field cap, and
  >   `read()` resets the state it fills.
  > - **A string on the wire is a BYTE length then that many bytes.**
  >   `iStream.read(std::string&)` used to resolve to the generic
  >   `template read(T&)`, which assigns from a `std::string` fabricated over
  >   the wire buffer — an arbitrary read driven by attacker bytes. That is
  >   what `de::wire::readString`/`writeString` exist for (ratchet R9).
  > - **A count the peer sends must be bounded before it is looped on**, and
  >   a wire cap must match the column that stores the value: the
  >   idempotency key's cap is 64 because `PointLedger.IdempotencyKey` is
  >   `VARCHAR(64) UNIQUE` and this project's non-strict `sql_mode` truncates
  >   silently on insert.
  > - **Doubling a quote is not escaping.** The Exchange access class doubled
  >   the single quote and nothing else, so a backslash before a quote broke
  >   out of the literal — reachable by any logged-in player. Its
  >   `escapeSQL()` lives in `repository/MySQLExchangeRepository.cpp` now and
  >   goes through `mysql_real_escape_string`; new repository SQL uses the
  >   parameterized `executeQuery` form instead (3.2).
  > - **A `sprintf` buffer sized for one type is not sized for another.**
  >   `StringStream`'s `long`/`ulong`/`float`/`double` `operator<<` each
  >   formatted into a `char buf[12]` copy-pasted from the 32-bit `int`
  >   overload, so any value outside the 32-bit range — or any float over
  >   10,000, since `"%f"` never uses exponent form — smashed the stack on
  >   the LP64 target. Widened, and `snprintf`.
  >
  > The behaviour defects the cross-check turned up beside the layouts
  > (the seller filter, the buy idempotency key, the 2048-character
  > statement bound, the claim-list and packet-reader leaks, the success
  > message, the list maximum against the client's ring) are closed or
  > recorded in `docs/FIXES.md`, 2026-09-24. Still open, each deliberately
  > out of scope for a layout change:
  > - Neither repo clamps the listing count in `write()`:
  >   `(uint16_t)m_Listings.size()` narrows silently while the body loop
  >   iterates the full vector, so at 65536+ listings the count wraps to 0
  >   with the body still emitted and counted. Unreachable only because the
  >   handler clamps the page to `kMaxListingsPerPage`, i.e. the invariant
  >   lives a layer above the packet. Identical in both repos, so it is not
  >   a divergence.
  > - Read/write **field order** is not compared by the inventory. The
  >   per-packet goldens (1.2) are the only pin for that, and the client has
  >   none.
  - Owner: matching inventory files in both repos; a cross-check script that
    diffs them (runnable locally from the parent dir).

- [x] **1.5 Ratchet tests for R1–R5.** Encode the ratchet table as tests with
  checked-in expected counts, failing on increase.
  > **Status:** done — `tests/ratchet/ratchets.sh`, run by ctest; fails on
  > increase AND on unrecorded decrease (tighten the baseline in the same
  > commit). Also pins `AllPacketFactories.inc` freshness and checks every
  > `PacketFactoryManager` registration is inventoried
  > (`tests/ratchet/factory_exceptions.txt` holds the deliberate omissions;
  > it has no entries today — the CR/RC relics that were in it went with
  > `src/Core/Rpackets`).
  - Owner: the ratchet tests.

**Phase exit criteria:** `make test` green locally; golden fixtures for at
least all GC/CG packets; inventory committed in both repos with zero
unexplained layout diffs (or every diff triaged and logged here).
**Met.** The suite is green; every one of the 449 factories a server actually
registers has a golden, which `ratchets.sh` proves by subtraction, and the
16 further factories the inventory carries are ones no
`PacketFactoryManager` list names (the deleted phone exchange, the
server-only guild pair, `GCExchangeBuy`, `GCExchangeList`, `GCModifyMoney`,
`GCShowGuildRegist`, `GCShowUnionInfo`, `GCSubInventoryInfo`; fourteen of
them have no golden, the two Exchange replies do); `wire_inventory_diff.sh` exits 0
with every remaining one-sided packet named in
`tests/wire-layout-exceptions.txt`.

### Defects found by adversarial review of this phase (2026-08-29)

Two independent reviewers attacked the suite's own claims. Fixed in the
same branch; recorded here because each is a trap worth not re-entering:

- **CRLF made the whole suite fail on any fresh checkout.** `core.autocrlf`
  is `true` on the dev machine and `.gitattributes` covered only `*.sh` /
  `Dockerfile*` / `*.conf`. A clone produced `0b1603\r\n` goldens, and the
  tests run in a Linux container where `ifstream` does no translation — all
  16 goldens, the inventory and the ratchet freshness check would fail,
  reported as "protocol-breaking change". Fixed with `eol=lf` on
  `tests/golden/*.hex`, `tests/wire-layout.txt`, `tests/generated/*.inc`,
  `tests/ratchet/*.txt`. **Any new committed test-data type needs the same
  treatment.**
- **The frame header had no byte pin.** `writeBody()` calls `packet.write()`,
  never `writePacket()`, so widening `PacketID_t`/`PacketSize_t`/
  `SequenceSize_t` left every golden and the inventory unchanged while
  desynchronising all 463 packets at byte one — the header tests read back
  through the same typedefs and agreed with themselves. Fixed with a framed
  golden plus explicit width assertions.
- **`UPDATE_GOLDENS` accepted any value**, so `UPDATE_GOLDENS=0` silently
  re-recorded every pin and asserted nothing. Now requires exactly `1`.
- **8 of 16 goldens were identical duplicates**: CGSay/CGWhisper never
  reference the encrypter, so their four per-code files advertised coverage
  that did not exist. Reduced to one each, with a test that fails if either
  packet ever starts encrypting.
- **`pump()` could hang forever** on an over-reporting `getPacketSize()`
  (a known drift mode — `SocketOutputStream` logs it to
  `packetsizeerror.txt`). The accepted socket is now non-blocking with a
  bounded idle-poll budget, and `wire_tests` has a ctest `TIMEOUT`.
- **`make test` poisoned the production build cache.** `DARKEDEN_BUILD_TESTS`
  is a cached BOOL and `make debug`/`release` reused the same `build/` dir
  without resetting it, so a later production build pulled in googletest and
  the then-948-file `TestPackets`. Fixed with explicit `=OFF` in those recipes
  plus `EXCLUDE_FROM_ALL` on both test targets.
- **`ratchets.sh` overwrote a tracked file with no trap** — an interrupt
  between generate and restore left `AllPacketFactories.inc` clobbered. It
  now generates into a scratch tree.
- Docs corrected: the re-record command pointed at `./build/tests/wire_tests`
  (binaries land in `bin/`), and "the same shape the client builds" was
  wrong — the client compiles with `__GAME_CLIENT__=1`, so `#ifndef
  __GAME_CLIENT__` blocks resolve the opposite way in `TestPackets`.

---

## Phase 2 — Module split: `de-kernel` ← `de-core` ← apps

Sidecar's kernel/core/app with the framework-free lower layers. The CMake
target graph is the first enforcement (a kernel target cannot see core/app
headers); an include-graph test is the ArchUnit analog for what target
visibility can't express.

- [x] **2.1 Define the target layering in CMake.**
  - `de-kernel`: packet data classes, socket streams, `Types.h`, encrypt
    utilities, `Packet`/`PacketFactory` base. **No** MySQL, Lua, Zone/Creature
    headers, or server-type `#ifdef`s.
  - `de-core`: game domains (skill, item, quest, war, mission, exchange, …) —
    game rules against kernel types and repository *interfaces* only.
  - Apps: `gameserver`, `loginserver`, `sharedserver` executables — DB
    adapters, network transport, handler wiring, composition root.
  Initially the split is aspirational for existing files; new code must land
  in the right target from day one.
  > **Status:** done (2026-09-01) — the three layers exist as real,
  > linked targets: `de-kernel` (below) is the one wire archive every
  > server links; `de-core` (`src/domain/`, created by 3.3) is so far
  > linked only by the gameserver and holds only the extracted formulas —
  > the domain dirs named above are still gameserver app libraries, and
  > moving them is 3.x extraction work, not part of this task's "define
  > the layering". The file sort for legacy code continues under Phase 3
  > ratchets; layering is enforced by the 2.2 test + the kernel target's
  > pinned include path.
  > `de-kernel` is a STATIC library whose membership is
  > `tests/arch/kernel_files.txt` (grown from a 57-file seed past a
  > thousand) and whose **only** include dir is `src/Core`, pinned as the
  > target's own `INCLUDE_DIRECTORIES` because the top-level directory
  > include path would otherwise leak `src/server` and MySQL in — that pin
  > is what makes a kernel source reaching for an app header fail to
  > compile, so do not relax it. `de-core` is pinned the same way — its own
  > `INCLUDE_DIRECTORIES` is `src` alone — and links no other target at all
  > (`src/domain/CMakeLists.txt`), which is what "freestanding" means here.
  - Owner: CMake `PRIVATE` include dirs on each target; membership file
    shared with the 2.2 test.

- [x] **2.2 Include-graph architecture test.** A Python script under `tests/`
  (run by `make test`) that parses `#include` edges and fails on forbidden
  ones: kernel → core/app; core → `mysql.h`, socket headers, Lua headers;
  any core/app → kernel-internal detail headers as they get marked. Keep the
  rule list in one file (sidecar's `ArchitectureRules` pattern: extend the
  list deliberately, never weaken a rule to fix a compile error — a violation
  means the class is in the wrong module).
  > **Status:** done — `tests/arch/check_includes.pl` (perl, not python:
  > the dev image has no python3 and the repo's generators are already
  > perl), run by ctest as `arch_includes` (2026-08-31). Five rules, each
  > stated in the script's own header: **K1** a kernel file quote-includes
  > only kernel files (transitive by construction); **K2** no server-type
  > macro and no `__COMBAT__` in a kernel file; **K3** the packet libraries
  > in `src/Core/CMakeLists.txt` may define no macro outside the K2 banned
  > set, so "one meaning" holds at the definition site rather than by
  > coincidence of today's `-D` set; **C1** the gameserver domain dirs
  > (skill/item/quest/war/mission/couple/ctf/mofus/exchange) must not
  > include MySQL, Lua or socket-transport headers; **D1** a `src/domain/`
  > file quote-includes only existing domain headers, spelled
  > `"domain/X.h"`, and angle-includes only `<algorithm>` and `<cmath>`.
  > K and D rules have no baseline (the lists are defined as what
  > complies); C1's remaining
  > pre-existing violations are frozen shrink-only in
  > `tests/arch/baseline.txt` — two entries, both `mofus/` Socket.h users,
  > which are Player-transport classes and stay until the mofus module is
  > sorted.
  - Owner: the include-graph test.

- [x] **2.3 Strip `execute()` off packets; dispatch table at the composition
  root.** The crux. Today packet classes carry `execute()` → `*Handler` which
  reaches into gameserver internals, with `#ifdef __GAME_SERVER__` /
  `__GAME_CLIENT__` switching (vestige of the once-shared codebase). Replace
  with a per-executable dispatch table (packet ID → handler function)
  registered at each app's composition root; handlers move out of `Core` into
  the app/domain that owns them. Kills the macro switching and is what makes
  `de-kernel` actually framework-free. Migrate direction-by-direction under
  the Phase 1 pin (layout must not change — golden tests prove it).
  Track with ratchet R4 (packets still carrying `execute()`).
  > **Status:** done (2026-08-31) — `Packet` carries no `execute()` at all.
  > `PacketDispatcher` (kernel: packet id → handler function, written only at
  > startup so zone threads read it lock-free) is called unconditionally by
  > every receive loop, and `PacketDispatcher::dispatch` throws
  > `InvalidProtocolException` on an unregistered id — so a packet a server
  > does not register disconnects the sender rather than running a no-op
  > handler. That is the one intended behaviour change, and it is why an
  > ignore-thunk, not a missing registration, is how a packet the live client
  > sends but the server does not act on is spelled: the validator's
  > `GPS_NORMAL` set admits every packet a client sends (the GC-named ones
  > are listed in `src/Core/GameClientLink.h`), so a legitimate client would
  > otherwise be disconnected. The gameserver has two such thunks
  > (`GCAddStoreItem`, `GCRemoveStoreItem`; `GCCannotUse`'s went when the
  > client link stopped admitting it, since no client sends it), plus explicit
  > ones for `CGPortCheck`'s player-less handler and `CGStashList`'s
  > `__BEGIN_DEBUG` wrapper. Registration macros live in
  > `PacketDispatcher.h`. Composition roots: `GamePacketDispatch.cpp`,
  > `LoginPacketDispatch.cpp`, `SharedPacketDispatch.cpp`. R4 481 → 0, held
  > by the ratchet. Closed after a live smoke test of all three servers
  > against the real client (login, guild ops, friend chat).
  >
  > **Residual:** the handler *classes* are still declared in the packet
  > headers. 196 `src/Core/*.h` carry a `class XHandler { ... };`; no
  > `src/server/*/handler/` directory holds a header at all, so the 194
  > `handler/*.cpp` define their members against the Core declaration
  > (`CGConnectSetKeyHandler` twice, in the gameserver and the loginserver),
  > and three — `CGDialUpHandler`, `CGPhoneDisconnectHandler`,
  > `CGPhoneSayHandler` — have no implementation left. The client repo
  > mirrors 167 of the 196 headers and 42 of its copies carry a handler class
  > of their own, so moving the definitions out is a two-repo change.
  - Owner: R4 ratchet test + include-graph test (a kernel packet including a
    Zone header fails).

- [x] **2.4 Move packet sources into the kernel target.** Once a direction's
  handlers are out (2.3), move those packet files under the `de-kernel`
  target. `Core`'s non-packet utilities get sorted kernel-vs-app as touched.
  > **Status:** done (2026-09-01) — every kernel `.cpp` is compiled exactly
  > once, in `de-kernel`, and all three apps plus the tests link that one
  > archive. `Core` is down to the three files the kernel cannot own —
  > `SXml` (tinyxml2 binding), `TimeChecker` (server `Timeval`) and
  > `DatagramFactoryRead.cpp`, which holds the two factory-calling datagram
  > receive paths split out of `Datagram.cpp`/`SerialDatagram.cpp` so the
  > rest of the framing could be kernel — and links `de-kernel` PUBLIC, so
  > consumers are unchanged. The per-server packet libraries are the three
  > per-server `#if` files (`PacketFactoryManager`/`PacketIDSet`/
  > `PacketValidator`); `PlayerStatus.h` stays app-side for the same reason
  > (the billing SSO header held out with it is gone with the billing
  > module). There are no hand-kept per-direction source lists any
  > more: membership is `tests/arch/kernel_files.txt` alone, which
  > `src/Core/CMakeLists.txt` and `gen_factory_list.sh` both read.
  >
  > The rules the flip left behind:
  > - A file joins the kernel only if it passes K1/K2 — membership was
  >   computed by fixpoint against the checker, not by hand. A packet that
  >   needs a game object keeps its member *definitions* in
  >   `src/server/gameserver/packetfill/` with the game types forward-declared
  >   in the header, which is how the 23 game-coupled GC packets, `CLSelectPC`
  >   and `PetInfo` got in. `PetInfo::write()` resolves the pet-item ObjectID
  >   through a type-erased thunk the app-side setter installs, so it is still
  >   a live read at write time — an earlier cached-id version shipped stale
  >   ids.
  > - K2 bans `__COMBAT__` as well as the server-type macros, because a
  >   macro-conditional in a kernel file now silently compiles as "off" for
  >   everyone.
  > - K3 and the K1 ban on parent-relative includes came out of the review of
  >   this flip: a `../` include resolves from the including file's directory
  >   and would bypass both the checker's basename match and the pinned
  >   include path.
  > - **One tripwire was lost knowingly.** The old thin per-server packet
  >   archives made a cross-server factory registration a link error; now
  >   every executable links all packet objects, so over-registration in
  >   `PacketFactoryManager.cpp`'s `#if` blocks links clean. The per-server
  >   validator whitelists are the runtime gate, and
  >   `tests/ratchet/factory_registrations.txt` is the membership pin.
  > - Handler bodies live under each app's `handler/` (gameserver 164,
  >   loginserver 22, sharedserver 8 today); the 271 GC/LC handler files that
  >   2.3 proved the server never runs were deleted, the client repo keeping
  >   its own copies.
  > - Packets with no id enumerator are deleted rather than kept: that is how
  >   `CGAddInjuriousCreature`, `GCMonsterKillQuestStatus` and `CLAgreement`
  >   went. The `Rpackets`/`Upackets`/`TOpackets` relic directories are gone
  >   with them, which is why `tests/ratchet/factory_exceptions.txt` is empty.
  > - The dead phone exchange (`CGDialUp`, `CGPhoneDisconnect`, `CGPhoneSay`)
  >   has no factory in any `PacketFactoryManager::init()` list, so its ids
  >   answer with `InvalidProtocolException`; its handlers and registrations
  >   are gone, and with them the only senders of `GCRing`,
  >   `GCPhoneConnected`, `GCPhoneConnectionFailed`, `GCPhoneDisconnected`
  >   and `GCPhoneSay`. The packet classes stay in `src/Core`, which the
  >   client repo mirrors.
  - Owner: CMake target membership + include-graph test.

- [x] **2.5 Link server implementations without their executable entry points.**
  > **Status:** done (2026-10-02) —
  > `GameServerRuntime`, `LoginServerRuntime` and `SharedServerRuntime` are
  > OBJECT libraries containing each server's implementation except
  > `main.cpp`. The executables compile only that entry point and link their
  > runtime. Server definitions, include paths and dependencies are PUBLIC
  > usage requirements, so tests use the same objects and class definitions
  > as production. OBJECT libraries keep all server objects ahead of the
  > subsystem archives, preserving the gameserver's existing back-references
  > from those archives.
  > Each runtime belongs in a separate test process: the servers have
  > different classes with identical names and a process-wide dispatch table.
  > The three `*_server_runtime_tests` register the real handlers and exercise
  > game compatibility packets, login logout, shared guild-info replies and
  > rejection of packets meant for another server. They need no listeners,
  > database or server startup. Their own `main` is compiled directly, so
  > accidentally moving a server's `main` into its runtime fails the link.
  > These tests add the full runtime build to `wire_tests`; the existing
  > isolated rule tests retain their smaller dependencies.
  - Owner: the three runtime test targets, built by `make dev-test`, and
    the production executable links.

- [x] **2.6 Extract startup configuration from the entry points.**
  > **Status:** done (2026-10-02) —
  > `ServerStartup` is a library over `de-kernel` alone. Its
  > `parseServerOptions` accepts `-f <file>` for all three servers and
  > `-i <signed decimal offset>` for loginserver; `loadServerConfiguration`
  > returns an owned `Properties` only after loading and applying the optional
  > login overrides. Neither function publishes anything to `KernelContext`.
  > Each `main()` owns and registers the completed configuration, and returns
  > failure on an argument, file, parse or override error before constructing
  > its server. Lifecycle control and process shutdown setup are shared by
  > 2.7 and 2.8 below; the entry points keep their fatal-error handlers.
  > `applyLoginServerOffset` validates the three decimal bases and checks all
  > sums before writing the port, UDP port and ID. Signed offsets, including
  > zero, retain their meaning; malformed numbers, overflow and trailing
  > command-line arguments are rejected (`docs/FIXES.md`). The underlying
  > `Properties` file grammar is unchanged.
  - Owner: `server_startup_tests`, linked without any runtime, MySQL or Lua;
    `server_startup_cli`, which checks the real executables' failure status
    and diagnostics for invalid startup inputs in a temporary directory.

- [x] **2.7 Extract lifecycle control from the entry points.**
  > **Status:** done (2026-10-02) —
  > Each entry point calls `runServerLifecycle` with construction/initialization,
  > start and stop actions. The shared `ServerLifecycle` library links only
  > `de-kernel`, so its production exception handling and cleanup sequence run
  > in tests without a server runtime, listeners or database.
  > Initialization still runs when shutdown was already requested; start is
  > skipped if the request exists after initialization. Normal return and
  > failed startup both request shutdown before attempting stop once, while
  > configuration and managers remain alive. Construction failure leaves
  > the entry point's server pointer null, making its stop action a no-op.
  > Startup `Throwable` diagnostics still reach stdout and `../log/instant.log`
  > before cleanup; other startup exceptions keep the existing unknown-error
  > diagnostic. Stop failures reach stderr and mark the result undrained.
  > A worker failure remains a failed exit even when stop returns normally.
  > The function returns its drain result and exit status. Signal/deadline
  > setup is shared by 2.8 below; fatal-error handlers, resource limits and
  > `_Exit` remain in `main()`. The server graph is not destroyed by the
  > shared runner.
  - Owner: `server_lifecycle_tests`, covering lifecycle order, shutdown
    requests, partial startup, exceptions at each stage, diagnostics and
    preservation of worker failure status; all three executable links and
    the existing `server_startup_cli` checks exercise the entry points.

- [x] **2.8 Extract process shutdown setup from the entry points.**
  > **Status:** done (2026-10-02) —
  > All three entry points construct `ServerProcessShutdown` before
  > initialization and return failure if `ready()` reports failed signal
  > installation. The library depends only on `Threads::Threads`. Its
  > SIGTERM/SIGINT handlers only set the existing lock-free shutdown request,
  > and its watcher keeps the 30-second default deadline after that request.
  > A shorter timeout can be supplied by tests. A guard never resets a
  > pending request or worker failure.
  > Leaving scope cancels and joins the watcher before restoring the previous
  > signal handlers, including their flags and masks. Partial signal
  > installation is rolled back; construction unwinding also restores
  > handlers if the watcher could not be started. The normal server path
  > still ends with `_Exit` after cleanup, preserving the process graph's
  > lifetime. Fatal-error handlers and core-dump limits remain in `main()`.
  - Owner: `server_process_shutdown_tests`, delivering SIGTERM/SIGINT on
    main and worker threads, checking lifecycle startup/stop integration,
    handler restoration and flag preservation, and using subprocesses to
    verify cancellation and forced failure for blocked initialization or
    cleanup. Existing executable CLI checks also cover early returns.

**Phase exit criteria:** `de-kernel` builds standalone with no MySQL/Lua/Zone
includes (include-graph test green); at least GC/CG fully migrated off
`execute()`; all three servers boot and pass a manual smoke test against the
live client. **Met**, and exceeded: `arch_includes` is green with no K-rule
baseline, R4 is 0 for every direction rather than GC/CG alone, and the smoke
test against the real client closed 2.3.

---

## Phase 3 — Testable domain logic: Outcomes, repositories, ownership rules

Runs as ongoing background work; every extraction is independently mergeable
and sheltered by Phase 1 tests. Ratchets R2/R3/R5 make progress monotonic.

- [x] **3.1 `Outcome<Events, Rejection>` result type.** C++20 template in
  `de-kernel` mirroring sidecar's `kernel.domain.Outcome`: gameplay mutations
  return `Ok(events)` or `Rejected(reason)`; exceptions reserved for
  programming/config errors. New/refactored domain code uses it; the
  `__BEGIN_TRY/__END_CATCH` macros stop being control flow (ratchet R5).
  > **Status:** done (2026-09-09). `src/Core/Outcome.h` is a `[[nodiscard]]`
  > `std::variant`-backed kernel type with unit tests. The shape every
  > adopter settled on: a `decide*` function takes a repository or topology
  > interface plus plain values, answers the events to perform — rows to
  > write, packets with their recipients, record mutations, in order — or
  > a typed rejection the handler maps to a wire code or to silence; the
  > handler gathers the facts, calls the decision and performs the events,
  > so every packet and every write keeps the old order and bytes. Each
  > adopter has a fake in `tests/support/` and a self-contained block at the
  > end of `tests/CMakeLists.txt`. Production callers, by process:
  > loginserver — every `CL*` decision: `CharacterCreation`,
  > `CharacterSelection`, `LoginDecision`, `CharacterDeletion`,
  > `ReconnectDecision`, `Registration`, `WorldSelection` (all under
  > `src/server/loginserver/`); no `CL*` handler decides by throwing.
  > gameserver — `exchange/ExchangeDecision` + `ExchangeService`,
  > `guild/GuildJoinDecision`, `party/PartyInviteDecision`,
  > `trade/TradePrepareDecision`, `trade/TradeTableDecision` (all under
  > `src/server/gameserver/`). sharedserver — `GuildDecision` +
  > `GuildStepRunner` for the five guild-mutation handlers. Assessed and
  > deliberately not adopted, with the reason at the site: `TradeManager::canTrade`
  > / `processTrade` and `CGTradeFinish` (a `goto` and an early return
  > interleave with the inventory simulation, so any split changes which
  > error code some inputs get), `CGPartyLeave` (its rules live in
  > `GlobalPartyManager`; the handler has nothing to decide),
  > `GSModifyGuildIntro` / `GSGuildMemberLogOn` (a lookup and one write).
  > Further decisions adopt the same shape as they are touched; the type
  > and the convention are complete.
  - Owner: R5 ratchet + convention grep test (no new `__BEGIN_TRY` in
    de-core sources).

- [x] **3.2 Repository extraction.** Pull inline `executeQuery` SQL out of
  game logic into `*Repository` classes behind interfaces (game logic takes
  the interface; the app wires the MySQL implementation). Sidecar convention
  adopted verbatim: **legacy schema quirks live in the repository on
  purpose** — zero-date columns, denormalized tables, encoding oddities are
  quarantined and documented *there*, never leaked into domain types. Order
  of attack: `PlayerCreature`/`Slayer`/`Vampire`/`Ousters` persistence first
  (biggest testability win), then Zone, then the long tail. Ratchets R2/R3.
  > **Status:** done (2026-09-22) — R2 104→0 and R3 317→0. No `executeQuery`
  > is left anywhere in `src` outside `src/server/database/` and the
  > `repository/` directories, so the two ratchets now hold the seam shut
  > rather than track it shrinking: a statement written back into game logic
  > fails `tests/ratchet/ratchets.sh`. 44 seams — 36 under
  > `src/server/gameserver/repository/`, two in ServerCore
  > (`src/server/repository/`, compiled into all three binaries), four in the
  > loginserver and two in the sharedserver — each an interface
  > `*Repository.h` with a `MySQL*Repository.cpp` implementation, reached
  > through `default*Repository()` accessors, never `g_p*` externs.
  >
  > **Extending a seam: the header is the authority.** Each repository
  > header carries its tables' quirks and, where SQL on the same tables
  > lives outside the seam, an explicit **"not enclosed"** list of it (nine
  > headers carry one today; the others say so in prose or not at all, so
  > grep before assuming a seam is total). Read that header before adding
  > a method, and grep the whole tree (loginserver/ and sharedserver/
  > included) before rewriting the list. A `SELECT MAX()` on an empty table
  > answers one NULL row; a method raises the intended `Error` for it rather
  > than converting it.
  >
  > **Seams:** `BalanceInfo`, `GameInfo`, `ContentInfo`, `ZoneInfo`,
  > `QuestInfo` — the read-only boot-time catalogues (exp/attr ladders,
  > skill/monster/NPC/script/option info, zone config incl. ZoneEffectInfo,
  > quest catalogues); `Character` (race-table loads, saves, tinysave),
  > `Gold`, `Stash`, `SkillSave`, `RankBonus`, `BloodBibleSign` (read-only),
  > `FlagSet`, `EffectSave` (the persisted effects), `SMSAddress`,
  > `QuestItem`, `PlayRecord`, `Goods`, `Nickname` — per-character state;
  > `ItemObject` (every `*Object` table's create/save/load/destroy behind a
  > per-table spec row with object-shape and info-shape enums — a loader
  > refuses a table of the wrong shape) and `Item` (trace logs, counters,
  > UniqueItemInfo, TimeLimitItems, the ItemID registry probes); `Guild`,
  > `Couple`, `Friend`, `Message`, `Session` (login/logout bookkeeping on the
  > account and GuildMember rows), `WarInfo` (shrines, castles, sweepers,
  > schedules, histories, reinforcement, race-war limits), `FlagWar`,
  > `RegenZone`, `BulletinBoard`, `ComebackEvent`, `MofusPoint`,
  > `SystemAvailability`, `SpecialEvent` (the one seam on the world-default
  > connection — `getConnection(int)`, see its header), `SMSMessage` (the SMS
  > relay's three statements, on the `SMS_DB_*` connection it owns itself
  > because the relay is not one of DatabaseManager's servers),
  > `CharacterPurge` (deletePC's 109-statement character-deletion list, one
  > method, one Statement, in the original order — every table on it is
  > another seam's, and those headers say so); `Exchange` (the Exchange
  > feature's own access class relocated under the convention:
  > ExchangeListing, ExchangeOrder, the AccountPoint / PointLedger
  > statements, which name the account schema on the game connection and
  > open at startup behind a reachability check, and the transaction
  > pair — the header says what each does against the shipped schema).
  > In ServerCore, under `src/server/repository/`:
  > `PayPlay` (PaySystem's Player pay-play columns and the PC-room tables,
  > on the dist connection) and `ServerInfo` (GameServerInfo with its
  > NonPKServerList and CastleStatInfo flags, and WorldInfo). In the
  > loginserver, under `src/server/loginserver/repository/` with a `Login`
  > prefix (the integration binary links every impl, so names must not
  > collide with the gameserver's): `LoginCharacterPurge`
  > (CLDeletePCHandler's ownership check, Slayer retirement, DeleteChar
  > record and 110-statement purge on the per-world connection, plus
  > ItemDestroyer's uncalled 41-table sweep); `LoginAccount` (the Player
  > row through a session — the three login projections, the LogOn /
  > LoginIP / server-id writes that answer whether a row changed, the
  > current world, group and slot, the stored password hash,
  > registration — with TestClientUser, WebLogin, IPBlockInfo,
  > Event200501Main, PrivateAgreementRemain, PCRoomUserInfo and the
  > USERINFO.LoginPlayerData record); `LoginCharacter` (the race tables
  > as the character list, creation with its balance-table probes,
  > selection, the FlagSet preset, all on the per-world connection);
  > `LoginConfig` (GameServerGroupInfo in both projections, ZoneInfo,
  > ZoneGroupInfo, ClientVersion). In the sharedserver, under
  > `src/server/sharedserver/repository/` with a `Shared` prefix:
  > `SharedGuild` (its own Guild/GuildManager persistence — GuildInfo,
  > GuildMember, the guild's GuildUnionMember rows and the
  > WarScheduleInfo cancel — plus the GS handlers' race-table GuildID
  > and Gold writes and their Messages inserts, the two whitespace
  > spellings behind an enum) and `SharedConfig` (the boot-time reads
  > of GameServerGroupInfo, GameServerInfo, ZoneInfo's resurrection
  > columns and SSStringPool).
  >
  > **Conventions a new method follows** (each one cost a review finding):
  > - Statements move **byte-for-byte**, quirks included (backticked
  >   `Rank`, mixed-case keywords, copy-paste whitespace). Moving SQL is
  >   not fixing SQL. A bug found in a statement is recorded in the
  >   header, pinned by a test, and gets its own PR.
  > - Repository SQL uses the parameterized `executeQuery` form, never
  >   string concatenation; StringStream chains become format strings
  >   with the same bytes. `executeQueryString` survives only where the
  >   statement takes no arguments, must be logged verbatim, or can
  >   outrun the 2,048-byte format buffer in `Statement::executeQuery` —
  >   the one case is `PlayRecordRepository::logPlayerTrade`, whose
  >   TradeLog row is as long as the two traded inventories make it, and
  >   the MySQL tier pins it past that width.
  > - Every `executeQuery` conversion is the argument's own width: `%u`
  >   for DWORD/WORD/BYTE, `%d` for int, `%ld` for `time_t`. A 32-bit
  >   argument read through `%ld` takes whatever the upper half of the
  >   register holds under the pinned Clang toolchain — a Fame of 777
  >   landing as 4294967295, an `ItemID=%ld` UPDATE matching no row.
  >   `Statement::executeQuery` carries
  >   `__attribute__((format(printf, 2, 3)))` and the build compiles with
  >   `-Wformat` (top-level `CMakeLists.txt`), so the compiler now says
  >   so — except for a format reached through a pointer (the per-table
  >   spec rows in the ItemObject, EffectSave and ComebackEvent seams),
  >   which only the tier covers.
  > - Rows are typed to the driver getter the inline code called
  >   (`getInt`→int, `getBYTE`→BYTE, `getString`→std::string); the
  >   caller keeps its casts and narrowings. Loads return a vector read
  >   in full before the caller acts on the first row.
  > - Same statement, different bytes → a spelling enum over a spec
  >   table, one literal written once. Different projection → distinct
  >   methods (distinct arities make a mix-up a compile error).
  >   Different value only → one format string with a typed parameter.
  > - Column names chosen at runtime (by race, by sex) move into the
  >   seam with the statements; the game-side lookup table is deleted.
  > - The Statement is freed on every path, success or failure:
  >   `END_DB`, `END_DB_EX` and `MySQLSMSMessageRepository`'s
  >   `END_DB_RETHROW` carry a catch-all that deletes it and rethrows
  >   unchanged. A failed statement crosses the seam as a
  >   `DatabaseError` (`src/server/database/DatabaseError.h`) that owns
  >   its `DBError.log` line, and that line names the repository method,
  >   not the caller.
  > - **Integration tier over fakes**: `mysql_repository_tests`
  >   (`tests/integration/`, `make integration-test`, needs docker) runs
  >   the real impls against MySQL 5.7 loaded with `initdb/` and the
  >   production sql_mode. A quirk is replayed there before it is
  >   written down — the first rounds' fakes documented three behaviours
  >   the server refuted. Fakes are not the tier's substitute: of the
  >   twelve in `tests/support/`, six back the pilot-era seams in
  >   `repository_tests` and six stand in for the repositories the 3.1
  >   decision tests drive. A seam whose callers are compiled out still
  >   gets a test: the compiler will never check them.
  > - R2/R3 are **textual** greps, so a commented-out `executeQuery`
  >   counts. A commented-out block that names code the conversion
  >   deleted is rewritten to name the seam method (it is otherwise
  >   wrong); a self-contained one is deleted, with the dead function
  >   around it when nothing calls it.
  >
  > **Pre-existing bugs found, pinned by the tier, deliberately not
  > fixed** (each named in its header):
  > - `FriendList`/`FriendHistory` are not in `initdb/`; opening the
  >   friend list disconnects the client.
  > - The FlagWar roll-up's GROUP BY is refused under ONLY_FULL_GROUP_BY
  >   (1055), so the first flag war to end takes the process down. Dead
  >   by config: `ActiveFlagWar` is 0 in both shipped confs.
  > - `RaceWarLimiter::clearPCList` reads the Name column as the race
  >   and indexes a three-element array with it.
  > - `GoodsRepository::takeOne` on a Num=0 row raises
  >   ER_DATA_OUT_OF_RANGE and leaves the purchase stuck.
  > - `giveGoldMedal`'s `INSERT INTO GoldMedalCount` names a table
  >   `initdb/` does not create, so every gold-medal award fails as a
  >   SQL error (PlayRecordRepository::insertGoldMedal, pinned).
  > - LearningItem's UPDATE says `Storage=%s` for an int;
  >   VampirePortalItem's zone loader reads eleven getters over an
  >   eight-column SELECT; CodeSheet's zone SELECT names columns its
  >   table lacks.
  > - GuildUnionOffer's PK is OwnerGuildID alone, so an ESCAPE insert
  >   over a standing JOIN/QUIT row throws out of CGQuitUnionHandler.
  >   Fixed since: the penalty replaces the guild's row (`docs/FIXES.md`,
  >   "A forced union quit fails on the guild's own offer row").
  > - ActionShowGuildDialog gates guild creation on a hardcoded seven
  >   days ahead of the handler's QUIT_GUILD_PENALTY_TERM.
  >
  > `MySQLSMSMessageRepository` is the one seam the tier cannot pin:
  > `uds_msg` and `msg_queue` are not in `initdb/`, and its thread is
  > live but dormant (`GameServer::start()` does not start
  > `SMSServiceThread`).
  - Owner: R2/R3 ratchet tests; repository unit tests (fake/in-memory
    implementations for domain tests; MySQL-backed integration tier runs
    locally against the existing docker + `initdb/` schema).

- [x] **3.3 Pure formula functions with unit tests.** Extract
  `SkillFormula`/`SkillUtil` math and stat calculations (`InitAllStat.cpp`)
  into pure functions in `de-core`. These are the highest-value tests in the
  game — they encode balance — and the cheapest to write.
  > **Status:** done (2026-09-23) — `de-core` (`src/domain/`, a
  > freestanding STATIC target) owns the game's balance math: all of
  > `AbilityBalance.cpp` (HP/MP/to-hit/defense/protection/damage/
  > attack-speed/critical/steal per race), `SkillUtil.cpp`'s
  > `computeFinalDamage`, `getDistance`, `computeRankExp` and
  > `decreaseConsumeMP`, `HitRoll.cpp`'s per-race success ratios, 293 of
  > the 304 per-skill `computeOutput` bodies (`SkillOutputFormulas.cpp`,
  > whose mirror SkillInput/SkillOutput structs keep the field names so the
  > move stays a diffable one), and 32 `InitAllStat.cpp` bonus formulas.
  > Every body was transplanted verbatim, narrow-integer wrap-around
  > included, behind a thin adapter at its old entry point. No named
  > extraction target remains; new formulas join `src/domain/` as the code
  > around them is touched.
  > What stays out is a rule, not a backlog. A formula that rolls dice
  > keeps its body in the adapter so the roll stays out of de-core (the 11
  > `computeOutput` bodies calling `Random()`/`rand()`, and `HitRoll`'s
  > rolls); so does anything that merely applies a stored parameter
  > (percentValue applications of effect-carried values, rank bonuses
  > applied as points, flat constants such as `ToHitBonus += 5`), and so do
  > the live-state gates and member writes the adapters wrap the call in.
  > The adapter's field mapping is the one surface no suite can see, since
  > `formula_tests` links de-core and gtest alone; it is verified by
  > reading, and the adapters say so where it matters.
  - Owner: the formula test suite; R6 line ratchets on `SkillUtil.cpp` /
    `InitAllStat.cpp` / `HitRoll.cpp` / `SkillFormula.cpp`.

- [x] **3.4 Codify thread ownership.** Document (in CLAUDE.md) which state is
  owned by which thread: zone-group state mutated only on its
  `ZoneGroupThread`, cross-group communication via queues only. Add
  debug-build `assertOwnedByZoneThread()` checks on Zone/Creature mutation
  entry points (sidecar analog: "never block the registry mailbox" — the
  invariant is written down *and* asserted).
  > **Status:** done (2026-09-23) — the contract is CLAUDE.md's "Thread
  > ownership" section, which is where it is maintained: ownership is
  > mutex-guarded rather than thread-affine (a `ZoneGroupThread` holds its
  > group mutex for the whole tick; any other thread takes it explicitly),
  > and the section lists the threads, the mailbox and snapshot seams that
  > carry cross-thread work, and what the asserts do not cover.
  > `ZoneGroup::assertOwned()` guards the eight `Zone` mutation gateways
  > under `DE_OWNERSHIP_CHECKS` (Debug only — this repo never defines
  > `NDEBUG`, so gating on that would have been a no-op) and `abort()`s
  > rather than throwing, because an `AssertionError` is a `Throwable` and
  > the `catch (Throwable&)` blocks on these very paths would swallow it.
  > The queue the task asked for is `src/server/Mailbox.h`: a player's box,
  > drained by whichever manager owns the player, plus `ZoneGroup::post()`
  > for group-level work. Tables read by every thread and extended by one
  > are `de::Snapshot`s.
  > A `Guild` or `GuildMember` another thread may hold is retired, not
  > freed, and answers as gone once retired: `GuildMember::getRank()` reads
  > `GUILDMEMBER_RANK_LEAVE` and `Guild::getState()` `GUILD_STATE_BROKEN`.
  - Owner: the debug asserts; `critical_section_audit`, which fails on a
    hand-written `unlock()` inside a critical section.

- [x] **3.5 Globals → context (long tail).** No big-bang DI. Introduce a
  `GameContext` owning the managers; converted subsystems take it (or narrow
  interfaces) explicitly; the old `g_p*` externs become shims into it until
  their last caller is converted. Ratchet R1.
  > **Status:** done (2026-09-23) — five registries of non-owning pointers.
  > `src/server/gameserver/GameContext.h` holds the gameserver's sixty-seven
  > managers, registered by `GameServer`, `ObjectManager`, `ClientManager` and
  > `IncomingPlayerManager`; `src/server/loginserver/LoginContext.h` the
  > loginserver's seven; `src/server/sharedserver/SharedContext.h` the
  > sharedserver's three. The managers no single server owns have two
  > process-wide registries of their own, each binary filling one set:
  > `src/Core/KernelContext.h` carries the configuration, the packet factory
  > table and the packet validator — it is a kernel file so that a Core
  > reader can take the factory table from it, which rule K1 would never let a
  > header under `src/server/` offer — and `src/server/ServerContext.h`
  > carries the database connection table, the game-server table and the world
  > table.
  > Each manager is registered by the code that creates it and read back
  > through an accessor that asserts it is there, a null one being a
  > startup-order bug, not a condition to branch on; a test for a null manager
  > went with each global that had one.
  > Ownership is untouched: the same `new` and `SAFE_DELETE` sites, each
  > global becoming a member of its creator or a local in the `main()` that
  > made it, initialised and deleted in the mirrored position. A manager no
  > global names is a member of its creator, registered only if something
  > outside that file reads it.
  > A reader reads the accessor inline where it needs the manager once or
  > twice and binds one local reference where a function reaches it three or
  > more times — except inside a `de::postToPlayer` command, which captures
  > by value and runs later, so it reads the accessor in the command body
  > rather than closing over a reference bound outside it.
  > `ctf/` and `quest/` are the converted subsystems: `FlagManager`,
  > `ActionFactoryManager`, `Trigger` and `TriggerParser` take the context in
  > their constructors and every `Action` gets it from its factory; none of
  > those calls `de::gameContext()`.
  > `game_context_tests`, `login_context_tests`, `shared_context_tests`,
  > `kernel_context_tests` and `server_context_tests` pin every accessor
  > registered and unregistered, each building a context over stand-in
  > pointers with no server linked — which is what the
  > forward-declaration-only headers are for. R1: 325 → 0.
  - Owner: R1 ratchet test.

- [ ] **3.6 Shared rules library.** The rules the client and the server
  both compute are implemented once, in de-core, and the client builds a
  byte-identical, hash-manifested copy of a listed subset
  (`third_party/decore/` there, written by its `tools/decore/sync.pl`)
  and asserts the same parity vectors on every client toolchain. Six
  slices, each a server change followed by the client's: (1) the shop
  buy, sell and repair price and the maximum durability; (2) the per-class
  grade policy and durability class table; (3) the existing de-core
  functions the client calls (server: their parity vectors and the
  steal-ratio cast; client: the callers); (4) equip requirements; (5)
  the castle tax; (6) `SkillOutputFormulas` and the small rules (skill
  range, party share, darkness).
  > **Status:** in progress (the client halves of slices 4, 5, 6a and
  > 6b) —
  > slices 1 to 3 are in on both sides
  > (server PRs #276 to #280, client PRs #285 to #287).
  > Slice 1: `ItemPrice` and `ItemDurability`, with `PriceManager`,
  > `computeMaxDurability`, `ConcreteItem::getMaxDurability` and the skull
  > sale as adapters.
  > Slice 2: `ItemGrade` (`gradeOffsets`, `gradePolicyOf`,
  > `hasDurability`) holds the per-class grade and durability table, and
  > `ConcreteItem` reads it through `itemClassHasGrade`,
  > `itemClassHasDurability` and `itemClassGradeOffsets`
  > (`ConcreteItem.cpp`) instead of taking grade and durability policies
  > as template arguments, so the server has no second copy. Classes not
  > built on `ConcreteItem` are `None` with no durability, and their
  > maximum durability is 1, not `maxDurabilityBase`.
  > Slice 3: `src/domain/vectors/stats.tsv` pins the `Formulas.cpp`
  > functions the client calls (the three races' to-hit, defense,
  > protection and min/max damage, `vampireSkillConsumeMP`,
  > `vampireDexHPRegenBonus`) and the three steal ratios, whose cast to a
  > byte goes through `int` (`docs/FIXES.md`). None of these indexes a
  > table; the party tables in `SkillOutputFormulas.cpp` are read
  > through a clamp since slice 6a.
  > Slice 4's server half: `EquipRequirement` (`requiredStats`,
  > `meetsRequirement`, `genderAllows`) holds the three races' item
  > requirement, its option raises, caps (slayer 200/290 and 300/435,
  > vampire level 100/150, ousters level 150) and 16- and 8-bit wraps;
  > `Slayer`, `Vampire` and `Ousters::isRealWearing` call it through
  > `computeEquipRequirement` (`ItemUtil.cpp`), which reads the item and
  > option tables. The advancement-class checks, time-limited items, the
  > premium-zone and pay gate and couple rings stay in the races, in
  > their own order. `src/domain/vectors/equip.tsv` pins all three
  > functions; its header lays out the row kinds the client's
  > `decore_tests` must learn (an `EquipRace` column, the six
  > `EquipRequirement` fields, two option lists, the five `EquipStats`
  > fields, the requirement result as a comma-separated list). The client
  > must also add `domain/EquipRequirement.cpp` to its
  > `third_party/decore/CMakeLists.txt` source list.
  > Slice 5's server half: `applyCastleTax` (`ItemPrice`) is the castle
  > tax on a shop purchase, which `CGShopRequestBuyHandler` applies to the
  > price times the count, or the mysterious rack's price; the handler keeps
  > the tax it credits to the castle. A taxed total past the int range is
  > defined as x86-64's 2147483648 (`docs/FIXES.md`). Its rows are the
  > `applyCastleTax` rows of `src/domain/vectors/price.tsv`, so the
  > client's `decore_tests` must learn that function (total, ratio,
  > expected).
  > Slice 6a's server half: `SkillOutputFormulas` is in the vendored
  > subset. Its warnings under `de-core-strict` were cleared without a
  > change in behaviour: `(void)` for the unused parameters,
  > `[[fallthrough]]` on HeadShot's deliberate fall-throughs, and an
  > explicit truncating `(int)` around each double expression that was
  > converted implicitly. The party tables are read through
  > `partyEffectBoost` and `partyDurationBoost`, which clamp the size to
  > 0..6; the server passes only 0 or 1, so no party bonus is granted
  > (`docs/FIXES.md`), and a client caller must pass the same.
  > `src/domain/vectors/skill_output.tsv` pins `WillOfLife`, the two
  > lookups at and past both ends, and `Bless` read through them. The
  > client's `decore_tests` must learn its row kinds (a skill row: the ten
  > `SkillInput` fields, the gun by `GunClass` name, the six `SkillOutput`
  > fields as a comma-separated list; a lookup row: party size, percent),
  > and its `third_party/decore/CMakeLists.txt` must list
  > `domain/SkillOutputFormulas.cpp`.
  > Slice 6b's server half: `skillRange` (`SkillRange`, vendored) is a
  > slayer skill's range at a proficiency level, which `computeSkillRange`
  > (`skill/SkillUtil.cpp`) reads the skill table and slot for; its three
  > inputs and its result wrap at 256 as the server's 8-bit types did, and
  > it no longer depends on the build machine's fused multiply-add
  > (`docs/FIXES.md`). `src/domain/vectors/skill_range.tsv` pins it; the
  > client's `decore_tests` must learn the row (minRange, maxRange,
  > expLevel, expected), and its `third_party/decore/CMakeLists.txt` must
  > list `domain/SkillRange.cpp`. `partyExpPool` (`PartyExp`, server
  > only, not vendored) is the percentage a party's size adds to the
  > experience it shares (150 to 270 for 2 to 6 members), which the five
  > sharing functions of `Party.cpp` each carried as a switch; they keep
  > their own splits of the pool, three in float and two in int
  > (`docs/FIXES.md`). The skill experience and fame party bonuses
  > (`skill/SkillExperience.cpp`) are other tables and stay there.
  > `src/domain/vectors/server/party_exp.tsv` pins it; the client does not
  > compute it. `darkLightForViewer` (`DarkLight`, server only) is the
  > dark and light levels a player is sent for its zone, by the zone's
  > type (castle, PK), its race and its Lightness and Yellow Poison;
  > `makeGCUpdateInfo` (`PacketUtil.cpp`), `EffectFlare::unaffect` and
  > the weather broadcast (`WeatherManager.cpp`, whose vampire levels were
  > unclamped until then) call it. The other `GCChangeDarkLight` senders
  > decide less and are recorded in `docs/FIXES.md`. `src/domain/vectors/server/dark_light.tsv` pins
  > it; the client draws the levels it is sent and computes none.
  > The vendored subset is `DECORE_VENDORED_SOURCES`
  > (`src/domain/CMakeLists.txt`), the domain headers those include, and
  > `src/domain/vectors/*.tsv`. A de-core source only the server calls
  > (`PartyExp`, `DarkLight`) stays out of that list and keeps its rows in
  > `src/domain/vectors/server/`,
  > which neither the client's sync nor `decore_client_diff.sh` reads, so
  > the client never sees a row it cannot evaluate; `formula_tests` asserts
  > them like the others. Every file under `src/domain` is checked out
  > LF (`.gitattributes`), as the client's copy is. Code in it
  > quote-includes only existing
  > `"domain/X.h"` headers and angle-includes only `<algorithm>` and
  > `<cmath>` (D1); takes
  > `int`, `unsigned`, `long long`, `double`, `bool`, `enum class` and POD
  > structs of its own, a variable-length input as `(const int*, int)`;
  > is free functions in `namespace decore` with no globals, RNG, I/O or
  > game-table lookups (a fixed switch over wire ids, as in
  > `gradePolicyOf`, is code) and no arithmetic in a header, so the
  > target's fp flags (`-ffp-contract=off -fno-fast-math`) reach every
  > formula; names each
  > wire item-class id it branches on in `decore::itemclass`
  > (`domain/ItemClass.h`, all 90 ids), which the adapter
  > `static_assert`s (`ConcreteItem.cpp` asserts every id and the count); and
  > compiles without a warning in `de-core-strict`. `SkillOutputFormulas`
  > (namespace `decore::skillformula`) has three named exceptions, kept so
  > its bodies stay verbatim moves of the gameserver's: the constant,
  > read-only party tables `PartyEffectBoost` and `PartyDurationBoost` in
  > an anonymous namespace; the plain enums `SkillGrade`,
  > `SkillInput::TargetType` and `SkillInput::TargetRace`, which mirror the
  > game's integer values; and `SkillOutput`, whose constructor zeroes it
  > as the gameserver's does, so it is not POD. Moves are verbatim,
  > oddities included (the kept ones are in `docs/FIXES.md`); changing
  > one is a balance decision. A change
  > to a shared rule: edit `src/domain`, re-record with
  > `bash tools/devbuild.sh test --record` and review the `.tsv` diff as a
  > balance change, resync the client and run
  > `bash tests/tools/decore_client_diff.sh <client-root>` until it is
  > clean, then merge the server first and the client straight after. The
  > vectors are recorded on the dev container and must pass unchanged on
  > x86-64 (`cpp20.yml`) and every client toolchain; a mismatch is
  > investigated, never re-recorded.
  - Owner: `formula_tests`' `SharedVectors` tests over
    `src/domain/vectors/`; `concrete_item_tests`, which probes every class
    built on `ConcreteItem` through its own base against the table, and
    fails when a new such class is missing from its list;
    `de-core-strict` (`-Werror`, built with the suite); D1 in
    `arch_includes`; and, as the done-criterion for a change to
    `src/domain`, `tests/tools/decore_client_diff.sh <client-root>`.


**Phase exit criteria:** no hard gate — this phase *is* the ratchets trending
down. The 3.2 checkpoint is passed: R2 reached 0, 3.2 is closed, and R3 is
re-baselined at 0 with it. Both hold rather than shrink now — an
`executeQuery` anywhere outside `src/server/database/` and the `repository/`
directories fails `tests/ratchet/ratchets.sh`. R1 holds at zero beside them,
3.5 having retired the last `g_p*` extern; R5 is the phase's remaining
trend line.

---

## Phase 4 — God files behind routers

- [x] **4.1 GM-command router for `CGSayHandler.cpp`** (4,904 lines). A
  `CommandRouter` with one class/function per GM command and **gating
  declared at registration** — sidecar's `on(name, limiter, handler)` /
  `onGated(...)` insight: hand-kept permission maps drift silently, so the
  registration site is the single source of truth. A test enumerates
  registered commands and asserts each declares a permission level.
  > **Status:** done — `src/server/gameserver/gm/`. `CommandRouter` matches a
  > name as a prefix of the chat message and passes over a command whose
  > level the caller is below, leaving a later matching name its turn;
  > `CommandRegistration.cpp` is the one place a name, the number of
  > characters it is compared over and its permission are written down (34
  > operator rows, four broadcast prefixes; `*billing disconnect` went with
  > the billing module, so that name is now an unknown command rather than a
  > silent no-op). The command bodies are free functions in `de::gm`,
  > grouped by what they touch across
  > `ServerCommands` / `PlayerCommands` / `ZoneCommands` / `ItemCommands` /
  > `GuildCommands` / `ConsoleCommands.cpp`; `CGSayHandler::execute` keeps
  > the leading-`*` test and hands the message to the two tables.
  > `tests/gm_command_router_test.cpp` links the real registration over
  > stubbed bodies and pins every row — name, length, gate and order — so a
  > gate change has to be written twice. The levels are God / DM / Helper /
  > Everyone, ordered as `Competence` is. Three alias names left mojibake
  > by the code-page move are registered unreachable, each longer than the
  > length it is compared over. `*command` carries a second table of its
  > own: `SubcommandTable` matches the word after it whole rather than as a
  > prefix, `ConsoleCommandRegistration.cpp` is the one place its 61 rows and
  > their gates are written down, the bodies are one function per name in
  > `ConsoleCommands.cpp`, and `tests/gm_console_command_test.cpp` pins the
  > rows the same way. A third table, `RelayCommandRegistration.cpp`, names
  > the twelve commands another game server may relay - the only place
  > `opmodifyunioninfo` and `oprefreshguildunion` are named - and
  > `GGCommandHandler::execute` keeps the host/port logging and hands the
  > relayed message to it. A relayed message carries no player, so each
  > sub-command row declares whether its body is correct without one and
  > `SubcommandTable::dispatch` passes over the 40 that are not, the way it
  > passes over a gate the caller is below.
  - Owner: router registration + the enumeration tests; R6e and R6f
    ratchets.

- [x] **4.2 Split `Zone.cpp`** (9,263 lines) by concern: movement, broadcast,
  spawn/despawn, scan/visibility, persistence (→ 3.2 repository). Mechanical,
  many small commits, each verified by build + smoke.
  > **Status:** done — the zone is seven translation units. `ZoneBroadcast.cpp` holds
  > the three `broadcastPacket` overloads, `broadcastDarkLightPacket`,
  > `broadcastSayPacket`, `broadcastLevelWarBonusPacket`,
  > `broadcastSkillPacket`, `movePCBroadcast` and `moveCreatureBroadcast`;
  > `ZoneScan.cpp` holds `scan`, `scanPC`, `monsterScan`, the four
  > `update*Scan` refreshes and `getWatcherList`; `ZoneMove.cpp` holds
  > `pushPC`, `movePC`, `moveCreature`, `moveFastPC` and `moveFastMonster`
  > with the `g_FastMoveSearch` step tables; `ZoneLoad.cpp` holds `init`,
  > `load`, `reload`, `loadTriggeredPortal`, `initSpriteCount`,
  > `loadNPCs` and `loadEffect`; `ZoneSpawn.cpp` holds the gated gateways
  > (`addPC` both overloads, `replacePC`, `addCreature`, `deleteCreature`,
  > `addCreatureToTile`, `deleteCreatureFromTile`) with `deletePC`,
  > `deleteQueuePC`, `deleteObject`, `createMonsterAddPacket`, `deleteNPC`,
  > `deleteNPCs`, `killAllMonsters`, `killAllMonsters_UNLOCK` and
  > `killAllPCs`; `ZoneItem.cpp` holds the ground-item tables — `addItem`,
  > `deleteItem`, `getItem`, `addToItemList`, `deleteFromItemList`, the
  > delayed and transport variants, `addRelicItem`, `deleteRelicItem`,
  > `addVampirePortal` and `deleteMotorcycle` — with the master-lair decay
  > constants, which nothing
  > else uses. They are still `Zone::` members with unchanged bodies — only
  > the translation unit differs — and the file-scope helpers more than one
  > unit calls (`isPotentialEnemy`, `sendRelicEffect`, `strlwr`) are declared
  > in `ZoneInternal.h`.
  > `Zone.cpp` 9,350 → 1,265, pinned by `ratchets.sh` R6g: the
  > constructors, the tile/sector/level accessors, the effect managers,
  > `getCreature`, the NPC info registry, `heartbeat`, `toString`, the
  > safe-zone and dark-light resets, the war and pay tails and the load
  > value. That is under the 2,000-line phase exit criterion, so the
  > `Zone.cpp` half of it holds.
  - Owner: R6 ratchet per extracted file.

- [x] **4.3 Race-class cleanup.** `Slayer.cpp`/`Vampire.cpp`/`Ousters.cpp`
  share large duplicated blocks; factor shared behavior toward
  `PlayerCreature` or free functions as formulas from 3.3 make the
  differences explicit.
  > **Status:** done (2026-09-23) — every body the three races spelled
  > identically, or identically under a substitution a seam can carry, is
  > defined once on `PlayerCreature`. Fourteen members hold the
  > persistence, gold and inventory work: `tinysave`, `setGold`,
  > `setGoldEx`, `increaseGoldEx`, `decreaseGoldEx`, `checkGoldIntegrity`,
  > `checkStashGoldIntegrity`, `setResurrectZoneIDEx`, `saveAlignment`,
  > `getIP`, `getItemShapeColor`, `getExtraInfo`, `getInventoryInfo` and
  > `isPayPlayAvaiable`, with `m_Gold`/`getGold()` beside `m_StashGold`.
  > Four member templates over the race's slot map hold the skill-slot
  > table: `findSkillSlot`, `removeCastleSkillSlot`,
  > `removeAllCastleSkillSlots` and `saveSkillSlots`, which all three
  > races delegate to in one line. `saveExps` writes the experience tail,
  > `saveInitialRank` the rank, `saveSilverDamage` the silver damage
  > (whose `m_SilverDamage` and accessors moved up with it), and
  > `loadItem(bool)` runs the connect-time item load.
  >
  > Four seams carry what those bodies differed by.
  > `characterRaceOf(getRace())` (`PlayerRace.h`) names the race table a
  > persistence body writes to. `skill/RaceSkillSlot.h` holds what
  > `VampireSkillSlot` and `OustersSkillSlot` spelled identically — the
  > name, skill type, interval, casting time and run time with their
  > accessors, `getRemainTurn` and both `setRunTime` overloads — leaving
  > each race the `create`/`save` pair its own table needs, plus
  > `destroy` and the `ExpLevel` for Ousters. `CharacterExpsRecord` holds
  > the eight columns the vampire and ousters exps rows share, and one
  > repository `saveExps` picks the table through `characterRaceTable()`,
  > composing per race the one clause whose bytes differed (an ousters row
  > always takes `SilverDamage`, a vampire's only when it is non-zero, and
  > the vampire fragment carries no space after its comma). `getLevel()`
  > is the seam under `saveInitialRank`: a slayer's is its highest
  > skill-domain level, the other two races' their stored level.
  > `loadItem(bool)` leaves two hooks: `loadOwnedItems()`, because
  > `ItemLoaderManager::load` overloads on the concrete race, and
  > `giveNewbieItems()`, empty on the base.
  >
  > What stays per race, and why. `Slayer::setGoldEx` is an override
  > because it writes `Gold = %u` where the shared body writes `Gold=%u`.
  > Slayer's `SkillSlot` stays outside `RaceSkillSlot`: it carries exp,
  > exp level and an enable flag, its `getSkillType()` is const and its
  > `setRunTime(Turn_t, bool)` takes a second argument, so deriving would
  > hide three base members across the ~500 handlers that take a
  > `SkillSlot*`. Its exps tail keeps its own `SlayerExpsRecord` — nine
  > domain goals, the three advanced attributes and the bonus, none of
  > which the other two rows have — and it has no silver damage, so
  > `saveExps` takes the goal experience and the silver damage as
  > arguments. `giveNewbieItems` is a slayer's and an ousters' only: a
  > vampire is made by transforming a slayer and is given no starting set,
  > a slayer reads `FLAGSET_RECEIVE_NEWBIE_ITEM_AUTO` as "still owed" and
  > turns it off, an ousters reads the same bit as "already given" and
  > turns it on. `load()`, `save()` and the `send*SkillInfo` builders
  > differ in the columns and the packet element types each race carries.
  > The free-play measure the races once held lives in the loginserver's
  > `CharacterSelection`.
  >
  > Two residuals are design facts rather than backlog. **The wear code is
  > per race because `WearPart` is wire-visible.** Each race scopes its
  > own `WearPart` enum, whose members differ in name and value, and those
  > values are the slot ids the client sends; the enum also sizes and
  > indexes `m_pWearItem`, so the eleven members keyed on it stay
  > three copies. Ten of them — `checkItemTimeLimit`,
  > `updateEventItemTime`, `destroyGears`, `saveGears`, `getGearInfo`,
  > `isRealWearing(WearPart)`, `isRealWearingEx`, `sendRealWearingInfo`
  > and, but for the race's own `PCInfo` member, `registerObject` and
  > `registerInitObject` — are character-for-character identical in
  > Vampire and Ousters once the wear constants are substituted, and
  > `checkItemTimeLimit`, `updateEventItemTime`, `isRealWearingEx` and
  > `sendRealWearingInfo` are identical in all three. `isRealWearing(Item*)`
  > is the one that differs for real: each
  > race checks its own item classes and gates, in its own order, and
  > delegates the requirement itself to de-core's `EquipRequirement`
  > (`computeEquipRequirement` in `ItemUtil.cpp` gathers the inputs).
  > Reconciling the enums is a protocol change the client repo must ship
  > identically, not a refactor.
  > **The two `addSkill` overloads differ in behaviour.**
  > Vampire alone does not assert on `SKILL_HOWL`, Ousters seeds
  > `ExpLevel` 1 and Slayer `Exp` 1 with `ExpLevel` 0, each logs to its own
  > error file, and Slayer refuses to delete a duplicate that is already
  > the mapped slot.
  >
  > Line counts are pinned by `ratchets.sh` R6h/R6i/R6j, `__BEGIN_TRY`
  > sites by R5; a body that comes back to a race file raises one of them.
  - Owner: R6h/R6i/R6j ratchets; `player_race_tests`;
    `race_skill_slot_tests`; the `saveExps` cases in
    `tests/integration/mysql_repository_test.cpp`.

**Phase exit criteria:** every GM command behind the router with declared
gating; `Zone.cpp` under 2,000 lines. **Both met.** `CGSayHandler::execute`
holds no command body — it tests the leading `*` and hands the message to
`de::gm::broadcastCommands()` then `de::gm::operatorCommands()`, and every
row of those two tables, of `SubcommandTable` and of the relay table
declares its `Permission` at the registration site, pinned by
`gm_command_router_tests` and `gm_console_command_tests`. `Zone.cpp` is
1,265 lines, held there by R6g. 4.3 continues past the exit criteria as
shrink-only work.

---

## Phase 5 — Process scaffolding (runs alongside all phases)

- [x] **5.1 Upgrade server CLAUDE.md to sidecar format.** From build manual to
  "non-obvious rules and gotchas that cost time to rediscover", each rule
  pointing at the test that owns it. Add the `make test` loop, the ratchet
  rules, and this document's status conventions.
  > **Status:** done (2026-09-22) — CLAUDE.md opens with a "Rules and the
  > tests that own them" table (fourteen rules, each with its owning test or
  > ratchet and the message it prints) and a "Working in this repository"
  > section carrying the `make dev-test` / `make test` loop, the ratchet
  > discipline, this document's `> **Status:**` conventions and
  > `docs/FIXES.md`. The build, container, database and run-the-servers
  > manuals stay; task and phase numbers are out of that file, because it is
  > read by sessions that never open this one. The standing rule the task
  > leaves behind: **a sentence in CLAUDE.md that no file or command backs is
  > the failure mode this task exists to fix** — every claim there names a
  > path, a target or a count something measures, and is re-checked against
  > the tree when it is touched.

- [x] **5.2 `.claude/skills/add-packet` skill.** Modeled on sidecar's
  `add-sidecar-domain`: the checklist for adding/changing a packet — layout
  inventory regenerated in *both* repos, golden fixture added, shuffle
  branches covered, handler registered at the composition root (never on the
  packet), client-repo counterpart commit linked.
  > **Status:** done (2026-09-22) — `.claude/skills/add-packet/SKILL.md`, ten
  > steps derived from what the tests already enforce rather than from
  > habit: the `Packet.h` enumerator (appending is cheap, inserting shifts
  > every later id in both repos), the factory's `kPacketID`/`kName`/
  > `kMaxSize` and the `de::PacketFactoryType` concept, kernel membership
  > in `tests/arch/kernel_files.txt` with K1/K2, the per-server
  > `FactoryList` plus regenerating `tests/ratchet/factory_registrations.txt`,
  > the handler at the composition root through
  > `DE_REGISTER_PACKET_HANDLER` (and the `PacketValidator` set for a packet
  > sent before `GPS_NORMAL`, or `GameClientLink.h` for a client-sent
  > GC-named packet), `tests/tools/gen_factory_list.sh`, the golden
  > fixture and its recording rule, the per-code goldens a shuffled packet
  > needs, the inventory re-record in this repo and in the client's, and the
  > `wire_inventory_diff.sh` cross-check with the counterpart commit linked
  > in the PR. Every step names the file or command that proves it; steps
  > that no check enforces say so (a plain packet's missing golden is the
  > one). `.claude/skills/` is out of `.gitignore` so the skill is checked
  > in, like the scripts under `tests/`.

- [x] **5.3 Fix log.** When restructuring uncovers real bugs (1.4 layout
  diffs, races, double-frees), record them in `docs/FIXES.md` with sidecar's
  `> **Status:**` convention rather than fixing silently.
  > **Status:** done — `docs/FIXES.md`, oldest entry the 1.4 max-size
  > reconcile (2026-08-31). The Exchange defect set predates the file; the
  > rules it left behind and the items still open are in 1.4's status.
  > Ongoing discipline, not a one-shot: new finds keep landing in
  > `docs/FIXES.md`, and an entry is flipped to `fixed` in the same commit
  > as the fix, never later.

- [x] **5.4 Language standard: C++11 → C++20 required.**
  Assessed 2026-08-30. No open task was *blocked* on the standard, but 3.1
  `Outcome` wants `std::variant` + `[[nodiscard]]` (an unchecked rejection
  becomes a compiler warning instead of a convention), the packet layer
  wants `string_view`/`optional`/`if constexpr`, and googletest is pinned at
  1.12.1 only because it is the last C++11 release. At assessment time the
  image's Ubuntu 20.04/GCC 9.4 compiler had full C++17 but only partial
  `-std=c++2a`. The completed work avoids a distro/dependency bump by making
  both builders use the already-added Zig toolchain: pinned Zig 0.16.0 ships
  Clang 21.1.0 while the runtime remains Ubuntu 20.04.
  The main mechanical migration cost was pre-C++11 dynamic exception
  specifications.
  A full production-target build also found three previously uncompiled uses
  of removed library facilities: `binary_function`, `bind2nd`, and
  `set_unexpected`; all three are now gone.

  | Form | Pre-migration | C++17 | C++20 |
  |------|---------:|-------|-------|
  | typed `throw(Error)`, `throw(ProtocolException, Error)`, … | 139 files / ~650 sites | hard error | hard error |
  | empty `throw()` | ~2,600 sites | accepted (= `noexcept`) | removed (GCC/Clang warn; `-pedantic-errors` fails) |

  Plan: one mechanical PR that (a) **deletes** typed specs — never replace
  with `noexcept`: `throw(Error)` means *may throw Error*, and `noexcept`
  would turn every thrown `Error` into `std::terminate`; (b) deletes the
  empty `throw()` in the same sweep so the later C++20 flip is free; (c) sets
  `CMAKE_CXX_STANDARD 17`; (d) v18-reformats every touched file — CI demands
  it anyway, and this is the one-time reformat CLAUDE.md has been deferring,
  so land it when no feature branch is in flight; (e) `make test` green: the
  goldens and inventory prove the wire layer did not move. Then rewrite 3.1's
  "C++11 template" to `std::variant` + `[[nodiscard]]` and unpin googletest.
  Not required for the mechanical migration: modules and coroutines still need
  architecture/build evidence before adoption; concepts become useful only at
  focused boundaries such as the packet stream and factory contracts described
  in `docs/TOOLCHAIN.md`.
  > **Status:** done (2026-09-04) — C++20 is the required project standard
  > (`CMAKE_CXX_EXTENSIONS=OFF`), and CMake probes `jthread`, `stop_token`
  > and stop-aware condition-variable waits at configuration time, so an
  > unsupported toolchain fails there instead of partway through
  > compilation. The C++17 rollback lane was retired deliberately once
  > `std::jthread`/`std::stop_token` entered the production
  > `ZoneGroupThread` lifecycle. Both Dockerfiles pin Zig 0.16.0 /
  > Clang 21.1.0 and isolate build trees, output roots and compiler caches by
  > Zig version, target and build type; the runtime stays Ubuntu 20.04, whose
  > distro GCC is no longer the compiler. All dynamic exception
  > specifications are gone (R7); legacy destructors that were declared as
  > potentially throwing keep that behaviour with `noexcept(false)`.
  > `Outcome` uses `std::variant`/`[[nodiscard]]`, and GoogleTest is v1.18.0.
  > The adoption priorities and guardrails live in `docs/TOOLCHAIN.md` §3,
  > "Where C++20 pays off in DarkEden"; that is the file to extend, not this
  > one.
  >
  > The shutdown contract the migration settled, because it is easy to break
  > by accident: every worker in all three processes is a `ManagedThread`
  > (the only remaining subclass of the legacy `Thread`). SIGTERM/SIGINT only
  > store a lock-free atomic request; each main loop returns, every worker is
  > asked to stop **before** any join, and the joins happen while the
  > workers' dependencies are still alive — `GameServer` destroys the zone
  > pool before the zone and database objects it uses. A derived destructor
  > must stop and join before its own members disappear; a base destructor is
  > too late. Main then `_Exit`s rather than running the legacy singleton
  > graph's unaudited destructors, so **joining workers adds no world-save
  > guarantee**. A 30-second watchdog per process forces a failed exit if I/O
  > or a heartbeat stays blocked, naming the process it kills, and
  > `docker/start.sh` bounds the gameserver drain at 35 seconds and the
  > login/shared drain at 8 after it, inside Compose's 45-second
  > `stop_grace_period`.
  - Owner: ratchet R7, held at 0.

- [ ] **5.5 Packet-read fuzzing.** Fuzz what clients send: the bytes a
  client puts on the wire, through the gates of `GameFrameGate::next`
  (run by `GamePlayer::processCommand`) and
  `LoginPlayer::processCommand`, into the packets' `read()` and the
  `toString()` every read packet is printed with. Every crash the fuzzers
  find is fixed test-first or recorded in `docs/FIXES.md`, and its input
  is replayed by ctest from then on.
  > **Status:** in progress (the CI fuzz job; the findings recorded, not
  > fixed, in `docs/FIXES.md` under "Packet-read fuzzing": enumerators
  > cast from wire bytes, non-protocol exceptions escaping the receive
  > loops, and the records six GC reads leak, `GCUpdateInfo`'s among
  > them, which is why the fuzz runs have leak detection off). The
  > targets are in `tests/fuzz/` (game and login),
  > `DARKEDEN_BUILD_FUZZERS` builds them with libFuzzer in
  > `tools/fuzz/Dockerfile.fuzz`, and the replay builds run in the zig
  > suite over the golden seed corpus and
  > `tests/fuzz/regressions/` (ctests `fuzz_replay_game`,
  > `fuzz_replay_login`, `fuzz_replay_game_any_id`); each fix but
  > the validator's has a gtest in `tests/packet_read_bounds_test.cpp`
  > (`wire_tests`). The client links admit only what a client sends
  > (`game_client_link_tests`, `login_client_link_tests`), so the GC, GS
  > and SG reads, the six leaking ones among them, are behind the
  > gameserver's gate: `fuzz_replay_game` and a default libFuzzer run
  > stop them there, and `fuzz_replay_game_any_id` (`DE_FUZZ_ANY_ID=1`)
  > still reads them. Fixed so far:
  > the validator's status table, the client links' in-game and
  > kick-verify sets, name-table lookups in debug strings
  > (including `PCVampireInfo`'s),
  > shop and stash slot indices, wire bools, repeated script parameter
  > names, the store item count, and the frame bound: `readPacket` holds
  > every read to the body its header declares (`FrameBoundTest` in
  > `tests/packet_frame_test.cpp`), and the targets abort unless it
  > moves the stream exactly one frame on, and the gameserver's
  > sequence count, which moves only when a frame is consumed
  > (`GameFrameGate`, `game_frame_gate_tests`): the targets deliver
  > each input whole, byte by byte and in hashed chunks and abort
  > unless the three agree, and the game target delivers each input
  > byte by byte through `GameFrameGate::next` itself as well, holding
  > the production gate to its mirror's result. Next reader: a UDP target
  > for the loginserver's GM datagrams (`DatagramFactoryRead.cpp`) is
  > not written, and the targets deliver each input from the start of
  > the stream's buffer, so its wrap-around path is not fuzzed
  > (`StreamFuzz.h`).
  - Owner: the replay ctests (every recorded input replays clean, and a
    read may refuse a body only with a ProtocolException), the gtests
    in `tests/packet_read_bounds_test.cpp` (each fix's refusal or value)
    and in `tests/packet_frame_test.cpp` (the frame bound, over every
    golden a server reads), and the client-link gtests (each server's
    client connection admits only what a client sends); the
    gameserver's GC link, narrowed to the GC-named packets its client
    link admits, refuses at compile time a GC handler that link would
    never deliver.

---

## Phase 6 — CI (deliberately last; limited by GitHub Actions minutes)

The C++20 lifecycle work now has a pinned-Zig Debug test and production-build
workflow (6.1, master pushes/merges only). Execution still depends on Actions
minutes being available. The remaining broader CI rollout below is deferred:

- [x] **6.1 Build + test workflow.** Pinned Zig Debug contract/lifecycle tests,
  all production targets, and the production image; master pushes/merges only.
  > **Status:** done — `.github/workflows/cpp20.yml`; running the jobs still
  > requires available Actions minutes. Local verification remains mandatory.

- [ ] **6.2 Integration tier in CI.** MySQL service container + `initdb/`
  schema for the repository integration tests.
  > **Status:** blocked (no Actions minutes)

---

## Legacy service cleanup (2026-09-05)

- [x] **Remove China billing.**
  > **Status:** done (2026-09-05) — removed the entire `src/server/chinabilling/` tree (including
  > its stress/test servers), both CBilling library targets and legacy Makefile
  > links, `EventCBilling`, the player-info mixins, dormant login/game hooks,
  > and the dedicated database connection API. The feature switch was already
  > disabled, so supported builds retain their existing login/payment behavior.
  > `PaySystem` remains intact.
  > Client packet IDs/error codes, string-table IDs/data, and the historical
  > `src/build_error.sql` build transcript are deliberately unchanged.
  > R1: 351 → 345; R3: 85 → 84; R5: 5,899 → 5,897.
  > The ratchet suite rejects China billing references in source/build files.
  > Verified with the pinned Zig/C++20 Debug toolchain: all three production
  > servers build, all nine CTest suites pass, and touched C++ files pass
  > clang-format 18. No live deployment was restarted for this cleanup.

- [x] **Remove `theoneserver`.**
  > **Status:** done (2026-09-05) — removed on 2026-09-05, following the audit below. Deleted
  > the whole `src/server/theoneserver/` tree (its private `Connection`/
  > `Statement`/`Result`/`DatabaseManager` forks, `GameServerManager`, the
  > unbuilt `KillManager` duplicate, `UDPManager`, thread-pool forks,
  > `PacketFactoryManager`, `TheOneServer` and `main.cpp`) and the three
  > `alltheoneserver` recipes in `src/Makefile`, `src/Core/Makefile` and
  > `src/server/Makefile`. Nothing else referenced the tree: no CMake
  > target, install entry, configuration, or Docker startup ever did.
  > R1: 345 → 338 (the tree's seven `g_p*` externs); R3: 84 → 81 (its
  > three SQL-bearing forks). R5 is gameserver-scoped and unchanged.
  > The ratchet regression scan that rejects China billing references now
  > also rejects `theoneserver`/`TOpackets` in source and build files;
  > the historical `src/build_error.sql` transcript is deliberately
  > untouched. Generic `GGCommand` handling, the shared `database/`
  > classes, and the login/shared-server managers are unchanged — the
  > supported servers still use them.
  >
  > **Audit (2026-09-05, pre-removal):** obsolete in the supported
  > repository build and deployment.
  >
  > Its role was publisher server registration/policy enforcement, not a
  > fourth gameplay service. `TheOneServer.cpp` created a database manager
  > and UDP `GameServerManager`; the latter bound `TheOneServerUDPPort`
  > and dispatched received datagrams. Its local `PacketFactoryManager.cpp`
  > registered only `GTOAcknowledgement`.
  >
  > The deleted handler (inspect
  > `git show 25f25ee6^:src/Core/TOpackets/GTOAcknowledgementHandler.cpp`)
  > recorded server IP/port/heartbeat information in
  > `TheOneServerRules_New`, consulted `NEWSERVER_POLICY`, and sent
  > `GGCommand` remote-control commands such as `*shutdown 0` under
  > deny/kill policies. This is historical functionality, not a live path.
  >
  > Commit `25f25ee6` already deleted the TOpackets protocol and the
  > China/Thailand-only phone-home sender/timer in gameserver's
  > `ClientManager.cpp`. No remaining caller outside the dead tree was
  > found. The legacy `alltheoneserver` Makefile recipes required the
  > deleted `libTheOneServerPackets.a` and packet headers and could not
  > resurrect a working service. `KillManager.{h,cpp}` was an unbuilt
  > duplicate of `GameServerManager`, not a separate active kill worker;
  > `UDPManager` construction was commented out and it was absent from
  > the legacy object list. The audit covered the checked-in
  > build/deployment, not independently maintained out-of-tree binaries
  > or installations. The tree remains recoverable from git history
  > (`git show <this commit>^:src/server/theoneserver/`).

- [x] **Remove `updateserver`.**
  > **Status:** done (2026-09-05) — removed on 2026-09-05. Deleted `src/server/updateserver/`
  > (the TCP patch-distribution daemon `UpdateServer`/`UpdateServerPlayer`,
  > `main.cpp`, and ~7,000 lines of one-off patch-manifest generators —
  > `update.cpp`, `update2.cpp`, `p.cpp`, `p21.cpp`, `semiup.cpp`,
  > `info.cpp`, `fuck.cpp` — that are not valid C++ and were never in any
  > object list), `src/server/old_update.tar` (a 2000s-era tarball of the
  > same tree), `conf/updateserver.conf`, and the Core patch-manifest
  > classes `Update.{h,cpp}` / `UpdateManager.{h,cpp}` whose only consumer
  > was this daemon (dropped from `tests/arch/kernel_files.txt`; they held
  > no packet factory, so the wire inventory is unchanged). `UpdateDef.h`
  > stays: `Resource` uses its size typedefs. Also removed the
  > `UpdateServerDatabase` CMake target — a fourth copy of `database/`
  > compiled under `__UPDATE_SERVER__`, which no source in `database/`
  > tested and no executable linked — plus the `.us.o` suffix rules, the
  > `libUpdateServerPackets.a`/`libUpdateServerDatabase.a` recipes and
  > the `updateserver` clean hooks in the legacy Makefiles. The
  > `__UPDATE_SERVER__`/`__UPDATE_CLIENT__` `PlayerStatus` enum branches
  > are gone; they were `#elif` alternatives to the game/login/shared
  > branches, so no supported build's enum values move.
  >
  > **Audit:** the daemon served the launcher's file-patch protocol
  > (`Upackets`: `CUBeginUpdate`/`CURequest`/`CUEndUpdate`/`UCUpdateList`)
  > over its own TCP port from `PatchDir`, versioned by `PatchVersion`.
  > `Upackets` was deleted in commit `25f25ee6`, so the tree had not
  > compiled since; it was already commented out of the legacy
  > `src/server/Makefile` `all` target, and no CMake target, Docker
  > startup, or `docker/conf` entry referenced it. The loginserver's
  > `CLVersionCheck` still enforces the client version independently.
  > R1: 338 → 337 (the tree's `g_pConfig` extern); R3/R5 unchanged. The
  > ratchet regression scan now also rejects `updateserver`/`Upackets`/
  > `__UPDATE_SERVER__`/`__UPDATE_CLIENT__` in source and build files.

- [x] **Remove `cacheserver`.**
  > **Status:** done (2026-09-05) — removed on 2026-09-05. `src/server/cacheserver/` was a
  > two-file (~220 line) "Database Cache Server" prototype from February
  > 2002 by a Metrotech contractor, untouched since the ragezone recovery.
  > `main.cpp` was the gameserver's main loop with the names swapped,
  > calling a `CacheServer` class defined nowhere in the repository and
  > including the gameserver's `GameServer.h`; `Query.h` was a broken
  > query-record class whose setters had degraded into invalid text. No
  > CMake target, legacy Makefile recipe, Docker/config entry, or source
  > outside the directory referenced it. It held no `g_p*` externs or
  > SQL, so R1–R7 are unchanged; the ratchet regression scan now rejects
  > `cacheserver` too.

- [x] **Remove the dead sub-trees, stale forks and VSS residue.**
  > **Status:** done (2026-09-05) — removed on 2026-09-05. Nothing below was compiled by any
  > CMake target; the legacy Makefiles carried only commented-out or
  > include-path references.
  >
  > Deleted inside `gameserver/`: `test/` (a 2002 cppunit suite plus
  > `MockPlayer`/`MockZone`/`MockSkillInput`, driven by a `-t` argv flag
  > and `g_pTestConfig`; its `GameServerTester.h` include in
  > `GameServer.cpp`, the `test` include path, the `-t` parsing in
  > `main.cpp` and the `g_pTestConfig` global in Core `Properties` went
  > with it), `testAlone/` (a standalone scheduler/war harness carrying
  > private forks of `VSDateTime`, `Mutex`, `StringStream`, `Schedule`,
  > `Scheduler`, `WarSystem`), `mofus/testserver/` (a fake mofus peer),
  > `quest/Squest/` (the pre-Lua "simple quest" system, reachable only
  > through `__ACTIVE_QUEST__` blocks that no build defines),
  > `gameguard/` (a 2003 INCA nProtect `CSAuth` SEED-cipher header and
  > `.tab`/`.idx` tables; `ObjectManager.cpp`, `CGAuthKeyHandler.cpp` and
  > `CGSayHandler.cpp` still included the header around commented-out
  > calls — the includes and `ObjectManager`'s two "CSAuth ...
  > Initialization" print pairs are removed, so startup logs four fewer
  > lines), `billing/test/` (config fixtures),
  > `item/_{weapon,armor,gear,accessory,etc}/` (leftover `create`
  > scripts) and `quest/luaScript/test/` (xmas-event Lua fixtures with
  > compiled `.luac` copies). The stale commented-out `test_exchange`
  > block in `gameserver/CMakeLists.txt` referred to a file that no
  > longer existed.
  >
  > Deleted in the `src/server/` root: the unbuilt ServerCore forks
  > `Restore.cpp`, `Restore2.cpp`, `ZoneUtil.cpp`,
  > `IncomingPlayerManager.cpp` (each a stale copy of a gameserver file)
  > and `UserGateway.{h,cpp}` (referenced only from comments).
  >
  > Deleted everywhere: all 27 `vssver.scc` Visual SourceSafe files.
  >
  > **Ported to the gtest suite** rather than deleted: the
  > `GameServerSkillTest` sharp-shield expectations now live in
  > `tests/formula_test.cpp` against `decore::skillformula::SharpShield`
  > (with the 2003 "Delay equals Duration" balance, which the old test
  > predated); `GameServerWarTest`'s `VSDateTime` arithmetic and
  > `testAlone`'s `ScheduleTest`/`WarSystemTest` scheduler behaviour are
  > `tests/scheduler_test.cpp`, a new `scheduler_tests` target linking
  > de-kernel plus the live `war/Schedule.cpp`/`Scheduler.cpp`. The live
  > `Schedule::heartbeat()` reads the wall clock (the old harness stepped
  > a fake one), so the port uses past/future scheduled times instead.
  > Not portable: `GameServerItemTest`/`ExpTest` (need
  > `g_pVariableManager`/`g_pLuckInfoManager` loaded from the DB),
  > `SpeedCheckTest` (needs a live `GamePlayer` socket) and
  > `UserGatewayTest` (its subject was deleted).
  >
  > R1: 337 → 333 (`g_pTestConfig` plus three externs in the deleted
  > trees); R3: 81 → 74 (the four root forks and three `Squest` files);
  > R5: 5,897 → 5,790 (the deleted gameserver sub-trees). Four
  > `tests/arch/baseline.txt` C1 entries for `mofus/testserver/` went too.
  >
  > Left for a later pass: commented-out `UserGateway`/`CSAuth` calls in
  > `ClientManager.cpp`, `IncomingPlayerManager.cpp`, `EventAuth.cpp` and
  > `GamePlayer.h`, and the `__ACTIVE_QUEST__` blocks in `NPC.cpp` and
  > `PlayerCreature.cpp` that still name the deleted `QuestBoard`.

## Appendix — measured inventory (2026-08-29)

- ~502k LOC across 4,271 C++ files. `Core` 149k (1,410 packet-prefixed files
  in its root), `gameserver` 120k, `skill` 103k / 1,031 files, `item` 51k,
  `quest` 23k (Lua-integrated). The legacy `chinabilling`, `theoneserver`,
  `updateserver` and `cacheserver` trees counted here were deleted on
  2026-09-05 (see "Legacy service cleanup").
- 433 packet types in `src/Core/Packet.h`
  (`grep -cE 'PACKET_(GC|CG|CL|LC|GL|LG|GS|SG|GG)[A-Z_]* *[,=]' src/Core/Packet.h`).
  The same command prints 434 at HEAD: 1.4 restored
  `PACKET_GC_USE_SKILLCARD_OK`, whose absence had shifted every later id
  away from the client's. An enumerator count is not a factory count — the
  inventory pins 465 factories.
- Wire encryption: per-session encrypt code reorders field read/write order
  via `SHUFFLE_STATEMENT_*` (`src/Core/EncryptUtility.h`) — part of the
  contract, must be covered by golden fixtures.
- Client repo carries divergent hand-copies of all packet classes
  (`client/Client/Packet/{Gpackets,Cpackets,Lpackets,Rpackets,Types,Upackets}`).
  The `#ifdef __GAME_CLIENT__` vestiges of the shared origin were in `Core`'s
  handlers then; K2 bans the macro from kernel files now, and the seven
  mentions left in `src` are the three per-server `#if` files.
- At this inventory date, existing `test*` directories were ad-hoc standalone
  test servers and CI was clang-format only. The current `tests/` suite and
  C++20 workflow were added subsequently.
- Sidecar reference (local clone at `../sidecar`): module split
  `sidecar-kernel` ← `sidecar-core` ← `sidecar-app`; enforcement vocabulary in
  `sidecar-kernel/src/test/java/.../kernel/arch/ArchitectureRules.java`;
  conventions in its `CLAUDE.md`.
