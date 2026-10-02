# CLAUDE.md

Guidance for Claude Code (claude.ai/code) working in this repository: the
**DarkEden** game server, an MMORPG server written in C++20 and split into
three cooperating processes.

## Rules and the tests that own them

Every architectural rule below is owned by something that fails, never by a
memory. The full ratchet table (R1–R18), with the command that measures each
number and the reason it exists, is in `docs/RESTRUCTURING.md`;
`tests/ratchet/ratchets.sh` is the enforcing copy, run by ctest as `ratchets`.

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| A packet's bytes never change silently | the golden fixtures in `tests/golden/`, compared by `wire_tests` | a byte diff in the golden — a protocol change the client repo must ship identically |
| Every factory's packet id, name and max body size stay as inventoried | `tests/wire_layout_test.cpp` against `tests/wire-layout.txt` | the inventory diff (`tests/tools/wire_inventory_diff.sh` cross-checks the client's copy) |
| A packet that touches the encrypter is pinned at codes 0..5 | `ratchets.sh`, looking for `tests/golden/<Name>.code5.hex` | "packets use the encrypter but have no per-code goldens"; exceptions live in `tests/ratchet/encrypter_exceptions.txt` |
| Each server registers exactly the factories it registered before | `tests/tools/factory_registrations.pl` vs `tests/ratchet/factory_registrations.txt`, run by `ratchets.sh` | a membership diff, on an add **or** a drop |
| `tests/generated/AllPacketFactories.inc` matches the kernel membership | `ratchets.sh` re-runs `tests/tools/gen_factory_list.sh` and diffs | "AllPacketFactories.inc is stale" |
| A kernel file includes only kernel files and mentions no server-type macro or `__COMBAT__` (K1/K2/K3); a core file includes no MySQL, Lua or socket-transport header (C1, shrink-only baseline in `tests/arch/baseline.txt`); `src/domain/` quote-includes only existing `"domain/X.h"` headers and angle-includes only `<algorithm>`/`<cmath>` (D1) | `tests/arch/check_includes.pl`, ctest `arch_includes` | the offending include or macro, named |
| Packets carry no `execute()` — handlers register at the composition root | ratchet R4 | R4 above 0 |
| Server implementations link without `main.cpp`, with the same definitions as production | `game_server_runtime_tests`, `login_server_runtime_tests`, `shared_server_runtime_tests` | duplicate `main` at link time, a mismatched server definition, or a production dispatch/handler test failure |
| Startup arguments and login offsets are validated before a completed configuration is published | `server_startup_tests`, over the shared `ServerStartup` library, and `server_startup_cli`, over all three executables | malformed arguments accepted, an incorrect or partially applied override, a failed load replacing the published configuration, or the wrong failure status/diagnostic from main |
| Configuration parsing processes the final line without a newline and rejects missing keys and read errors before startup publication | `properties_parser_tests` over `PropertiesParser` in `de-kernel`, plus `server_startup_tests` and `server_startup_cli` | a dropped final property, a malformed line accepted, a read failure spinning or silently succeeding, or a directory accepted as configuration |
| Every server requests shutdown before stopping, attempts cleanup after failed startup, and preserves worker failures in its exit status | `server_lifecycle_tests`, over the shared `ServerLifecycle` library used by all three entry points | starting after a shutdown request, missing or repeated cleanup, lost failure diagnostics, an incorrect drain result or a successful exit after failure |
| SIGTERM/SIGINT request shutdown from any thread, and the process deadline bounds blocked initialization and cleanup | `server_process_shutdown_tests`, over the `ServerProcessShutdown` guard used by all three entry points and the shared lifecycle runner | a missed signal, a clean exit after the deadline, a deadline firing before any request or after guard destruction, or signal handlers left installed after the guard's scope |
| Fatal allocation handlers report failure without C++ heap allocation or normal process teardown | `server_fatal_handler_tests`, over the shared `ServerFatalHandlers` guard, with allocation faults confined to subprocesses | a successful allocation-failure exit, recursive allocation while reporting, an exit callback running, a lost diagnostic or a handler left installed after its scope |
| Core-dump setup respects the inherited hard limit; game process initialization seeds rand and ignores only its three legacy signals | `server_process_environment_tests`, over `ServerProcessEnvironment`, with process mutations confined to subprocesses | a failed soft-limit raise under a finite cap, a changed hard limit or unrelated resource, an incorrect seeded sequence, or damaged shutdown handlers/state |
| Application configuration is published only after a successful load, stays alive through cleanup and the result, and restores its previous context binding on scope exit | `server_application_tests`, over the shared `ServerApplication` used by all three entry points | actions running after a configuration error, an incorrect effective login override, a lost binding or failure status, an incorrect drain diagnostic, or unflushed final output |
| Server shutdown requests every auxiliary worker's stop before draining game zones or joining any auxiliary worker; retained worker failures remain visible after all joins | `server_worker_shutdown_tests`, over `ServerWorkerShutdown` used by all three runtimes | a join before all stop requests, a lost or incorrectly named failure, skipped joins after a retained run failure, or a successful lifecycle exit after worker failure |
| No `executeQuery` outside `src/server/database/` and the `repository/` directories | ratchets R2/R3 | R2/R3 above 0 |
| A critical section is never unlocked by hand | `tests/tools/critical_section_audit.pl`, ctest `critical_section_audit` | the file and line of the hand-written `unlock()` |
| Zone-group state is touched only under that group's mutex | `ZoneGroup::assertOwned()` under `DE_OWNERSHIP_CHECKS` (Debug builds only) | `abort()` at the gateway |
| Every seed `Player` row ships a current argon2id hash | `tests/password_hash_test.cpp`, which reads `initdb/DARKEDEN.sql` | the seed account missing from the test's password map |
| Every `initdb/` table is InnoDB in `utf8mb4` / `utf8mb4_unicode_ci` | `ratchets.sh` | the table, named |
| No seed guild leads or joins two guild unions | `ratchets.sh` | the guild and its unions, named |
| Every seed union has a member or a pending join offer, and every seed join offer names a union | `ratchets.sh` | the union or the offer, named |
| Every `src/**/*.cpp` is compiled by some target, every header is included | ratchets R15/R16 | the dead file, listed |
| The de-core subset the client vendors computes exactly the parity vectors and compiles without a warning under a GCC/Clang proxy for the client's warning set (MSVC's C4146 and C4805 have no counterpart) | `formula_tests`' `SharedVectors` over `src/domain/vectors/`; `de-core-strict` (`-Werror`, built by `make dev-test`) | the row, named, with its expected and actual value; the warning, as an error |
| `NDEBUG` is never defined, so `Assert` and `__BEGIN_TRY`/`__END_CATCH` keep one meaning | an `#error` in `src/Core/Assert.h` and `src/Core/Exception.h` | the compile of every project file, in any configuration that defines it |
| A client's bytes reach a packet's `read()` only through the receive loops' gates, and a malformed body is refused only with a `ProtocolException` | the packet-read fuzz targets in `tests/fuzz/`; their replay ctests `fuzz_replay_game`, `fuzz_replay_login` and `fuzz_replay_game_any_id` run every golden seed and every input in `tests/fuzz/regressions/`, and the targets abort on any other exception; and the gtests in `tests/packet_read_bounds_test.cpp` (`wire_tests`), which pin each fix's refusal or value | the input, named, aborting the replay (a zig Debug UB trap, an assertion, or a non-protocol exception); the gtest, named, with the refusal or value it expected |
| A packet's `read()` sees exactly the body its header declares: `SocketInputStream::readPacket` bounds every read, peek and skip to it, refuses (with `InvalidProtocolException`) a body left unread, overrun or failed inside `read()`, and leaves the stream at the next frame on every exit | `tests/packet_frame_test.cpp` (`FrameBoundTest` in `wire_tests`), whose golden sweep reads every golden a server reads as one frame; the fuzz targets' body oracle | the case, named; the golden, named, with the bytes declared and consumed; the fuzz input, aborting the replay |
| The gameserver checks and counts a client's sequence byte only where the frame is consumed, so a frame split across TCP receives is read as if it had arrived whole, and a whole frame out of sequence still ends the connection with `DisconnectException("Packet sequence error")`, logged to `SequenceError.txt` | `game_frame_gate_tests` (`tests/game_frame_gate_test.cpp`), over `GameFrameGate`, the gate `GamePlayer::processCommand` runs each frame through, which own each case's step and refusal and the log file and exception each refusal ends in (not each log line's format, which `GamePlayer::refuseFrame` keeps and only reading checks); the fuzz targets, whose three deliveries of every input hold the mirrored loops to one result wherever the bytes are cut, and whose game target delivers every input byte by byte through `GameFrameGate::next` itself and holds it to the mirror's whole result (not under `DE_FUZZ_ANY_ID` or `DE_FUZZ_NO_STORE_SKIP`, which the gate has no hook for) | the case, named, with the step or refusal it expected; the fuzz input, aborting the replay with the delivery and the first difference |
| A client's connection admits only the packets a client sends: the gameserver's `GPS_NORMAL` set is folded from its factory lists (every CG packet but the datagram-only `CGPortCheck`, plus the GC-named packets `src/Core/GameClientLink.h` lists), and no other game or login status admits a registered packet a client does not send | `game_client_link_tests` and `login_client_link_tests` (`tests/*_client_link_test.cpp`, each compiled as its server), which check every id against the factory table's names; the gameserver's `kReceivedDirections`, whose GC link is narrowed to that list (`DirectionSet::narrowed`) and which every `DE_REGISTER_PACKET_HANDLER*` checks with `admits()` | the id, named, with the status that admits or refuses it; a GC handler registration the client link would refuse, as a compile error of `GameServerRuntime` in both `make dev-test` and `make dev-build`; `game_server_runtime_tests` also exercises that production dispatch table, while `tests/packet_meta_test.cpp` pins the narrowing mechanism and `game_client_link_tests` pins the admitted ids |
| Repository SQL behaves against a real MySQL | `make integration-test` (`tests/integration/`, needs docker) | the failing statement |

## Working in this repository

- **The loop.** `make dev-test` builds the suite in the container and runs
  all of ctest; `make test` does the same with a local C++20-capable
  compiler *and library*. Run one before every push. The C++20 workflow
  (`.github/workflows/cpp20.yml`) runs the suite and the production builds
  only on master pushes/merges — feature branches are verified locally, to
  conserve Actions minutes.
- **Ratchet numbers only go down.** `ratchets.sh` fails in both directions:
  above the baseline is new debt, below it is progress nobody wrote down.
  When a number drops, tighten it in `tests/ratchet/ratchets.sh` **and** in
  `docs/RESTRUCTURING.md` in the same commit. Every baseline comes from
  running its command, never from an estimate.
- **Task status lines.** `docs/RESTRUCTURING.md` is the living restructuring
  plan. Each task has a checkbox and a `> **Status:**` line — `not started` |
  `in progress (<what remains>)` | `done (<commit>)` | `dropped (<why>)` |
  `blocked (<on what>)` —
  updated in the same commit as the work it describes. It records the current
  state and what the next reader needs to act (conventions, open bugs, what
  remains); the narrative of a change belongs in its PR description and
  commit message. A task is `done` only once its **Owner** exists — the test
  or mechanism that keeps the rule true from then on.
- **Bugs found while restructuring** are recorded in `docs/FIXES.md` with the
  status convention `fixed (<branch>)` | `recorded, not fixed (<branch>)` |
  `open` | `not a defect` rather than fixed silently.
- **A change to `src/domain` is done only when
  `bash tests/tools/decore_client_diff.sh <client-root>` is clean** against a
  client checkout resynced from this one (its `tools/decore/sync.pl`): the
  client builds a byte-identical copy of the vendored subset, so the two
  land together, the server first (`docs/RESTRUCTURING.md` task 3.6). A
  changed vector row is a balance change: review the `.tsv` diff as one.
- **A failing golden or inventory diff is a protocol change**, not a test to
  silence. The client repo keeps hand-maintained copies of every packet
  class; ship the identical change there and link the two commits. Re-record
  deliberately: `UPDATE_GOLDENS=1 ./bin/wire_tests`.

## Build Commands

### Building the project

```bash
make            # Debug build (default)
make debug      # same as `make`
make release    # Release build
make clean      # remove build/, bin/, lib/
```

The project uses CMake; the Makefile wraps it. `make` / `make debug`
configures with `-DCMAKE_BUILD_TYPE=Debug`, `make release` with `Release`;
both build with tests off (`-DDARKEDEN_BUILD_TESTS=OFF`). Binaries go to
`bin/`, libraries to `lib/`. For development, always choose the debug build:
`DE_OWNERSHIP_CHECKS` is armed only there. `Assert` and `__BEGIN_TRY` stay
live in every configuration, because `NDEBUG` is never defined: the
optimized configurations pass `-UNDEBUG` (`zig c++` defines it from `-O1`
up), and `Assert.h` and `Exception.h` refuse to compile with it.

### Code formatting

```bash
make fmt            # format all C++ code under src/ and tests/
make fmt-check      # check the files you modified (fast)
make fmt-check-all  # check every file (slow)
```

The project uses clang-format with a `.clang-format` configuration file.
- Format checking runs via GitHub Actions only on pushes/merges to master
  (`.github/workflows/format-check.yml`), on the files changed since the
  previous master tip, using whatever clang-format `ubuntu-latest` installs
  (18.x as of 2026-08).
- **Do not run bare `make fmt` before committing — the tree is not v18-clean.**
  Most of `src/` was formatted with an older clang-format, so `make fmt`
  reformats files you never touched and buries your diff in unrelated churn.
  Format only what you touched:
  ```bash
  git diff --name-only master HEAD | grep -E '\.(cpp|h|hpp)$' | xargs clang-format -i
  ```
- Touching a file that was never v18-formatted makes CI demand you reformat
  it. That is expected; the reformat lands in your PR.
- `make fmt` / `fmt-check-all` cover `src/` **and** `tests/`, matching CI.

### Tests

```bash
make test              # build and run the suite with a local C++20 compiler
make dev-test          # same suite, built in the container off a local
                       # workspace. On a Windows host, this is the one to use.
make integration-test  # MySQL-backed repository tier (throwaway MySQL 5.7 +
                       # initdb/ schema + the real MySQL*Repository impls).
                       # Needs docker + the darkeden-dev image.
```

Prefer the pinned Zig container. Ubuntu 20.04's distro GCC 9 lacks the
required library facilities; CMake checks `jthread`, `stop_token` and
stop-aware condition-variable waits at configure time.

Beyond the rules table above: the suite (in `tests/`) carries golden byte
fixtures and loopback round-trips for most registered packet factories (the
wire-layout inventory covers all 465, the goldens 451), and
`tests/password_hash_test.cpp` pins the loginserver's argon2id hashing
(`src/server/loginserver/PasswordHash.cpp`) against upstream argon2's own
vectors. The whole packet set is compiled once, macro-free, in the
`de-kernel` library — the same archive the deployed servers and the tests
link; membership is `tests/arch/kernel_files.txt`, and `TestPackets` is only
the define-free factory/validator trio on top of it. Kernel files may not
mention server-type macros or `__COMBAT__` (K2), so the wire layer stays
buildable — and identical — alone. (This is not the client's config, which
defines `__GAME_CLIENT__=1`.)

#### Packet-read fuzzing

`tests/fuzz/` holds two fuzz targets, one per server a client talks to.
Each mirrors its receive loop (the gameserver's per-frame gate,
`GameFrameGate::next`, which `GamePlayer::processCommand` runs, and
`LoginPlayer::processCommand`) up to the packet's `read()`, stops short
of the handler, and prints the packet with `toString()` as the loop does.
An input is `[code byte][status byte][raw stream]`; `StreamFuzz.h`
describes it and the switches. Each input is delivered three times (the
game target once more, through the production gate), to a fresh session:
whole, one byte per receive, and in 1-to-48-byte chunks cut by a hash of
the input, with the loop run after every receive; a target aborts unless
the three read the same packets and end the same way, so a loop that
depends on where TCP cut the bytes is a crash. The game target also links
the production `GameFrameGate`, delivers every input byte by byte through
it, and aborts unless it ends as the mirror's whole delivery did, so a
gate that drifts from its mirror is a crash too; the switches below turn
that check off, since the gate has no hook for them. Any exception but a
`ProtocolException` is a crash unless `DE_FUZZ_ALLOW_EXCEPTIONS=1`; a
`readPacket` that returns or refuses without moving the stream exactly
the frame its header declared always is. The game target's validator is
the gameserver's, so in `GPS_NORMAL` it reads only what a client sends
and refuses the rest of the factory table before the read;
`DE_FUZZ_ANY_ID=1` admits every registered id there instead, so the GC,
GS and SG reads behind that gate are fuzzed too, and
`DE_FUZZ_NO_STORE_SKIP=1` reads the two store-info packets the gameserver
refuses unread (with the gate open, since the validator refuses them
first).

The zig suite builds the targets as `fuzz_replay_game` and
`fuzz_replay_login`, a plain `main()` over saved inputs, and ctest runs
them over a seed corpus that `tools/fuzz/golden2corpus.pl` writes from
`tests/golden/` into the build tree, plus `tests/fuzz/regressions/`
(`.hex` files; every input that ever found a crash goes there with its
fix). `make dev-test` runs them like any other test.

libFuzzer needs a Clang with compiler-rt, which `zig c++` is not, so the
fuzzers are a separate configure with `-DDARKEDEN_BUILD_FUZZERS=ON`
(it refuses zig, AppleClang and a Clang that cannot link a
`-fsanitize=fuzzer` target, and instruments every target with ASan,
UBSan and `_GLIBCXX_ASSERTIONS`). `tools/fuzz/Dockerfile.fuzz` is the
toolchain:

```bash
docker build -f tools/fuzz/Dockerfile.fuzz -t darkeden-fuzz tools/fuzz
docker run --rm -it -v "$PWD:/repo:ro" darkeden-fuzz bash
# in the container: build off a copy of the inputs, never the mount
mkdir /src && cp -a /repo/CMakeLists.txt /repo/cmake /repo/src /repo/tests \
    /repo/third_party /repo/tools /src/
cmake -G Ninja -S /src -B /build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 \
    -DDARKEDEN_BUILD_FUZZERS=ON -DDARKEDEN_OUTPUT_ROOT=/out
cmake --build /build --target fuzz_game_stream fuzz_login_stream \
    fuzz_replay_game fuzz_replay_login fuzz_seed_corpus
# run from a scratch directory: Assert appends to assertion_failed.log there
mkdir -p /scratch /corpus && cd /scratch
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 /out/bin/fuzz_game_stream \
    -max_total_time=300 -timeout=10 -rss_limit_mb=2048 -max_len=32769 \
    -close_fd_mask=3 /corpus /build/tests/fuzz/corpus/game /src/tests/fuzz/regressions/game
```

`-close_fd_mask=3` keeps the packets' debug output off the terminal;
`-fork=N -ignore_crashes=1` keeps going past the first crash; with
`DE_FUZZ_ANY_ID=1` in the environment the run fuzzes every read in the
gameserver's table. Leak detection is off because six GC reads,
`GCUpdateInfo`'s among them, leak the records they allocate (recorded in
`docs/FIXES.md`). The client link refuses all six, so only an all-ids
run reaches them, but the documented runs keep it off either way.
Replay a crash file with `fuzz_replay_game <file>` in either build; in
the fuzz build it comes with the sanitizer's report and needs the same
`ASAN_OPTIONS=detect_leaks=0`, which ctest sets itself there
(`ctest -R fuzz` in `/build` runs the replay tests). Some recorded inputs
fail only under ASan: the zig Debug build does not trap on every
out-of-bounds read (`CGConnect-name-table.hex` is one), so their fixes
are guarded in the zig suite by the gtests in
`tests/packet_read_bounds_test.cpp`. There is no CI fuzz job yet.

