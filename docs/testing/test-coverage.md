---
type: Reference
title: "What the runtime and library tests cover"
description: "Per process and per library, what the runtime tests and the isolated startup, socket and worker tests exercise beyond the rule each owns: the inputs they supply, the failures they inject and the policies they pin."
tags: [testing]
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-23T12:00:00Z }
---

# What the runtime and library tests cover

The rule each test owns is in its area's concept; this is the coverage behind those rules, as
the README described it until it became a getting-started guide.
[The testing overview](overview.md) says how the suite runs.

# The runtime tests

The suite includes `game_server_runtime_tests`, `login_server_runtime_tests`
and `shared_server_runtime_tests`. They link the same server implementation
objects as the production executables, with a test entry point instead of
`main.cpp`, and exercise real packet handlers without starting a server or
connecting to MySQL. The first suite build therefore also compiles the server
runtimes. Smaller rule tests remain available as individual CMake targets.

## Sharedserver

Rules: [construction and catalogues](../shared/construction-and-catalogues.md), [guilds](../shared/guilds.md).

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
The shared runtime's connection tests inject a listener without configuration
or database startup, sweep allocation faults through construction, acceptance
and descriptor-limit refusal, and verify registration, broadcast, retry and
destruction cleanup. `Socket::getSockError` reports whether the query failed
or `SO_ERROR` is nonzero; reading `SO_ERROR` consumes it.

## Common world and server catalogues

Rules: [world and server catalogues](../game/world-and-server-catalogues.md).

`world_catalogue_tests` links the common production world loader directly from
`ServerCore`, without a server runtime or database connection. It checks owned
replacement, rollback and retry after load failures, ID/status validation,
read-only lookups and preservation through throwing load diagnostics.
`server_catalogue_tests` similarly links the common game-server catalogue from
`ServerCore`. It checks owned rows/traversal tables, complete non-PK and castle
flags, cold and repeated cleanup, field validation, world zero and rollback/retry
after query, allocation or diagnostic failure without executable startup.

## Loginserver

Rules: [catalogues and selection](../login/catalogues-and-selection.md),
[account ownership and kicks](../login/account-ownership-and-kicks.md),
[character and account flows](../login/character-and-account-flows.md).

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
Normal login, reconnect, registration and kick completion share an explicit
account owner. It prepares identity storage before acquiring LOGON and retains
the acquired account across later failures or ID changes. Disconnect takes
explicit transport actions and persistence, attempts every cleanup step, and
retains failed logouts for retry. Tests cover all phases, first-error preservation,
registration, completion retry, production socket closure and allocation faults.
Login-player retirement removes table membership and polling before cleanup.
An allocation-free owner keeps failed account logouts for retries at five-second
monotonic deadlines; completed cleanup releases the player immediately. Runtime
tests cover broken cleanup/reporting, deadline boundaries, descriptor reuse,
manager locks, real I/O and both incoming-reply handlers. Pending teardown is
local and iterative; outstanding database cleanup does not survive process exit.
Incoming game-connection replies use an explicit manager, sender and retirement
actions. Only a matching pending account is consumed; late and duplicate replies
leave other phases alone. Runtime tests pin the saved public reconnect address,
port/key bytes, lock ownership and cleanup after send/allocation failure. Failed
reply writes close without flushing; failed logout remains in the retirement owner.
Selected-character incoming requests take explicit catalogue, configuration,
repositories and sender inputs. Preparation validates the destination and packet
fields before publishing the pending phase and public address. Send failures
restore local state; successful sends precede the account and character writes,
whose failures preserve the sent request. Runtime tests cover configured port
fallback, owned snapshots, real UDP delivery, reply composition and allocation cleanup.
The complete character-selection flow also runs with supplied topology, rules,
repositories and reply actions. It snapshots session/packet inputs, preserves
legacy refusal bytes and exception mapping, and keeps diagnostics best effort.
Tests cover every rejection, each race, partial writes, retained account ownership,
broken production output and allocation failure without executable startup.
Character creation likewise takes an explicit repository, balance cache and reply
actions. Tests pin race/slot persistence order, packet attribute updates, refusal
bytes, exception boundaries and publication only after a successful reply. Earlier
inserts remain visible after later failure; retry and allocation tests exercise
that existing boundary. Default replies still use the real player output stream.
The creation decision also takes explicit raw random draws and optional Ousters
diagnostics. Deterministic tests cover all 136 legal Vampire attribute spreads,
draw ordering and cache reuse after failure. Production keeps its two-draw modulo
rules and log text; failed diagnostics cannot change acceptance or rejection.
Character deletion takes explicit persistence, replies and diagnostics as well.
Tests pin refusal bytes, retirement/record/purge order, owned request fields and
session publication after sending. Reporting failures cannot skip replies;
earlier persistence effects remain after later failure, and retry does not resume
an already retired character. Real output/audit checks and allocation sweeps run
without executable startup or a database.
Character-name availability uses explicit world, repository and sender inputs.
It checks configured world IDs, owns the queried name through callbacks and
preserves the existing lookup and reserved-name rules. Runtime tests cover
sparse/boundary worlds, real reply bytes, failures and publication after sending.
Account-name availability likewise takes an explicit repository and sender,
owns the requested ID through lookup/reply, and publishes the registration phase
only after sending. Tests preserve its distinct lookup policy, repeated-query
behavior, exact reply bytes and caller-owned account cleanup after failures.
Character-list refresh owns the selected world/account and takes explicit
repository and sender inputs. It keeps the reply alive through sending and enters
character management only after the sender returns. Runtime tests cover complete
and empty lists, exception mapping, real output, retry and allocation cleanup.
Account registration takes explicit hashing, connection settings, catalogue
lookup, persistence, replies and diagnostics. Tests cover refusal/retry policy,
partial writes and cleanup ownership, checked world/group publication after
sending, real production adapters and allocation failure without server startup.
Web and NetMarble authentication take explicit repository, password, reply
and diagnostic actions. Tests cover credential snapshots, hash migration,
account creation, web-key consumption before free-pass publication, refusal
bytes, partial writes and allocation cleanup. Diagnostic failures cannot change
these authentication results, and web-key mismatch logs omit both credentials.
The login runtime tests drive forwarded admission without database or listener
startup. `LoginConnection` retains each player through registration and ends
its session before cleanup; allocation sweeps, duplicate refusal, real socket
I/O and packet-history destruction cover that ownership. An injected
`LoginContext` supports scoped managers and restores the previous reconnect
binding. Manager teardown releases local resources without database logout.

