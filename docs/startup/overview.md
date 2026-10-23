---
type: Design
title: "Startup composition"
description: "How each main() is composed from the shared startup, port, parser, application, lifecycle, shutdown, fatal-handler, environment, listener, connection and worker libraries, and which test owns each."
tags: [startup, architecture]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Purpose

The three entry points share one shape. Each `main()` builds a process-shutdown guard, parses
its arguments into an owned configuration, validates its ports, publishes the configuration to a
context, runs the server's lifecycle through the shared control flow, and reports the drain
result. Every piece is a library of its own that links only `de-kernel` (or nothing), so each one
is tested without a server runtime or a database. The rules those tests own are in
[lifecycle rules](lifecycle-rules.md), [sockets and connections](sockets-and-connections.md) and
[workers and synchronization](workers-and-synchronization.md).

# Moving parts

- **`ServerStartup`** depends only on `de-kernel`. It parses arguments and loads an owned
  configuration, including the loginserver's optional ID offset, without publishing it.
  `server_startup_tests` exercises this without linking a server runtime.
- **`ServerPortSettings`** reads required TCP/UDP listener ports without sockets or runtime
  dependencies. Values must be complete decimal integers in 1..65535; leading plus or zeroes,
  spaces, tabs and a trailing CR from a CRLF file remain valid. `ServerApplication` validates the
  current server's effective ports after login offsets and before publishing configuration. All
  five listener paths use the same reader before retrying binds, so direct manager construction
  also refuses malformed values. Generic integer properties, optional proxy configuration and other
  outbound ports keep their existing readers; shared-server and Mofus connections also use this
  reader (`readServerPort`) when their features connect, and those optional services are not
  required by application startup. Pure settings tests, application tests and executable CLI
  failures own this boundary.
- **`PropertiesParser`.** `Properties::load` opens the file and delegates to `de::readProperties`
  in `PropertiesParser` (in `de-kernel`). The parser accepts a borrowed stream and merges into a
  destination `Properties`, so grammar and read failures are tested in memory. It processes the
  last line without requiring a newline and rejects empty keys and failed reads; the file adapter
  explicitly rejects directories. Earlier entries remain in the destination after a parse error,
  so startup still loads an unpublished object. `properties_parser_tests` owns this contract,
  including EOF under stream exception masks and the existing whitespace and comment rules.
- **`ServerLifecycle`** also depends only on `de-kernel`. `runServerLifecycle` takes
  construction/initialization, start and stop actions, handles exceptions, requests shutdown
  before cleanup and returns the drain result and exit status. It uses the workers'
  `ServerShutdown` flags and never resets them. The caller keeps the configuration and server
  graph alive. `server_lifecycle_tests` runs the shared control flow with controlled actions and
  isolated diagnostics, without starting a server or a deadline thread.
- **`ServerApplication`.** The entry points link it; it composes startup loading, listener-port
  validation and lifecycle control. It owns the completed configuration, publishes it to an
  explicit `KernelContext`, runs the supplied lifecycle actions, reports the drain result and
  flushes diagnostics. A configuration error returns an empty result before any action runs. The
  object permits one run and retains its configuration after the result, including a failed
  drain; destruction restores the previous context binding before freeing it. Each `main()` keeps
  it alive through `_Exit` once lifecycle execution has begun, preserving the legacy server
  graph's configuration even if workers could not stop. Tests use a local context and controlled
  actions; `server_application_tests` covers publication, lifetime, failure paths, effective login
  overrides and final reporting without linking a runtime or database.
- **`ServerProcessShutdown`** owns the SIGTERM/SIGINT handlers and the deadline. Each `main()`
  constructs it before initialization and checks `ready()` before proceeding. The default
  deadline is 30 seconds after a shutdown request. On scope exit it cancels and joins the watcher,
  then restores the previous signal handlers; it never clears shutdown state. The library depends
  only on `Threads::Threads`. `server_process_shutdown_tests` delivers real signals to main and
  worker threads and checks blocked lifecycle paths in subprocesses.
- **`ServerFatalHandlers`** installs the allocation handler for each server and the gameserver's
  terminate handler, restoring previous handlers on scope exit. It has no runtime or kernel
  dependency; the common `ServerKind` enum lives in its own header. Fatal diagnostics use fixed
  text and POSIX writes, so they cannot re-enter the allocation handler through C++ heap
  allocation. The gameserver still appends `CriticalError.log` and aborts; its emergency records
  are plain message lines without timestamp formatting. Login and shared report allocation failure
  to stderr and `_Exit(EXIT_FAILURE)`, preserving their existing terminate handlers.
  `server_fatal_handler_tests` injects allocation failure on main and worker threads in child
  processes with core dumps disabled.