#### Building in the container

**Do not compile straight off the bind-mounted checkout.** Measured inside
the container, the Windows mount costs ~160x on `stat` and ~145x on reads
versus the container's own filesystem, and since every translation unit opens
dozens of headers the build becomes I/O bound: a full build took ~20 minutes
at ~20% CPU on 8 cores. `tools/devbuild.sh` syncs the build *inputs*
(`cmake/`, `src/`, `tests/`, `third_party/`, `data/`, `initdb/`, `tools/`,
`docker/start.sh` and the top-level CMakeLists/Makefile) into a container
volume, builds there with Ninja and ccache, and copies only generated test
data back (`tests/golden/`, `tests/generated/`, `tests/wire-layout.txt`, and,
after `--record` only, de-core's parity vectors in `src/domain/vectors/`).
Same build: **~3.5 minutes at ~95% CPU**, and a no-op rebuild in seconds.

```bash
make dev-test                          # build wire_tests + ctest
bash tools/devbuild.sh test --record   # re-record goldens and vectors, then run
make dev-build                         # all production targets
make dev-shell                         # shell in the workspace
make dev-clean                         # drop the workspace + cache volumes
```

Needs the image once: `docker build -f Dockerfile.dev -t darkeden-dev .`.
It pins Zig 0.16.0 (Clang 21.1.0) and carries Ninja, ccache and rsync.
Artifacts live in compiler/target/build-type-specific directories in the
volume, so `bin/` and `lib/` in the checkout are **not** updated by these
targets; `bash tools/devbuild.sh output-dir` prints the artifact root. Two
concurrent checkouts (git worktrees) sharing one workspace volume race the
script's `rsync --delete`: give each its own `DEVBUILD_WORK_VOLUME` (the
ccache and Zig cache volumes are safe to share). `DEVBUILD_JOBS=N` caps the
build at N jobs and pins the container to N CPUs.

