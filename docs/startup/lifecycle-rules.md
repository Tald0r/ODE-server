---
type: Rule
title: "Lifecycle rules"
description: "Arguments, configuration and ports are validated before anything is published; shutdown is requested before cleanup, every cleanup step is attempted, and a worker failure survives into the exit status."
tags: [startup]
enforced_by: [server_startup_tests, server_startup_cli, properties_parser_tests, server_port_settings_tests, server_application_tests, server_lifecycle_tests, server_process_shutdown_tests, server_fatal_handler_tests, server_process_environment_tests, server_worker_shutdown_tests, server_start_sequence_tests]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Lifecycle rules

Every rule here is owned by a test that links no server runtime and no database; the libraries
are described in [startup composition](overview.md).

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| Startup arguments and login offsets are validated before a completed configuration is published | `server_startup_tests`, over the shared `ServerStartup` library, and `server_startup_cli`, over all three executables | malformed arguments accepted, an incorrect or partially applied override, a failed load replacing the published configuration, or the wrong failure status/diagnostic from main |
| Configuration parsing processes the final line without a newline and rejects missing keys and read errors before startup publication | `properties_parser_tests` over `PropertiesParser` in `de-kernel`, plus `server_startup_tests` and `server_startup_cli` | a dropped final property, a malformed line accepted, a read failure spinning or silently succeeding, or a directory accepted as configuration |
| Required listener ports are complete decimal values in 1..65535, validated after login offsets and before publication or manager construction | `server_port_settings_tests`, `server_application_tests` and `server_startup_cli`, over `ServerPortSettings` shared by application startup and all five listeners | a malformed, missing or wrapped port accepted, the wrong server's keys required, a rejected configuration published, or a lifecycle action run after invalid input |
| Application configuration is published only after a successful load, stays alive through cleanup and the result, and restores its previous context binding on scope exit; final reporting attempts both flushes and preserves drain status while recording output failure | `server_application_tests`, over the shared `ServerApplication` used by all three entry points | actions running after a configuration error, an incorrect effective login override, a lost binding or drain result, a successful exit after reporting failure, skipped final flushes or an incorrect completion diagnostic |
| Every server requests shutdown before stopping, attempts cleanup after failed startup even when reporting throws, and preserves worker failures in its exit status | `server_lifecycle_tests`, over the shared `ServerLifecycle` library used by all three entry points | starting after a shutdown request, missing or repeated cleanup, diagnostics before marking failure, one failed report suppressing the other destination, a thrown diagnostic hiding the drain result or a successful exit after failure |
| SIGTERM/SIGINT request shutdown from any thread, and the process deadline bounds blocked initialization and cleanup | `server_process_shutdown_tests`, over the `ServerProcessShutdown` guard used by all three entry points and the shared lifecycle runner | a missed signal, a clean exit after the deadline, a deadline firing before any request or after guard destruction, or signal handlers left installed after the guard's scope |
| Fatal allocation handlers report failure without C++ heap allocation or normal process teardown | `server_fatal_handler_tests`, over the shared `ServerFatalHandlers` guard, with allocation faults confined to subprocesses | a successful allocation-failure exit, recursive allocation while reporting, an exit callback running, a lost diagnostic or a handler left installed after its scope |
| Core-dump setup respects the inherited hard limit; game process initialization seeds rand and ignores only its three legacy signals | `server_process_environment_tests`, over `ServerProcessEnvironment`, with process mutations confined to subprocesses | a failed soft-limit raise under a finite cap, a changed hard limit or unrelated resource, an incorrect seeded sequence, or damaged shutdown handlers/state |
| Server shutdown requests every auxiliary worker's stop before draining game zones or joining any auxiliary worker; every drain/report is attempted and the first error propagates after all attempts | `server_worker_shutdown_tests`, over `ServerWorkerShutdown` used by all three runtimes | a join before all stop requests, skipped stop/action/join/report after failure, reporting before remaining joins, a lost or incorrectly named failure, replaced first error or a successful lifecycle exit after worker failure |
| Background startup steps precede the main loop and stop advancing once shutdown is observed; failures still reach lifecycle cleanup | `server_start_sequence_tests`, over `ServerStartSequence` used by all three runtimes, including real managed workers composed with lifecycle and drain helpers | a later startup callback after a shutdown request, early main-loop entry, a translated/lost exception, lost failure status or a worker left undrained after partial startup |

After the shutdown sequence the process uses `_Exit` to reclaim the legacy singleton graph
without running its unaudited destructors; this adds no world-save operation. The 30-second
watchdog (`ServerShutdown::Deadline`) forces a nonzero exit if startup, I/O or a heartbeat
prevents shutdown.