## Gameserver

Rules: [connections and players](../game/connections-and-players.md).

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
`GameDestructor` destroys a `Guild`, a `GuildManager`, a `Party` and a
`HolyLandManager` under their own lock, an `InventorySlot`, an `Inventory`, an
`EffectScheduleWork` and a `GamePlayer` that break their invariant, and an
`EffectAddItem` destroyed without a target, pinning what a destructor does where
it cannot throw; `GameCatch` drives a disconnect whose socket cannot send, which
logs both failed flushes and still ends the session.
`GameLevelWarCommand` builds a level war zone and its group in memory and runs
`*command LevelWar` from a thread that does not hold the group: the schedule
lands only when the group drains its mailbox.

# The startup and library tests

These link no server runtime and no database; the libraries are described in
[startup composition](../startup/overview.md).

## Startup, configuration and lifecycle

Rules: [lifecycle rules](../startup/lifecycle-rules.md).

`server_startup_tests` covers argument parsing, configuration loading and the
login-server port/ID overrides without linking a server runtime.
`properties_parser_tests` checks configuration grammar, final lines without a
newline and read failures using in-memory streams, including EOF under stream
exception masks and the existing whitespace and comment rules.
`server_lifecycle_tests` exercises the shared initialization, start and stop
sequence, including partial startup, shutdown requests, exception reporting
and worker failure status, with controlled actions and isolated diagnostics. It
links without a server runtime or database and starts no deadline thread.
Throwing formatters and rejected output cannot skip cleanup or hide its result;
process failure is marked before reporting, and startup log/console diagnostics
are attempted independently.
`server_process_shutdown_tests` checks the shared signal handlers and deadline,
delivering real signals to main and worker threads, and blocked startup/cleanup
in subprocesses.
`server_fatal_handler_tests` injects allocation failures on main and worker
threads in subprocesses with core dumps disabled, to check fatal diagnostics,
failure status and handler restoration without a server.
`server_process_environment_tests` checks core-dump limits, random seeds and
game signal policy in subprocesses that cannot alter the runner's environment.
`listener_startup_tests` exercises bind retries, diagnostics and shutdown during
retry waits, including real TCP/UDP bind failures on occupied ports without
leaked descriptors and descriptor reuse after a failed construction.
`server_port_settings_tests` covers required listener ports without opening
sockets. Startup validates complete decimal ports in 1..65535 before publishing
configuration: `TCPPort`/`GameServerUDPPort` for game, `LoginServerPort`/
`LoginServerUDPPort` for login (after `-i` offsets), and `TCPPort` for shared.
Missing values, malformed text and out-of-range ports fail before manager
construction; leading plus/zeroes, surrounding spaces/tabs and trailing CR remain
accepted. The application and executable CLI tests also cover this boundary.
`server_application_tests` runs configuration loading through lifecycle cleanup
and final reporting using an explicit context and controlled actions. It checks
configuration publication, lifetime and restoration, effective login overrides,
rejected input and failure status without starting a server or connecting to a
database.
Final reporting preserves the completed drain result, attempts both flushes
independently and returns failed exit status for exceptions or stream error flags.
`server_worker_shutdown_tests` exercises the three servers' shared auxiliary
worker drain with real cooperative threads, checking stop-before-join order,
partial startup, dependency lifetime and retained failure reporting, without
linking a runtime or database.
Stop, zone-drain, join and diagnostic failures do not skip remaining work;
reporting follows all joins and the first error reaches the lifecycle after
the drain attempts. Throwing worker overrides fall back to managed stop/join.
`server_start_sequence_tests` exercises the background startup steps and main
loop handoff used by all three servers. Shutdown gates stop later callbacks
after a request or worker failure; real managed-worker tests verify lifecycle
cleanup for partial startup and main-loop exceptions without booting a server.

## Sockets and connection keys

Rules: [sockets and connections](../startup/sockets-and-connections.md).

`outbound_server_connection_tests` checks the shared-server/Mofus connection
helper against loopback peers, including socket options, byte exchange and
refused connections. `socket_construction_tests` and game runtime tests inject
allocation failures in child processes to check socket/stream cleanup and
complete Mofus connection publication, without starting a server or database.
Shared-server and Mofus ports use the
same checked reader when those features connect.
Socket construction coverage also checks accepted-socket adoption and failed
reconnects, including descriptor reuse and successful retries after failure.
`accepted_server_connection_tests` checks owned accepted-socket preparation,
actual nonblocking/linger options and rejection of pending peer errors.
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

## Synchronization and workers

Rules: [workers and synchronization](../startup/workers-and-synchronization.md).

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
