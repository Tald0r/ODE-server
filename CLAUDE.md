# CLAUDE.md

Guidance for AI-assisted sessions in this repo: the **DarkEden** MMORPG server, written in C++20
and split into three cooperating processes (loginserver, sharedserver, gameserver).
[README.md](README.md) says how to build, set up and run it. The rules, and the incidents behind
them, live in the knowledge bundle under `docs/` (Google's Open Knowledge Format): start at
[docs/index.md](docs/index.md); [docs/knowledge-bundle.md](docs/knowledge-bundle.md) says how it is
kept. This file is the index: one line per rule and a link to the concept that explains it.
**Open the linked concept before you work in its area** - the line here is a reminder, not the
rule.

## Build & test

```bash
make dev-test            # the whole suite, built in the darkeden-dev container (the loop; on Windows, the one to use)
make test                # the same suite with a local C++20 compiler and library
make dev-build           # every production target, in the container
make debug               # local Debug build into bin/ and lib/ (make release, make clean)
make integration-test    # the MySQL*Repository impls against a throwaway MySQL 5.7 (docker + darkeden-dev)
perl tests/tools/check_knowledge_bundle.pl .   # the bundle check alone, before a docs push
```

- Run `make dev-test` or `make test` before every push: CI builds master pushes only, so a branch is verified locally - [CI](docs/build/ci.md), [testing overview](docs/testing/overview.md).
- **Never run bare `make fmt`**: most of `src/` predates clang-format 18, so it buries your diff in churn. Format only what you touched: `git diff --name-only master HEAD | grep -E '\.(cpp|h|hpp)$' | xargs clang-format -i`; a touched file that was never reformatted gets reformatted in your PR - [formatting](docs/build/formatting.md).
- Never compile off the bind-mounted checkout; `tools/devbuild.sh` builds in a volume, and its artifacts never reach `bin/`. Two worktrees building at once each need their own `DEVBUILD_WORK_VOLUME` - [container builds](docs/build/container-builds.md).
- Develop on the Debug build: `DE_OWNERSHIP_CHECKS` is armed only there; `NDEBUG` is never defined - [build commands](docs/build/build-commands.md), [assertions and NDEBUG](docs/architecture/assertions-never-ndebug.md).
- Prefer the pinned Zig 0.16.0 container: Ubuntu 20.04's GCC 9 lacks the library facilities CMake checks for (`jthread`, `stop_token`) - [toolchain notes](docs/build/toolchain.md), [dependencies](docs/build/dependencies.md).
- Native macOS builds need the system `assert.h` forced in for the suite - [native macOS builds](docs/build/native-macos.md).

## Working rules

- Every rule is owned by a test, ratchet or gate that fails, never by a memory; the whole rule/owner/failure table is [rules and owners](docs/testing/rules-and-owners.md).
- Ratchet numbers only go down: tighten a drop in `tests/ratchet/ratchets.sh` **and** the plan's table in the same commit; every baseline comes from running its command - [ratchets](docs/testing/ratchets.md).
- Each task in `docs/reviews/RESTRUCTURING.md` has a `> **Status:**` line updated in the same commit as the work; a task is `done` only once its owner exists - [fix log and task status](docs/testing/fix-log-and-status.md).
- A bug found while restructuring goes in `docs/reviews/FIXES.md` with a Status line, never fixed silently - [fix log and task status](docs/testing/fix-log-and-status.md).
- A change to `src/domain` is done only when `bash tests/tools/decore_client_diff.sh <client-root>` is clean against a resynced client; a changed vector row is a balance change - [de-core parity](docs/protocol/decore-parity.md).
- A failing golden or inventory diff is a protocol change, not a test to silence: ship the identical change in the client repo, link the commits, re-record with `UPDATE_GOLDENS=1 ./bin/wire_tests` - [wire contract](docs/protocol/wire-contract.md).
- Comments under `src/` and `tests/` are English and describe behaviour: no task numbers, dates, review history or migration narrative; translate legacy Korean or mojibake comments you meet - [dead sources and source encoding](docs/architecture/dead-sources-and-encoding.md).
- A change to a rule or a design updates its concept in the same commit; a moved document gets every reference repointed and a `docs/log.md` entry - [the knowledge bundle](docs/knowledge-bundle.md).
- A sentence here or in a concept that no file, command or count backs is the failure mode to fix; re-check a claim against the tree when you touch it, and the code wins a disagreement - [the knowledge bundle](docs/knowledge-bundle.md).

## Architecture

- Three processes, each one `main.cpp` over a runtime OBJECT library the runtime tests link too; link a runtime with `target_link_libraries`, one per executable - [server architecture](docs/architecture/overview.md).
- `de-kernel` includes only kernel files and no server macro (K1-K3), ServerCore no MySQL, Lua or socket header (C1), `src/domain` only itself (D1) - [module layering](docs/architecture/module-layering.md).
- No `g_p*` extern: take a manager from its layer's context (`de::kernelContext()` ... `de::sharedContext()`) - [contexts, not globals](docs/architecture/contexts-not-globals.md).
- Every `.cpp` is named by a target and every header included; sources are UTF-8, and a line with a non-ASCII byte fails R17 - [dead sources and source encoding](docs/architecture/dead-sources-and-encoding.md).
- `throw Error("text")`, never `throw "text"`; gameplay results are `Outcome<Events, Rejection>` - [exceptions and outcomes](docs/architecture/exceptions-and-outcomes.md).
- Server-type macros and `__COMBAT__` are set per target; never reintroduce a macro nothing defines; `__MOFUS__`/`__METRO_SERVER__` are not switches - [preprocessor macros](docs/architecture/preprocessor-macros.md).
- Each `main()` is composed from the startup, port, application, lifecycle, shutdown and worker libraries - [startup composition](docs/startup/overview.md), [lifecycle rules](docs/startup/lifecycle-rules.md), [sockets and connections](docs/startup/sockets-and-connections.md), [workers and synchronization](docs/startup/workers-and-synchronization.md).

## Threading

- Zone-group state is touched only while that group's mutex is held; `Zone::m_Mutex` does not satisfy this, and the Debug check aborts, never throws - [the mutation rule](docs/threading/mutation-rule.md).
- Never hand-write `x.unlock()` inside `__ENTER_CRITICAL_SECTION`; release early with `__CRITICAL_SECTION_LOCK.unlock()` - [critical sections](docs/threading/critical-sections.md).
- From another thread, only send to a player's socket; everything else goes through `de::postToPlayer` (commands capture by value), `ZoneGroup::post()` or a `de::Snapshot` (a change is pure work on the copy) - [cross-thread communication](docs/threading/cross-thread-communication.md).
- Which thread owns what, and the thread-local DB connection each worker registers - [threads and what they own](docs/threading/overview.md).
- No creature is written to a `Tile` outside `ZoneSpawn.cpp`/`ZoneMove.cpp`; war work is posted to the owning group; guilds are retired, never freed - [closed violations](docs/threading/closed-violations.md).

## Packets

- A packet is a wire class in `src/Core` with no `execute()` and a handler bound at its server's composition root; follow the `add-packet` skill (`.claude/skills/add-packet/SKILL.md`) - [packet system](docs/protocol/overview.md).
- Factories carry `kPacketID`/`kName`/`kMaxSize`; edit the `constexpr` factory lists, never an `addFactory` sequence - [packet system](docs/protocol/overview.md).
- Each server registers exactly the factories it registered before, on an add or a drop - [factory registrations](docs/protocol/factory-registrations.md).
- A client connection admits only what a client sends; a new client-sent GC packet needs its id in `src/Core/GameClientLink.h` - [client link](docs/protocol/client-link.md).
- A `read()` sees exactly its declared body and refuses a malformed one only with a `ProtocolException` - [frame gate](docs/protocol/frame-gate.md).
- Every input that ever crashed a fuzz target goes in `tests/fuzz/regressions/` with its fix - [packet-read fuzzing](docs/protocol/fuzzing.md).

## Database and repositories

- All SQL lives behind a `*Repository.h` seam with a `MySQL*Repository.cpp` impl; read the header's `Not enclosed` list first, move statements byte-for-byte, and use the parameterized `executeQuery` (`%u`, `%d`, `%ld`) - [repository seams](docs/architecture/repository-seams.md).
- MySQL 5.7 or 8 with the relaxed `sql_mode` (no `NO_ZERO_DATE`, `STRICT_TRANS_TABLES` or `NO_BACKSLASH_ESCAPES`; `ONLY_FULL_GROUP_BY` stays), utf8mb4 end to end with a session pin; migrations are one-time and nothing checks them - [database setup](docs/database/overview.md).
- Every `initdb/` table is InnoDB utf8mb4, every seed `Player` row an argon2id hash, the seed unions consistent - [seed-data rules](docs/database/seed-data-rules.md).

## Configuration and operations

- `conf/` keys, the two connection blocks and the Exchange's one-server rule; diagnostics carry no SQL or private payloads - [configuration](docs/database/configuration.md).
- Read configuration through `de::kernelContext().config()`; a read before `main()` registers it is a startup-order bug - [contexts, not globals](docs/architecture/contexts-not-globals.md).
- Start login, shared, then game; `docker compose down` drains the gameserver first - [running the servers](docs/operations/running-the-servers.md).
- Diagnostics are off unless `DARKEDEN_TRACE=1` - [production logging](docs/operations/logging.md); browser clients go through the [WebSocket gateway](docs/operations/websocket-gateway.md).

## Per-process rules

Before changing a server's flows, read its folder's overview and rule tables. The runtime tests
link the production objects without `main.cpp`, with supplied repositories and injected
allocation faults; what each one exercises is in [test coverage](docs/testing/test-coverage.md).

- Loginserver: every acquisition owns its account identity before writing LOGON, cleanup is attempted step by step and retried, and session state is published only after the reply is sent - [login lifecycle](docs/login/overview.md), [account ownership and kicks](docs/login/account-ownership-and-kicks.md), [catalogues and selection](docs/login/catalogues-and-selection.md), [character and account flows](docs/login/character-and-account-flows.md).
- Sharedserver: construction owns every manager before publishing its graph, and guild loads publish a complete graph under the table lock - [sharedserver](docs/shared/overview.md), [construction and catalogues](docs/shared/construction-and-catalogues.md), [guilds](docs/shared/guilds.md).
- Gameserver: a connection keeps an owner through registration, a player handoff keeps its source until the destination accepts, and queued broadcasts own their bytes - [gameserver](docs/game/overview.md), [connections and players](docs/game/connections-and-players.md).
- Every catalogue loader prepares owned rows before replacing its table and keeps the previous rows on failure - [world and server catalogues](docs/game/world-and-server-catalogues.md).

## Records

- [The restructuring plan](docs/reviews/RESTRUCTURING.md): every task with its Status line and the R1-R18 ratchet table.
- [The fix log](docs/reviews/FIXES.md): every bug found, newest first, each with its Status.
