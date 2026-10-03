# opendarkeden-server

Browser and native WebSocket clients can use the optional
[WebSocket gateway](docs/websocket.md), which preserves player IPs across
login and game handoffs while retaining the existing TCP listeners.

## Install using Docker

Everything below builds the server **from the sources in this repository** - no
pre-built image is downloaded.

## Quick start (docker compose)

```sh
cd docker
docker compose up -d --build
```

That command:

1. builds `../Dockerfile`, which compiles `loginserver`, `sharedserver` and
   `gameserver` as C++20 with pinned Zig 0.16.0/Clang 21.1.0, then packages
   the binaries in an Ubuntu 20.04 runtime together with `data/` and
   `docker/conf/`;
2. starts MySQL 5.7 and imports `initdb/*.sql` on first run;
3. applies `docker/initdb-docker.sql`, which points `DARKEDEN.WorldDBInfo` and
   `DARKEDEN.GameServerInfo` at this stack (the dumps ship with the original
   developers' LAN addresses);
4. starts the three servers in order once the database is ready
   (see `docker/start.sh`);
5. builds `src/server/websocketproxyserver` and starts the WebSocket gateway
   (`odk-websocket`) in the game servers' network namespace, published on
   `127.0.0.1:8080` for browser and native WebSocket clients
   (`docker/websocket.json`, see `docs/websocket.md`).

Follow the logs:

```sh
docker compose logs -f odk-server
```

Stop everything (the database keeps its data in the `odk-mysql-data` volume):

```sh
docker compose down
```

Add `-v` to `docker compose down` to wipe the database as well.

**macOS:** the same commands work under OrbStack (what this was verified
with) and should under Docker Desktop. On Apple Silicon the server images
build natively for arm64, but
`mysql/mysql-server:5.7` is published for amd64 only, so Docker runs it under
emulation and warns that `The requested image's platform (linux/amd64) does
not match the detected host platform`. That warning is expected, and the
stack comes up as it does on Linux.

**NOTE:** the compose setup assumes server and client run on the same machine.
To run the client on another machine, set the server IP in the
`DARKEDEN.GameServerInfo` table (or in `docker/initdb-docker.sql` before the
first start) and restart the server container.

### Rebuild after changing the code

```sh
cd docker
docker compose up -d --build
```

The image uses C++20 and defaults to `CMAKE_BUILD_TYPE=Release`. The compose
build arguments can select Debug:

```sh
BUILD_TYPE=Debug docker compose up -d --build
```

`docker compose down` requests gameserver shutdown first and keeps the
login/shared processes alive until its workers finish. Gameserver has a
30-second shutdown deadline; Compose allows 45 seconds before killing the
container. A deadline expiry is a failed, forced exit, not a completed drain.
This joins workers but does not introduce a full world-save operation.

### Start the servers by hand

Each executable accepts `-f <config file>`. Loginserver also accepts `-i ID`,
a signed decimal offset added to `LoginServerBasePort`,
`LoginServerBaseUDPPort` and `LoginServerBaseID`. Argument, file-loading and
override errors return a failure status before server initialization.

Set `command: ["sleep","infinity"]` on the `odk-server` service, then:

```sh
docker exec -w /home/darkeden/vs/bin -it odk-server /bin/bash
./start.sh
```

## Development builds and tests

`Dockerfile.dev` provides the same pinned Zig/Clang compiler as the production
builder. Build it once from the repository root:

```bash
docker build -f Dockerfile.dev -t darkeden-dev .
```

The development helper copies only build inputs into a Docker volume, avoiding
the cost of compiling directly from a Windows bind mount. Artifacts remain in
that volume rather than updating the checkout's `bin/` and `lib/` directories.

```bash
make dev-test
make dev-build

make dev-shell
```

The same commands work on macOS under OrbStack (verified) and should under
Docker Desktop; on Apple Silicon the image is arm64 and the build is native
to it.

The suite includes `game_server_runtime_tests`, `login_server_runtime_tests`
and `shared_server_runtime_tests`. They link the same server implementation
objects as the production executables, with a test entry point instead of
`main.cpp`, and exercise real packet handlers without starting a server or
connecting to MySQL. The first suite build therefore also compiles the server
runtimes. Smaller rule tests remain available as individual CMake targets.
The shared runtime also constructs and destroys the real server graph with an
owned listener, without database initialization. Construction faults, nested
context bindings, allocation retry and cleanup of accepted sockets are covered.
Its server/group catalogue loaders also accept explicit repository inputs.
The tests cover complete replacement, preservation after invalid data or
allocation failures, reload cleanup and ID bounds without MySQL or `main`.
Resurrection and shared-string loaders use the same boundary: tests exercise
paired race locations, exact text, complete reloads and preservation after
repository, validation, duplicate, allocation or diagnostic failure.
Shared guild loading is covered through an explicit repository too, including
complete guild/roster replacement, allocation rollback and retry, orphan-member
cleanup, field validation and the production guild-info reply built from loaded rows.
Guild startup also accepts explicit configuration and repository inputs. Tests
check ID arithmetic, empty-table query policy, and preservation of both counters
and borrowed rows after query, configuration, roster or allocation failure.
Guild replies prepare owned records before transferring a completed batch;
allocation and count-refusal tests check cleanup, preserved destination rows,
ordering and retry through the production builder and packet handoffs.
`world_catalogue_tests` links the common production world loader directly from
`ServerCore`, without a server runtime or database connection. It checks owned
replacement, rollback and retry after load failures, ID/status validation,
read-only lookups and preservation through throwing load diagnostics.
`server_catalogue_tests` similarly links the common game-server catalogue from
`ServerCore`. It checks owned rows/traversal tables, complete non-PK and castle
flags, cold and repeated cleanup, field validation, world zero and rollback/retry
after query, allocation or diagnostic failure without executable startup.
Login world-list replies use explicit catalogue, account repository and player
inputs. Tests run the production assembler without sockets or database startup,
covering sparse world IDs, ordered rows, saved-selection bounds, packet limits,
and cleanup/retry after lookup, allocation or send failures.
Login group and population catalogues also load from supplied repositories.
Tests cover scoped construction/destruction, checked owned replacement,
preservation of live counters and borrowed rows after failures, world-zero
cleanup, boundary worlds, allocation retry and throwing load diagnostics.
Login world/server selection borrows these managers explicitly. Runtime tests
exercise actual sparse membership, ascending group lists, missing-ID fallback,
empty-table refusals, reloads and live populations across the full stored ID
range. Decision tests retain the separate world-closed and group-down policies.
Server-list reply assembly also runs with supplied topology, account and player
inputs. Tests preserve both account-query paths, default/saved group selection,
packet limits and truncation, with allocation, lookup and send failure cleanup.
Login zone/group routing catalogues accept explicit repositories and replace
owned maps only after successful preparation. Runtime tests cover immutable
borrowed lookups, failed-load preservation, stale-row removal, duplicate and
allocation cleanup/retry, empty loads and existing ID widths/error translation.
Character selection uses an explicit adapter over the server and routing
catalogues. Runtime tests compose it with the production decision for all races,
sparse/boundary IDs, non-PK limits, quest-zone routing, missing references and
quiescent reloads, without publishing process contexts or starting a server.
Selection accepts only the creation system's canonical `SLOT1`–`SLOT3` text,
keeps account slot values at 1–3 and refuses malformed rows before routing.
Tests cover all suffix bytes, malformed prefixes and earlier rejection gates.
Character-list assembly takes explicit world, account and repository inputs and
returns an owned `LCPCList` for the handlers to send. Runtime tests cover every
race and slot, field/query mappings, reply independence and cleanup/retry after
validation, duplicate, repository, allocation and error-reporting failures.
The server-selection flow also runs with supplied topology and repository
inputs. Tests verify that normalized world/group IDs reach the session and
character query together, refusals preserve state, and character management
follows successful sending, including real serialization and failure/retry.
Kick preparation resolves saved or cached locations through explicit repositories,
validates IDs and account slots, and caches a complete character target before
datagram dispatch. Tests cover missing data, cached slots, refusal serialization,
exception identity and cache preservation/cleanup through allocation failures.
The saved kick target owns its account key and location separately from live
selection. Tests cover identity changes through the base player, stale verify
replies, atomic replacement, borrowed pointer stability and refusal cleanup.
Kick dispatch takes a supplied catalogue and sender, prepares owned destinations
for occupied groups before sending, and starts its wait after sender returns.
Runtime tests cover sparse/boundary groups, missing first-server rows, reloads,
send/allocation failures, deadline ordering and the production UDP path.
Datagram sending also runs with a supplied transport and diagnostic stream.
It validates canonical IPv4 endpoints, checks the full byte count, and reports
failure without hiding serialization errors or depending on executable startup.
Tests cover malformed endpoints, incomplete sends, throwing diagnostics,
allocation cleanup and safe disconnection after a failed kick send.
Kick verification takes a supplied player manager and completion action, admits
only waiting sessions with a matching account-owned target, and holds a scoped
manager lock through completion. Tests cover duplicate/late replies, identity
changes, descriptor boundaries and exception/allocation failure cleanup.
Kick retries take explicit time, a request result and a completion action. Fresh
attempts reset their count; refused, failed or superseded requests cannot advance
it. Tests pin the three-retry fallback, deadline boundaries, completion failures,
account/target changes, allocation cleanup and production refusal/disconnect.
Kick-login completion takes an account repository, statistics clock and
diagnostics explicitly. Both login paths share fully initialized success replies
and timestamped statistics writes. Runtime tests cover success/refusal ordering,
packet bytes, safe refusal disconnect, partial effects, broken diagnostics and
allocation cleanup without MySQL or `main`.

`server_startup_tests` covers argument parsing, configuration loading and the
login-server port/ID overrides without linking a server runtime.
`properties_parser_tests` checks configuration grammar, final lines without a
newline and read failures using in-memory streams.
`server_lifecycle_tests` exercises the shared initialization, start and stop
sequence, including partial startup, shutdown requests, exception reporting
and worker failure status. It also links without a server runtime or database.
Throwing formatters and rejected output cannot skip cleanup or hide its result;
process failure is marked before reporting, and startup log/console diagnostics
are attempted independently.
`server_process_shutdown_tests` checks the shared signal handlers and deadline,
including signals on worker threads and blocked startup/cleanup in subprocesses.
`server_fatal_handler_tests` injects allocation failures in subprocesses to
check fatal diagnostics, failure status and handler restoration without a server.
`server_process_environment_tests` checks core-dump limits, random seeds and
game signal policy in subprocesses that cannot alter the runner's environment.
`listener_startup_tests` exercises bind retries, diagnostics and shutdown during
retry waits, including real TCP/UDP bind failures without leaked descriptors.
`server_port_settings_tests` covers required listener ports without opening
sockets. Startup validates complete decimal ports in 1..65535 before publishing
configuration: `TCPPort`/`GameServerUDPPort` for game, `LoginServerPort`/
`LoginServerUDPPort` for login (after `-i` offsets), and `TCPPort` for shared.
Missing values, malformed text and out-of-range ports fail before manager
construction; leading plus/zeroes, surrounding spaces/tabs and trailing CR remain
accepted. The application and executable CLI tests also cover this boundary.
`outbound_server_connection_tests` checks the shared-server/Mofus connection
helper against loopback peers, including socket options, byte exchange and
refused connections. `socket_construction_tests` and game runtime tests inject
allocation failures in child processes to check socket/stream cleanup and
complete Mofus connection publication. Shared-server and Mofus ports use the
same checked reader when those features connect.
Socket construction coverage also checks accepted-socket adoption and failed
reconnects, including descriptor reuse and successful retries after failure.
`accepted_server_connection_tests` checks owned accepted-socket preparation,
actual nonblocking/linger options and rejection of pending peer errors.
The shared runtime's connection tests inject a listener without configuration
or database startup, sweep allocation faults through construction, acceptance
and descriptor-limit refusal, and verify registration, broadcast, retry and
destruction cleanup. `Socket::getSockError` reports whether the query failed
or `SO_ERROR` is nonzero; reading `SO_ERROR` consumes it.
The login runtime tests drive forwarded admission without database or listener
startup. `LoginConnection` retains each player through registration and ends
its session before cleanup; allocation sweeps, duplicate refusal, real socket
I/O and packet-history destruction cover that ownership. An injected
`LoginContext` supports scoped managers and restores the previous reconnect
binding. Manager teardown releases local resources without database logout.
The game runtime tests exercise owned game admission with an injected listener
and context, without database initialization. Sockets stay owned through
authorization; `GameConnection` retains players through registration and ends
their local session on cleanup.
Allocation faults, refusal diagnostics, descriptor limits, real socket I/O and
retry cover admission; scoped teardown restores the connection-info binding and
releases the listener, player table and both queues. Explicit `clearPlayers`
also attempts each disconnect. Cleanup requires stopped users and queue producers.
`GamePlayerHandoff` keeps the sending table or queue owner until destination
insertion succeeds. Runtime tests cover failed handoffs and retry through the
real `CGReady` handler and heartbeat paths, plus socket service after refusal.
Batch transfer tests cover ordering, failed destinations and throwing diagnostics.
Incoming and zone managers share player cleanup, with terminal session state,
duplicate references, failing logout and repeated clear covered without world
or database startup. Zone/account side effects remain in their existing callers.
`GameBroadcast` owns queued packet bytes and optional filter clones. Runtime
tests cover preparation failures, borrowed-input lifetime, real race-filtered
socket delivery and concurrent producers through heartbeat. Dispatch consumes
each started message; an uncaught failure retains only later entries for retry.
Queue access is synchronized, and quiescent manager destruction releases pending
messages. Literal broadcast framing stays unchanged.
`server_synchronization_tests` links the same `ServerSynchronization` library
as all three servers, without their runtimes or `main()`. Native error-checking
mutexes retain ownership through condition waits. Tests cover busy/recursive
refusal, wrong-owner unlock, returned error codes, attribute reuse, scoped
release, contended updates and condition wakeups/timeouts. Explicit native
attribute overrides remain available; name setup and destruction require
quiescent users.
`ServerWorkers` contains the production managed worker lifecycle. Worker,
startup-sequence and shutdown tests link it without `ServerCore` or server
startup. Coverage includes launch allocation failure/retry, identity/status
publication, concurrent metadata reads, self-join failure retention and the
existing stop/join contracts. Every production worker uses this backend; the
unused native creation/detachment backend and thread attributes are removed.
`ThreadPool` is part of the same library and consumes unique worker owners.
Its standalone cases cover failed registration and retry, rollback, stop/join
ordering and failures, and cleanup despite throwing diagnostics. Registration
closes at start/stop, completed drains are not replayed, and the zone thread
manager owns its pool by value. Destruction requires quiescent callers outside
the owned workers.
`player_connection_key_tests` exercises pure key-table calculation and owned
installation in `Player`. Allocation faults verify atomic replacement and
cleanup; fixed vectors and real socket I/O preserve existing table and stream
bytes. The game and login runtimes also exercise their production handshake
handlers, including repeated keys and retries, without running `main()`.
`socket_stream_setup_tests` exercises plain and encrypted buffered streams in
`de-kernel` with explicit sizes and a borrowed socket. Allocation faults verify
partial cleanup and atomic player socket replacement, preserving old buffers
on failure. The same replacement cases run against `GameServerPlayer` in the
shared runtime; successful replacement closes the previous socket, while
same-socket replacement retains it. Absent input/output streams stay absent.
`server_application_tests` runs configuration loading through lifecycle cleanup
and final reporting using an explicit context and controlled actions. It checks
configuration lifetime and restoration, rejected input and failure status without
starting a server or connecting to a database.
Final reporting preserves the completed drain result, attempts both flushes
independently and returns failed exit status for exceptions or stream error flags.
`server_worker_shutdown_tests` exercises the three servers' shared auxiliary
worker drain with real cooperative threads, checking stop-before-join order,
partial startup, dependency lifetime and retained failure reporting.
Stop, zone-drain, join and diagnostic failures do not skip remaining work;
reporting follows all joins and the first error reaches the lifecycle after
the drain attempts. Throwing worker overrides fall back to managed stop/join.
`server_start_sequence_tests` exercises the background startup steps and main
loop handoff used by all three servers. Shutdown gates stop later callbacks
after a request or worker failure; real managed-worker tests verify lifecycle
cleanup for partial startup and main-loop exceptions without booting a server.

## Build natively on macOS

The servers also build as native macOS executables with Apple Clang. This is
verified on Apple Silicon (macOS 27.0, Apple Clang 21, CMake 4.4). The
dependencies come from Homebrew:

```bash
xcode-select --install
brew install cmake mysql-client@8.4 luajit
```

`mysql-client@8.4` is the MySQL client library the build was verified
with; the unversioned `mysql-client` formula is a newer series nobody has
built against. Both are keg-only. CMake looks for them under Homebrew's
`opt/` directories, on Apple Silicon (`/opt/homebrew`) and Intel
(`/usr/local`) alike, preferring `mysql-client@8.4`, then
`mysql-client@8.0`, then `mysql-client`, so no extra flags are needed.
From the repository root:

```bash
make debug      # or: make release
```

That writes `bin/loginserver`, `bin/sharedserver`, `bin/gameserver` and
`bin/hashpw`. `bin/hashpw` works as it does in the container:

```bash
./bin/hashpw <<< 'new-password'
```

The Docker stack is still the way to run the servers. Running the native
binaries needs a MySQL they can reach (the compose file publishes no MySQL
port) and a copy of `conf/` with `HomePath`, `DB_HOST`, `UI_DB_HOST` and
`LoginServerIP` set for this machine. That has not been tried on macOS yet.

### Tests on macOS

`make dev-test` is the reference run. The suite also builds natively, with
one workaround: the macOS file system ignores letter case, so
`#include <assert.h>` finds the project's own `src/Core/Assert.h` and
googletest does not compile. Forcing the system header in first works. The
tree gets its own output root so that it leaves `make debug`'s `bin/` and
`lib/` alone:

```bash
cmake -B build-tests -DCMAKE_BUILD_TYPE=Debug -DDARKEDEN_BUILD_TESTS=ON \
    -DDARKEDEN_OUTPUT_ROOT="$PWD/build-tests" \
    -DCMAKE_CXX_FLAGS="-include $(xcrun --show-sdk-path)/usr/include/assert.h"
cmake --build build-tests --target wire_tests -j"$(sysctl -n hw.ncpu)"
(cd build-tests && ctest --output-on-failure)
```

The native suite has three known platform-specific failures: `ratchets`,
`proxy_acceptor_tests` and `shutdown_supervisor`. `docs/FIXES.md` records
each one; the pinned container remains the reference run.

## Howto

### Login to the MySQL

```sh
docker exec -it odk-mysql mysql -u elcastle -pelca110
```

```SQL
use DARKEDEN;
update GameServerInfo set IP = '192.168.0.16';
```

### Accounts and passwords

`initdb/DARKEDEN.sql` ships two development accounts, each with characters:

| Account  | Password |
|----------|----------|
| `111111` | `111111` |
| `222222` | `222222` |

Passwords are stored as argon2id hashes in `Player.Password` (the
loginserver's `PasswordHash` module, over the vendored `third_party/argon2`),
never in plain text. Registering from the client creates a hashed account.
To set or reset a password by hand, hash it with `bin/hashpw` (the password
is read from stdin so it stays out of shell history) and store the result:

```sh
docker exec -i odk-server ./hashpw <<< 'new-password'
```

```SQL
UPDATE DARKEDEN.Player SET Password = '$argon2id$v=19$...' WHERE PlayerID = 'someone';
```

An existing database needs the column widened once
(`initdb/migrations/001-argon2-password-column.sql`). Its rows can keep
their old plaintext value: the loginserver still accepts it and rewrites the
row as a hash on that account's next successful login, so nobody is locked
out. `bin/hashpw --verify '<stored value>'` checks a password against a
stored value.

### English content

The content tables of `initdb/DARKEDEN.sql` (NPC names and dialogue, zone,
monster and item names, system messages, nicknames and the rest) and the
quest lists in `data/` are English; the translations and the scripts that
write them are in `tools/i18n/` (its README explains the tables). A fresh
install gets the English rows from the seed; a database created before them
takes them once, without touching accounts, characters or items:

```sh
docker exec -i odk-mysql mysql -u elcastle -pelca110 DARKEDEN < initdb/migrations/004-english-content.sql
```

The client repository ships the matching English for the client-side data
(`tools/i18n` there); the NPC and place spellings are shared between the two.

### Pack pre-built binaries into an image

`Dockerfile.pub` packages an already-compiled `bin/` directory instead of
compiling from source, which is useful when publishing a release image. It
installs the same runtime libraries and applies the same `start.sh`/CRLF
handling as the source build's runtime stage. The checkout's `bin/` must hold
Linux binaries built for the Ubuntu 20.04 runtime (e.g. copied out of the
`darkeden-dev` volume; `make dev-build` does not update `bin/`). BuildKit is
required so that `Dockerfile.pub.dockerignore` (which keeps `bin/` in the
context) is used instead of `.dockerignore`:

```sh
DOCKER_BUILDKIT=1 docker build . -t darkeden:latest -f Dockerfile.pub
```