## Project Architecture

### Server Architecture

Three coordinated processes:

1. **loginserver** - Handles authentication and character selection
2. **sharedserver** - Manages shared data (e.g., guild info) across game servers
3. **gameserver** - The main game logic server (one per world/zone group)

CMake is the build system; the `Makefile`s in `src/` subdirectories are
legacy and superseded. No target globs its sources: every `.cpp` is named,
which is what lets ratchet R15 call a source no target compiles dead.

`GameServerRuntime`, `LoginServerRuntime` and `SharedServerRuntime` are
OBJECT libraries. Each executable compiles only `main.cpp` and links its
runtime; the corresponding runtime tests link those same objects without
running process startup. Link a runtime directly with
`target_link_libraries` to inherit its server definitions, headers and
dependencies. Keep different runtimes in separate executables: several
classes share names across servers, and packet registration is process-wide.
The existing isolated rule tests remain the faster choice for pure logic.

`ServerStartup` depends only on `de-kernel`. It parses arguments and loads an
owned configuration, including loginserver's optional ID offset, without
publishing it. `server_startup_tests` exercises this without linking a server
runtime.

`Properties::load` opens the file and delegates to `de::readProperties` in
`PropertiesParser`. The parser accepts a borrowed stream and merges into a
destination `Properties`, so grammar and read failures are tested in memory.
It processes the last line without requiring a newline and rejects empty keys
and failed reads; the file adapter explicitly rejects directories. Earlier
entries remain in the destination after a parse error, so startup still loads
an unpublished object. `properties_parser_tests` owns this contract, including
EOF under stream exception masks and the existing whitespace/comment rules.