- **`ServerProcessEnvironment`** holds the other process setup without a runtime or kernel
  dependency. All three entry points call `raiseCoreDumpLimit` before server construction, raising
  the soft limit to the inherited hard limit; the returned syscall error remains nonfatal for
  startup. The gameserver seeds `rand` in its main and reseeds at the start of `GameServer::init`,
  at the same points as before. `initializeGameProcess` also ignores SIGPIPE, SIGALRM and SIGCHLD
  for the rest of the process, leaving the shutdown handlers and thread masks alone. Explicit seeds
  make this setup deterministic in tests. Unlike the scoped shutdown and fatal guards, these
  process settings are not restored; `server_process_environment_tests` changes them only in
  subprocesses.
- **`ListenerStartup`** supplies `retryListenerStartup` for the five TCP/UDP listener paths in the
  three runtimes. It accepts binding and reporting actions and links only `de-kernel` and threads.
  Only `BindException` is retried, using each caller's existing diagnostic and delay (1 ms for
  login's player listener, 1 s elsewhere). Shutdown prevents the first or next attempt and
  interrupts the wait, checked at most every 10 ms; other exceptions propagate. Socket
  constructors release their resources if setup throws, and managers keep temporary ownership
  through socket configuration. `listener_startup_tests` checks the retry policy and real occupied
  ports, including descriptor reuse after failed TCP and UDP construction.
- **`OutboundServerConnection`** owns the shared-server and Mofus clients' common socket setup.
  `connectOutboundServer` takes an explicit numeric IPv4 endpoint, makes one blocking connection
  attempt, configures nonblocking I/O and disables linger, then returns `unique_ptr<Socket>`.
  Retry delays, shutdown policy and protocol work stay with each caller. Socket constructors
  retain temporary implementation ownership through creation. `Player(Socket*)` adopts even on
  failed stream construction, and Mofus publishes socket, input and output together after complete
  setup. The focused connection tests (`outbound_server_connection_tests`) use real loopback peers;
  isolated allocation probes verify resource cleanup in the kernel and the production game runtime
  without starting a server or database.
- **Socket construction.** `SocketImpl::accept` closes an accepted descriptor if implementation
  allocation fails and retains ownership through peer metadata setup; `ServerSocket::accept`
  retains that implementation until its `Socket` wrapper can adopt it. Empty nonblocking accepts
  still return null. `Socket::reconnect` closes the old connection first, then owns a replacement
  locally through creation and connection. On failure the old, closed implementation remains
  available for inspection, safe repeated close, destruction or retry; a successful replacement
  then takes its place. Its retained descriptor number is not closed again if another socket has
  reused it. `socket_construction_tests` covers these handoffs and failures.
- **`ServerWorkerShutdown`** provides `stopServerWorkers` for all three servers' auxiliary
  workers. After stopping the foreground loop, each server supplies borrowed workers and its error
  stream. The helper requests every stop, runs an optional action (the gameserver drains its zone
  pool here), then joins and reports retained run failures in order. An explicit diagnostic name
  preserves the login/shared `GameServerManager` label. Stop, extra-drain and join exceptions still
  propagate to the lifecycle's failed-drain handling; worker run failures are reported while
  remaining joins continue. Ownership, the server's stop idempotence and the process deadline stay
  with their existing callers. `server_worker_shutdown_tests` uses real cooperative workers,
  including partial startup and injected failures, without linking a runtime or database.
- **`ServerStartSequence`** runs the background startup steps before the main loop and stops
  advancing once shutdown is observed; failures still reach lifecycle cleanup
  (`server_start_sequence_tests`).
- **`ServerWorkers`** holds `ManagedThread`, the `std::jthread` worker every process uses, and
  the thread pool ([workers and synchronization](workers-and-synchronization.md)).

What each of these tests covers in detail is in [test coverage](../testing/test-coverage.md).

The order inside a process, and what each thread then owns, is in
[threading](../threading/overview.md). `docs/build/toolchain.md` has the full C++20 contract
behind the thread lifetime.
