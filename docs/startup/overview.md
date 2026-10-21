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

- **`ServerStartup`** parses arguments and loads an owned configuration, including the
  loginserver's optional ID offset, without publishing it (`server_startup_tests`).
- **`ServerPortSettings`** reads required TCP/UDP listener ports without sockets or runtime
  dependencies: complete decimal integers in 1..65535, with leading plus or zeroes, spaces, tabs
  and a trailing CR from a CRLF file still valid. `ServerApplication` validates the current
  server's effective ports after login offsets and before publishing; all five listener paths use
  the same reader before retrying binds, so direct manager construction refuses malformed values
  too. Generic integer properties, optional proxy configuration and other outbound ports keep
  their existing readers; shared-server and Mofus connections use this reader when they connect.
- **`PropertiesParser`** (`de::readProperties`, in `de-kernel`) takes a borrowed stream and
  merges into a destination `Properties`, so grammar and read failures are tested in memory
  (`properties_parser_tests`). It processes the last line without a newline, rejects empty keys
  and failed reads, and the file adapter rejects directories. Earlier entries stay in the
  destination after a parse error, so startup still loads an unpublished object.
- **`ServerLifecycle`** (`runServerLifecycle`) takes construction, start and stop actions,
  handles exceptions, requests shutdown before cleanup and returns the drain result and exit
  status. It uses the workers' `ServerShutdown` flags and never resets them; the caller keeps the
  configuration and server graph alive (`server_lifecycle_tests`).
- **`ServerApplication`** composes startup loading, port validation and lifecycle control. It
  owns the completed configuration, publishes it to an explicit `KernelContext`, runs the
  lifecycle actions, reports the drain result and flushes diagnostics. A configuration error
  returns an empty result before any action runs; the object permits one run, retains its
  configuration after the result, and restores the previous context binding on destruction. Each
  `main()` keeps it alive through `_Exit` once lifecycle execution has begun
  (`server_application_tests`).
- **`ServerProcessShutdown`** owns the SIGTERM/SIGINT handlers and the deadline, 30 seconds after
  a shutdown request. Each `main()` constructs it before initialization and checks `ready()`. On
  scope exit it cancels and joins the watcher and restores the previous handlers; it never clears
  shutdown state (`server_process_shutdown_tests`).
- **`ServerFatalHandlers`** installs the allocation handler for each server and the gameserver's
  terminate handler, restoring the previous ones on scope exit. Fatal diagnostics use fixed text
  and POSIX writes, so they cannot re-enter the allocation handler. The gameserver appends
  `CriticalError.log` and aborts; login and shared report to stderr and `_Exit(EXIT_FAILURE)`
  (`server_fatal_handler_tests`, allocation failure injected in child processes).
- **`ServerProcessEnvironment`** raises the core-dump soft limit to the inherited hard limit (a
  failed raise is nonfatal), seeds `rand` for the gameserver in its `main` and again at the
  start of `GameServer::init`, and ignores SIGPIPE, SIGALRM and SIGCHLD for the rest of the
  process. These settings are not restored (`server_process_environment_tests`, in subprocesses).
- **`ListenerStartup`** (`retryListenerStartup`) serves the five TCP/UDP listener paths. Only
  `BindException` is retried, with each caller's diagnostic and delay (1 ms for login's player
  listener, 1 s elsewhere); shutdown prevents the next attempt and interrupts the wait, checked
  every 10 ms (`listener_startup_tests`).
- **`OutboundServerConnection`** (`connectOutboundServer`) is the shared-server and Mofus
  clients' socket setup: one blocking attempt at an explicit numeric IPv4 endpoint, nonblocking
  I/O, linger off, returned as `unique_ptr<Socket>`. Retry delays, shutdown policy and protocol
  work stay with the caller (`outbound_server_connection_tests`).
- **Socket construction.** `SocketImpl::accept` closes an accepted descriptor if implementation
  allocation fails and keeps ownership through peer metadata setup; `ServerSocket::accept` keeps
  that implementation until its `Socket` wrapper adopts it; `Socket::reconnect` closes the old
  connection first and owns the replacement locally through creation, leaving the old closed
  implementation inspectable on failure (`socket_construction_tests`).
- **`ServerWorkerShutdown`** (`stopServerWorkers`) requests every auxiliary worker's stop, runs an
  optional action (the gameserver drains its zone pool here), then joins and reports retained run
  failures in order. An explicit diagnostic name preserves the login/shared `GameServerManager`
  label (`server_worker_shutdown_tests`).
- **`ServerStartSequence`** runs the background startup steps before the main loop and stops
  advancing once shutdown is observed; failures still reach lifecycle cleanup
  (`server_start_sequence_tests`).
- **`ServerWorkers`** holds `ManagedThread`, the `std::jthread` worker every process uses, and
  the thread pool ([workers and synchronization](workers-and-synchronization.md)).

The order inside a process, and what each thread then owns, is in
[threading](../threading/overview.md). `docs/build/toolchain.md` has the full C++20 contract
behind the thread lifetime.