`ServerLifecycle` also depends only on `de-kernel`. `runServerLifecycle` takes
construction/initialization, start and stop actions, handles exceptions,
requests shutdown before cleanup and returns the drain result and exit status.
It uses the workers' `ServerShutdown` flags and never resets them. The caller
keeps the configuration and server graph alive.
`server_lifecycle_tests` runs the shared control flow with controlled actions
and isolated diagnostics, without starting a server or a deadline thread.

The entry points link `ServerApplication`, which composes these two libraries.
It owns the completed configuration, publishes it to an explicit
`KernelContext`, runs the supplied lifecycle actions, reports the drain result
and flushes diagnostics. A configuration error returns an empty result before
any action runs. The object permits one run and retains its configuration
after the result, including a failed drain; destruction restores the previous
context binding before freeing it. Each `main()` keeps it alive through
`_Exit` once lifecycle execution has begun, preserving the legacy server
graph's configuration even if workers could not stop. Tests use a local
context and controlled actions; `server_application_tests` covers publication,
lifetime, failure paths, effective login overrides and final reporting without
linking a runtime or database.

`ServerProcessShutdown` owns the SIGTERM/SIGINT handlers and the deadline.
Each `main()` constructs it before initialization and checks `ready()` before
proceeding. The default deadline is still 30 seconds after a shutdown request.
On scope exit it cancels and joins the watcher, then restores the previous
signal handlers; it never clears shutdown state. The library depends only on
`Threads::Threads`. `server_process_shutdown_tests` delivers real signals to
main and worker threads and checks blocked lifecycle paths in subprocesses.

`ServerFatalHandlers` installs the allocation handler for each server and the
gameserver's terminate handler, restoring previous handlers on scope exit.
It has no runtime or kernel dependency; the common `ServerKind` enum lives
in its own header. Fatal diagnostics use fixed text and POSIX writes, so
they cannot re-enter the allocation handler through C++ heap allocation.
Gameserver still appends `CriticalError.log` and aborts; its emergency records
are plain message lines without timestamp formatting. Login/shared report
allocation failure to stderr and `_Exit(EXIT_FAILURE)`, preserving their
existing terminate handlers. `server_fatal_handler_tests` injects allocation
failure on main and worker threads in child processes with core dumps disabled.

`ServerProcessEnvironment` holds the other process setup without a runtime or
kernel dependency. All three entry points call `raiseCoreDumpLimit` before
server construction, raising the soft limit to the inherited hard limit; the
returned syscall error remains nonfatal for startup. Gameserver seeds rand in
its main and reseeds at the start of `GameServer::init`, at the same points as
before. `initializeGameProcess` also ignores SIGPIPE, SIGALRM and SIGCHLD for
the rest of the process, leaving the shutdown handlers and thread masks alone.
Explicit seeds make this setup deterministic in tests. Unlike the scoped
shutdown/fatal guards, these process settings are not restored;
`server_process_environment_tests` changes them only in subprocesses.

`ServerWorkerShutdown` provides `stopServerWorkers` for all three servers'
auxiliary workers. After stopping the foreground loop, each server supplies
borrowed workers and its error stream. The helper requests every stop, runs an
optional action (gameserver drains its zone pool here), then joins and reports
retained run failures in order. An explicit diagnostic name preserves the
login/shared `GameServerManager` label. Stop, extra-drain and join exceptions
still propagate to the lifecycle's failed-drain handling; worker run failures
are reported while remaining joins continue. Ownership, the server's stop
idempotence and the process deadline stay with their existing callers.
`server_worker_shutdown_tests` uses real cooperative workers, including partial
startup and injected failures, without linking a runtime or database.

### Key Directory Structure

```
src/
├── Core/                      # de-kernel: packets + shared utilities, no server-type dependencies
│   ├── [GC|CG|CL|LC|GL|LG|GS|SG|GG]*.{h,cpp}   # Protocol packet classes, directly in Core/
│   ├── [core utilities]       # Socket, datagram, player info, items, skills, etc.
│   └── CMakeLists.txt         # de-kernel, Core, and the per-server packet libraries
├── domain/                    # de-core: pure formula functions (Formulas, SkillOutputFormulas, SkillRange, ItemPrice, ItemDurability, ItemGrade, ItemClass, EquipRequirement, and the server-only PartyExp and DarkLight), freestanding; vectors/ holds the parity rows the client asserts too, vectors/server/ the rows of the server-only ones
├── server/
│   ├── Thread.h, ManagedThread.h  # the worker-thread base (CooperativeThread.h is reached only through ManagedThread)
│   ├── Mailbox.h, Snapshot.h  # cross-thread command queue, copy-on-write tables
│   ├── ServerContext.h        # registry for the managers ServerCore defines (database, server and world tables)
│   ├── database/              # Database abstraction layer and connection management
│   ├── repository/            # ServerCore persistence seams, linked into all three binaries
│   ├── gameserver/            # Main game server executable
│   │   ├── GamePacketDispatch.cpp   # composition root: packet id -> handler
│   │   ├── handler/           # CG/GC/LG/GG/SG packet handler bodies
│   │   ├── packetfill/        # Server-side packet fill helpers moved out of Core
│   │   ├── repository/        # Persistence seams: *Repository.h interfaces + MySQL*Repository.cpp impls
│   │   ├── gm/                # GM and console command bodies behind a router
│   │   ├── skill/, item/, war/, quest/   # Skill, item, war and (Lua-scripted) quest modules
│   │   ├── guild/, party/, trade/, couple/, exchange/   # Social and trading decisions
│   │   ├── ctf/, mission/, mofus/        # Capture the flag, missions, game events
│   ├── loginserver/           # Login server executable (CL/GL/GM handlers and one CG, its own repository/)
│   └── sharedserver/          # Shared server executable (GS handlers, its own repository/)
tests/
├── arch/                      # kernel_files.txt (de-kernel's source list), check_includes.pl, baseline.txt
├── ratchet/                   # ratchets.sh and the files it pins
├── tools/                     # generators and audits ctest runs
├── fuzz/                      # packet-read fuzz targets, replay driver, regressions/
└── golden/, generated/, support/, integration/  # fixtures, generated lists, fakes, MySQL tier
docs/
├── RESTRUCTURING.md           # the living plan: tasks, statuses, the R1-R18 table
├── FIXES.md                   # the fix log
└── TOOLCHAIN.md               # Zig/Clang, C++20 adoption, wire and diagnostics contracts
third_party/
├── tinyxml2/                  # Vendored XML parser (10.0.0), replaces xerces-c
└── argon2/                    # Vendored argon2 reference implementation (C API)
.claude/skills/add-packet/     # checklist for adding or changing a packet
```

### Packet System

Packets are the primary communication mechanism between servers and clients.
Each is named for the link it rides, source letter first: **CG**/**GC**
between client and gameserver, **CL**/**LC** between client and loginserver,
**GL**/**LG** between game and login servers, **GS**/**SG** between game and
shared servers, **GG** between gameservers, and **GM** for the server-info
datagram a gameserver sends the loginserver.

Each packet lives in two layers:
- `src/Core/PacketName.{h,cpp}` — the packet class (wire layout only; no
  `execute()`), its `PacketNameFactory`, and the declaration of its handler.
- `src/server/<server>/handler/PacketNameHandler.cpp` — the handler body.
  There is no handler header: the class is declared beside the packet.

The handler is bound to its packet id at the server's composition root —
`GamePacketDispatch.cpp`, `LoginPacketDispatch.cpp`,
`SharedPacketDispatch.cpp` — with `DE_REGISTER_PACKET_HANDLER(Name)`. Each
root also declares the links it accepts as a `de::packet::DirectionSet`
(`kReceivedDirections`), so registering a handler for a packet on a link the
server does not receive is a compile error. A link can be narrowed to some
of its packets: the gameserver's GC link admits only the GC-named packets
its client link admits (`src/Core/GameClientLink.h`).

Every `XFactory` states its packet's id, name and maximum body size as
`static constexpr kPacketID` / `kName` / `kMaxSize`; the virtual getters
return them. `src/Core/PacketMeta.h` names that contract
(`de::PacketFactoryType`) and folds a pack of factories into a `constexpr`
table (`de::packet::FactoryList`) that rejects duplicate or out-of-range ids
while compiling. `PacketFactoryManager::init()` selects per server from four
such lists — edit the lists, not an `addFactory` sequence: the gameserver
`Concat`s three, the loginserver two, the sharedserver uses one as it is.
Each server creates its own factory table and validator and registers them
on `de::kernelContext()` (`src/Core/KernelContext.h`, a kernel file so the
kernel's own reader can use it); readers call
`de::kernelContext().packetFactories()` and `.packetValidator()`. The
validator's in-game set on the gameserver is folded from the same lists:
every CG packet it registers, bar the datagram-only `CGPortCheck`, plus
the GC-named packets the live client sends, listed in
`src/Core/GameClientLink.h`; a new client-sent GC packet needs its id
there, or its handler registration does not compile.
`tests/packet_meta_test.cpp` compiles the whole kernel into one list. A new
packet needs the three constants in its factory or it will not satisfy the
concept. See `docs/TOOLCHAIN.md` §3 and `.claude/skills/add-packet`.

### Preprocessor Macros

Compile definitions that control behavior, set per target in the
CMakeLists.txt files — plus `LoginServer.h` and `SharedServer.h`, which
define their own server macro so that including the header makes the
translation unit that server's: `__GAME_SERVER__` /
`__LOGIN_SERVER__` / `__SHARED_SERVER__` say which server a translation unit
is compiled for, `__COMBAT__` enables combat code (the gameserver targets
define it), `__LINUX__` comes from the top-level `CMakeLists.txt`.

A macro no build defines makes the block behind it dead text, so R14 holds
the count of the ones that were removed at zero; do not reintroduce one.

### Configuration

`conf/` holds `gameserver.conf`, `loginserver.conf` and `sharedserver.conf`.
The settings that matter most: `HomePath` (the repository directory, which
must be set correctly), `DB_HOST` (database address) and `LoginServerIP`.
The `WorldDBInfo` and `GameServerInfo` database tables must agree with these
files. Each connection is one block of keys (`DB_*` for the game database,
`UI_DB_*` for the account database, which the gameserver's second connection
reads too); there is no `DIST_DB_*` block. The Exchange's point tables
(`AccountPoint`, `PointLedger`) are reached from the game connection by the
account schema's name (`UI_DB_DB`), so a purchase is one transaction on one
connection: for the Exchange both blocks must name one MySQL server, and
`DB_USER` must be able to read and write those two tables. The gameserver
checks that at startup (`ExchangeService::openPointLedger`); when it fails
it starts anyway with Exchange purchases refused, and says why on stderr and
in `DBError.log`. Code reads the loaded configuration through
`de::kernelContext().config()`, which each `main()` registers right after
loading it and which asserts on a configuration nobody registered: a read
before that point is a startup-order bug, not a condition to branch on.

## Database Setup

The project requires MySQL 5.7 or 8 with specific SQL mode settings:

```sql
-- Remove NO_ZERO_DATE and STRICT_TRANS_TABLES from sql_mode
set @@global.sql_mode = 'ONLY_FULL_GROUP_BY,NO_ZERO_IN_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';
```

Text is UTF-8 end to end. Both databases and every table are InnoDB in
`utf8mb4` / `utf8mb4_unicode_ci` (`tests/ratchet/ratchets.sh` fails on any
other engine or charset in `initdb/`), the MySQL containers run
`--character-set-server=utf8mb4 --collation-server=utf8mb4_unicode_ci`, and
every server pins its session to `utf8mb4` right after connecting
(`src/server/database/Connection.cpp`). The pin matters: the 8.0 client
library asks MySQL 5.7 for a collation it does not know, and the server then
silently drops the session to latin1 and passes text through as raw bytes.
The dumps' legacy EUC-KR and GBK text was re-encoded to real UTF-8 with
`tools/reencode_legacy_dump.pl`, which documents how each literal's language
was decided.

All SQL lives behind repository seams: an interface `*Repository.h` with a
`MySQL*Repository.cpp` implementation under a `repository/` directory,
reached through a `default*Repository()` accessor, never a `g_p*` extern.
The same rule covers every process-wide manager: ratchet R1 holds the
`g_p*` extern count at zero, and a manager is taken from the context of its
layer — `de::kernelContext()` (`src/Core`), `de::serverContext()`
(ServerCore: the database manager, the server and world tables),
`de::gameContext()`, `de::loginContext()` or `de::sharedContext()` — whose
accessor asserts it was registered by its creator.
There are 44 — 36 under `src/server/gameserver/repository/`, two in
ServerCore (`src/server/repository/`, compiled into all three binaries),
four in the loginserver and two in the sharedserver, the last two sets
prefixed `Login`/`Shared` because the integration binary links every impl.
The header is the authority on its tables' quirks and, where SQL on the
same tables lives outside the seam, carries an explicit `Not enclosed`
list of it (nine headers do today; grep for the phrase) — read it before
adding a method. Statements move byte-for-byte, quirks included, and use
the parameterized `executeQuery` form with each argument's own conversion
(`%u` for DWORD/WORD/BYTE, `%d` for int, `%ld` for `time_t`). Where two
races share a row shape the record is shared and the table is chosen in
the seam: `CharacterExpsRecord` with
`saveExps(ownerName, race, record)`, over `characterRaceTable()`
(`repository/CharacterRace.h`), which `characterRaceOf()`
(`gameserver/PlayerRace.h`) maps the game's `Race_t` onto. R2/R3 fail on
an `executeQuery` written anywhere else.

Databases:
- `DARKEDEN` - Main game database
- `USERINFO` - User account database

Account passwords live in `Player.Password` as argon2id hashes. A database
created before that change needs `initdb/migrations/001-argon2-password-column.sql`
run once; its plaintext rows are rehashed by the loginserver on each
account's next login. `bin/hashpw` hashes a password for a manual `UPDATE`.
The later migrations are one-time too: `002-war-schedule-castle-war-kind.sql`
(the `CastleWarKind` column) and `003-castle-tax-balance-signed.sql` (the
castle tax balance saved as a signed relative change; on the old unsigned
column an owner change can fail whole). Nothing checks them at startup; run
each once on a database created before it.

Load schema with (`initdb/a-setup.sql` creates both databases and the
`elcastle` user; the docker compose setup applies all three automatically):
```bash
mysql -h 127.0.0.1 -u root -p < initdb/a-setup.sql
mysql -h 127.0.0.1 -u elcastle -D 'DARKEDEN' -p < initdb/DARKEDEN.sql
mysql -h 127.0.0.1 -u elcastle -D 'USERINFO' -p < initdb/USERINFO.sql
```

## Dependencies

Required libraries (all expose a C API — see `docs/TOOLCHAIN.md` for why
re-introducing a C++-API dependency is a problem under the Zig toolchain):
- **libmysqlclient-dev** - MySQL client library (Ubuntu 20.04 ships 8.0 /
  `libmysqlclient21`; the server talks to MySQL 5.7 or 8)
- **lua5.1-dev** or **luajit** - Lua scripting (used by quest system)
- **zlib**

XML parsing uses the vendored **tinyxml2** (10.0.0) in `third_party/tinyxml2`,
wrapped by `SXml` in Core; xerces-c is no longer needed (`docs/TOOLCHAIN.md` §1).
Password hashing uses the vendored **argon2** reference implementation
(20190702, C API) in `third_party/argon2`; nothing to install for it.

Install on Ubuntu/Debian:
```bash
sudo apt install libmysqlclient-dev liblua5.1-dev zlib1g-dev
```

## Key Game Concepts

- **Races** — **Slayer** (human vampire hunters), **Vampire**, **Ousters**,
  one `PlayerCreature` subclass each
- **Zone/ZoneGroup** - Geographic areas where players exist; **DynamicZone**
  is instanced content (e.g. dungeons)
- **Creature** - Base class for all entities (players, monsters, NPCs)
- **Effect** - Time-based effects applied to creatures
- **Skill** - Combat and utility abilities
- **Guild/Party** - Social grouping systems

## Thread ownership

The gameserver's threading contract, as the code actually implements it.

### Threads in the gameserver process

- **Main thread** — runs `GameServer::init()` (single-threaded startup:
  config, DB, zone loading), then `GameServer::start()`, which spawns the
  threads below and finally becomes `ClientManager::run()`, an infinite
  loop accepting client TCP connections and driving the pre-zone
  login/handshake phase before a player is handed to a zone group.
- **`ZoneGroupThread` (one per `ZoneGroup`,** via
  `ThreadManager`/`ThreadPool`) — the owner of all zone-group state. Its
  loop is: lock the group's mutex → `ZoneGroup::processPlayers()` (socket
  `select`, read, parse, `PacketDispatcher::dispatch` of **CG** packets)
  and `ZoneGroup::heartbeat()` (NPC/monster AI, effects, zone systems) →
  unlock. So all CG handler code runs on the zone thread **with the group
  mutex held**. Each zone thread registers its own DB `Connection` keyed
  by thread id (`de::serverContext().database().addConnection(Thread::self(), …)`) — DB
  connections are thread-local by convention, never shared. The
  manager's tables of them are not: a worker registers while others look
  theirs up, so lookups take the tables' `std::shared_mutex` shared and
  registrations exclusive, a leaf held for the map access alone.
- **`LoginServerManager` thread** — UDP datagram link to the loginserver;
  dispatches **LG** and **GG** packets on its own thread under its own
  `m_Mutex`.
- **`SharedServerManager` thread** — TCP link to the sharedserver;
  dispatches **SG** packets on its own thread under its own `m_Mutex`.
- **`MPlayerManager` (mofus), `GDRLairManager`** — auxiliary threads with
  their own loops, stopped and joined with the managers above.
  `MPlayerManager` is a `GameServer` member; `GDRLairManager` is still a
  singleton (`GDRLairManager::Instance()`). The mofus pair sits behind
  `#ifdef __MOFUS__`, which `mofus/Mofus.h` defines when `__METRO_SERVER__`
  is, and `src/Core/types/ServerType.h` defines that unconditionally for
  everything that includes `Types.h`, so the module is always built and
  neither guard is the switch it looks like; `__METRO_SERVER__` also gates
  `Encrypter.h`, so it is not a knob to turn.
  (`SMSServiceThread` is a `ManagedThread` too, but
  `GameServer::start()` never starts it; its queue is only filled by
  `CGSMSSendHandler`. Neither billing integration is in the tree: the
  external billing link had a thread of its own, and the China one its own
  tree.)

The loginserver and sharedserver follow the same contract. The loginserver's
main thread runs `ClientManager::run()` and its `GameServerManager` worker
owns the UDP link to the game servers; the sharedserver's main thread runs
`HeartbeatManager::run()` and its `GameServerManager` worker owns the TCP
listener plus `GuildManager::heartbeat()`. Each worker registers its own DB
`Connection` keyed by `Thread::self()` where it needs one.

Every worker in all three processes uses `ManagedThread` (`std::jthread`);
it is the only remaining subclass of the legacy `Thread`. Start and
stop are serialized; stop-before-start is terminal, and join allows a
concurrent stop request. Derived destructors must stop/join before destroying
members. SIGTERM/SIGINT request process shutdown; main exits its client loop,
requests every worker to stop, and joins them while dependencies remain alive.
The process then uses `_Exit` to reclaim the legacy singleton graph without
running its unaudited destructors. This does not add a world-save operation.
A 30-second watchdog (`ServerShutdown::Deadline`) forces a nonzero exit if
startup, I/O or a heartbeat prevents shutdown. See `docs/TOOLCHAIN.md` for
the full contract.

### The mutation rule

**Zone-group state (Zones, Creatures in them, the group's
`ZonePlayerManager`) may only be touched while that group's mutex is
held.** The group's own `ZoneGroupThread` holds it for its entire tick;
any other thread must take it explicitly, e.g.
`__ENTER_CRITICAL_SECTION((*(pZone->getZoneGroup())))` — `GDRLairManager`
does this at every site that touches zone-group state. Its
`addEffect_LOCKING`/`deleteEffect_LOCKING` calls need no group mutex: they
go to the zone's separate locked effect manager, which the tick services
only under `Zone::m_MutexEffect`, the one sub-lock designed for cross-thread
adds. Note `Zone::m_Mutex` is a **different, narrower** lock some
main-thread heartbeats take (war/ctf via `pZone->lock()`); holding it does
NOT exclude the zone-group tick and does not satisfy this rule.

`__ENTER_CRITICAL_SECTION` / `__LEAVE_CRITICAL_SECTION` delimit a **block**
owned by a scoped `CriticalSection` guard (`src/Core/Exception.h`), so the lock
is released on every exit — end of block, `return`, `goto`/`continue` out of it,
and any thrown type. Consequently a hand-written `x.unlock()` inside a section
is a **double unlock** of a non-recursive mutex: to run work unlocked, use
`__CRITICAL_SECTION_LOCK.unlock()` / `.lock()`, the guard's own name.
`tests/tools/critical_section_audit.pl` fails on a hand-written one; it runs
in ctest as `critical_section_audit`, alongside `ratchets` and `arch_includes`.

This is mutex-guarded ownership, not pure thread-affinity: the guarded
region is the contract. Under `DE_OWNERSHIP_CHECKS` — defined only for
Debug builds; this project deliberately never defines `NDEBUG`, so the
checks ride their own macro and every optimized build compiles them away
completely — `ZoneGroup::lock()` records the holding thread
(`pthread_equal` + a valid flag, never a raw `==` or zero-tid sentinel)
and `ZoneGroup::assertOwned()` **calls `abort()`** on a violation. It
must not throw: an `AssertionError` is a `Throwable`, and the
`catch (Throwable&)` blocks sitting on these very paths would swallow it,
turning a detected race into a silently half-applied mutation. The check
is armed by `ZoneGroupThread::run()`, so single-threaded startup/loading
is exempt. `ZoneGroup::drainMailbox` asserts too; on `Zone` the coverage
is exactly the eight gateways — `addPC` (both
overloads), `replacePC`, `addCreature`, `deleteCreature`, `moveCreature`,
and the tile-only pair `addCreatureToTile`/`deleteCreatureFromTile` that
the move-mode swaps and corpse paths use — so every creature write to a
`Tile` outside `ZoneSpawn.cpp` and `ZoneMove.cpp`, where the gateways live,
is gated. `Zone::movePC`/`deletePC`/`pushPC`/`addItem`/`deleteItem` are
**not**: the assert is a tripwire on the main gateways, not a guarantee.

### Cross-thread communication

- Cross-thread packet handlers (SG/LG/GG, on the manager threads) reach
  player creatures through the player-creature finder
  (`de::gameContext().playerCreatures()`) under **its** critical section
  (`getCreature_LOCKED`), then use `pPlayer->sendPacket(...)` — sending
  to a player's socket is the main legitimate cross-thread operation.
- Anything beyond sending — gold, guild id, kick flags, a zone broadcast —
  goes through the player's **mailbox** (`src/server/Mailbox.h`, owned by
  `GamePlayer`): `de::postToPlayer(name, command[, ifGone[, scope]])`
  (`src/server/gameserver/PlayerMailbox.h`) finds the player under the
  PCFinder lock and queues the command; the manager that owns the player
  drains it from the one place it processes its players, each tick, before
  the player's own packets. The box belongs to the player rather than to a
  group because a player's owner changes over its life (the main thread's
  `IncomingPlayerManager` during login and zone transfer, a
  `ZonePlayerManager` on a zone thread in between) and the creature's zone
  pointer does not say which — it is set as soon as the character loads and
  stays on the old zone during a transfer, so the box follows the player and
  keeps posting order across group changes. The zone manager, under the group
  mutex, runs everything (for a player whose creature is in another group's
  zone — the listing mismatch it already logs as ZPMCheck — only the
  player-scoped ones); the main thread runs only `Scope::Player` commands
  (the kick flags), because the zone the creature points at is ticking
  elsewhere, so `Scope::Zone` commands wait in order for a zone thread. A
  player who logs out with commands pending runs their `ifGone` handlers
  instead (`~GamePlayer`, right after the PCFinder removal, so exactly one
  of command/`ifGone` runs), for the handlers whose offline branch matters,
  like charging a guild fee in the database. Commands capture by value only
  (a `Guild*`/`GuildMember*` may be deleted before they run) and run without
  the PCFinder lock, so they may post to other players. `ZoneGroup` has a box
  of its own (`ZoneGroup::post()`, drained at the top of the tick under the
  group mutex) for group-level work from other threads; its producer is
  dynamic zone recycling, handing an instance's `init()` to its owner.
- Tables that every thread reads and one thread occasionally extends — a
  group's zone map, the `ZoneInfoManager` lookups — are published
  copy-on-write through `de::Snapshot` (`src/server/Snapshot.h`): readers
  load an immutable `shared_ptr<const T>` without waiting on a writer and
  may iterate it for a whole tick; a writer copies, changes and swaps the
  pointer under a leaf mutex held for nothing else. That is what lets a
  zone thread create a dynamic zone in another group's map while that
  group iterates it. The writer mutex is held while the change runs, so a
  change must be pure work on the copy (the current ones are map inserts
  and erases); the snapshot protects the table, not the objects it points
  to, which stay raw pointers with their old lifetime rules.
- Players enter a zone group through the `ZonePlayerManager` under its
  lock; the zone thread integrates them on its next tick.

### Known violations (each closed, and the rules they left behind)

- **SG/LG/GG handlers must not mutate creature state under the `PCFinder`
  lock alone.** The six guild handlers and `LGKickCharacter` post their
  gold / guild-id / kick-flag / zone-broadcast work through
  `de::postToPlayer`. `GuildManager` and `Guild` lock their maps on both
  sides: `getMembers_NOLOCKED()` is the writer thread's, readers copy the
  member names under the guild mutex, and deleting a guild empties its map
  through `retireAllMembers()`. The per-member flags and counters and the
  guild's own integral fields are atomics, the rank change and its counter
  update going under the mutex together; its strings are copied in and out
  under `Guild::m_Mutex`, a non-recursive leaf the string accessors take and
  nothing else, so code already holding it touches the members directly. Lifetime is handled by not freeing: `getGuild()` and
  `getMember()` return raw pointers after releasing their locks, so a
  deleted guild or member is *retired* (`GuildManager::m_RetiredGuilds`,
  `Guild::m_RetiredMembers`; the whole-table `clear()` a sharedserver resync
  triggers retires too, never frees) and stays readable, stale, until the managers
  are destroyed. A retired object says so: an atomic flag set where the
  retirement happens, under the lock that already covers it, and the answers
  a stale pointer gives enforce it, so a reader that decides on a rank or a
  state needs no check of its own; the readers that decide membership alone
  test `isRetired()`.
  `GuildMember::getRank()` reads `GUILDMEMBER_RANK_LEAVE` once retired,
  `Guild::getState()` reads `GUILD_STATE_BROKEN` and `Guild::getMember()`
  answers NULL for every name, while the guild's own counters, teardown list
  and database rows take `getStoredRank()` / `m_State` and still carry what
  the member and the guild had. `GuildUnionManager` has the same shape: its
  tables are a `GuildUnionRegistry` locked on both sides, a union it takes
  away is retired and names no member guild, and the lock order is in
  `GuildUnion.h`. A union exists for its member guilds and its pending join
  offers, and an offer lives ten days: every path that takes a member or an
  offer away applies that one rule (`dissolveIfAbandoned_LOCKED`), and the
  expired offers are purged before an offer is read to be acted on and at
  every load (`purgeOffers`, stated in `GuildUnion.h`).
- **No creature is written to a `Tile` outside `ZoneSpawn.cpp` and
  `ZoneMove.cpp`.** PC swaps go through `Zone::replacePC`; the move-mode
  swaps, the knockback and NPC-warp moves, and the corpse paths that take a
  dead creature off the map while its manager keeps it go through
  `Zone::addCreatureToTile` / `Zone::deleteCreatureFromTile`, all gated.
- **A zone map may be extended from another group's thread**, because the
  group's zone map and the `ZoneInfoManager` tables are `de::Snapshot`s: the
  iterating heartbeat and every concurrent `getZone()` keep the map they
  loaded. A recycled instance's `init()` is posted to the owning group
  (`ZoneGroup::post()`), `DynamicZoneGroup` serialises selection/creation
  under its own mutex, and the instance status flag is atomic. `addZone` is
  not a gateway, so the assert does not see this path.
- **`GDRLairManager` takes the owning group's mutex at every site** that
  walks its PCManager, sweeps effects or writes inventory and registries.
  None of those sites hits a gated gateway.
- **A war changes no zone from the thread that starts or ends it.** The war
  heartbeat (main thread), a castle's scheduler (the castle zone's thread),
  a GM starting the race war and the flag war (`FlagManager`'s schedule, on
  the main thread) post every zone change to the owning group
  through `war/WarZoneWork.h`: `postToZone`/`postToZones` (one command per
  group, the zone looked up by id when it runs), `postToEveryZoneGroup`, and
  `postItemReturn` for a castle symbol, blood bible, dragon eye or flag, which
  reads the item's row and posts the step that takes it out to the zone or
  player (`de::postToPlayer`) holding it; that step hands the item on
  through `Zone::transportItemToCorpse`, or destroys a flag. The flag war's
  drops, pole sweeps, status broadcasts and newbie-war summons are
  `postToZone`/`postToZones` commands, its values in `ctf/FlagWarPlan.h`. The castle owner change goes
  through `CastleInfoManager::postCastleWarEnd`. A posted war command asserts
  its group, and `Zone::killAllMonsters`, the shrine shields and the siege
  set-up and reset assert it too. Reading a zone's player list is zone-group
  state as well, so a broadcast to another group's zones is posted:
  `postBroadcast` captures the packet's body on the calling thread
  (`war/CapturedPacket.h`; each player's stream frames it with its own
  sequence and encryption) and each group's command sends it. The holy
  land's, a castle's and a level war's bonus-zone broadcasts
  (`HolyLandManager::broadcast`, `CastleInfo::broadcast`,
  `LevelWarZoneInfoManager::broadcast`) go this way from any thread;
  `ZoneGroupManager::broadcast` stays direct, walking each group's players
  under its `ZonePlayerManager` mutex and only sending.
- **Packets pipelined behind `CGReady` are drained by the zone thread.**
  `GamePlayer::processCommand` stops the main-thread
  (`IncomingPlayerManager`) drain once the status flips to `GPS_NORMAL`, so
  nothing reaches a gateway without the group mutex.

## Running the Servers

Start servers in this order:
```bash
./bin/loginserver -f ./conf/loginserver.conf
./bin/sharedserver -f ./conf/sharedserver.conf
./bin/gameserver -f ./conf/gameserver.conf
```

In the container image `docker/start.sh` does this (it uses `docker/conf/`
rather than `conf/`).

## Development Notes

- Source file encoding is **UTF-8** (the project was migrated from legacy
  encodings). R17 holds the source lines that carry a non-ASCII byte at
  zero: the legacy comments and string literals are all English now, and a
  literal that must keep foreign bytes because data is matched against it
  is written as escaped UTF-8 with an English comment beside it.
- Write comments in **English**, and describe behaviour: no task numbers,
  dates, review history or migration narrative in `src/` or `tests/`.
  Translate the legacy Korean or mojibake comments you come across.
- C++20 is the required language standard, verified with the pinned
  Zig/Clang container toolchain.
- Exceptions: `throw Error("text")`, never `throw "text"` — a bare literal
  throws a `const char*`, a type nothing in the tree catches, so it walks
  past every handler into a `catch (...)` backstop or `std::terminate`
  (R11/R10b hold both at zero). Gameplay results use
  `Outcome<Events, Rejection>` (`src/Core/Outcome.h`); exceptions are for
  programming and configuration errors.
- Deep inheritance is the house style (Creature → PlayerCreature →
  Slayer/Vampire/Ousters), and Lua drives the quest system
  (`quest/luaScript/`).
