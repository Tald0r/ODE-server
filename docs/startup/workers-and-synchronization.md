---
type: Rule
title: "Workers and synchronization"
description: "Mutexes use native ownership, every worker is a managed jthread whose entry follows publication, and pools retain their members through a failed registration."
tags: [startup, threading]
enforced_by: [server_synchronization_tests, managed_thread_tests, server_start_sequence_tests, server_worker_shutdown_tests, cooperative_thread_tests]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Workers and synchronization

**Every worker in all three processes is a `ManagedThread`** (`std::jthread`), implementing the
abstract `Thread` interface in the `ServerWorkers` library; the unused native creation and
detachment backend and `ThreadAttr` are gone. Worker entry waits for identity and status
publication, and native identity reads are atomic. Start and stop are serialized: stop before
start is terminal, and join allows a concurrent stop request. A derived destructor stops and
joins before destroying its members. SIGTERM/SIGINT request process shutdown; main exits its
client loop, requests every worker to stop and joins them while their dependencies are alive
([lifecycle rules](lifecycle-rules.md)).

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| Default server mutexes use native ownership, including across condition waits; synchronization wrappers interpret returned pthread error codes | `server_synchronization_tests` over `ServerSynchronization`, shared by all servers | false try-lock success, lost ownership after waiting, ignored misuse, wrong error/timeout classification, failed attribute reuse or lost contended updates/wakeups |
| Production workers share an independently linkable managed lifecycle; worker entry follows identity/status publication and failed launch leaves a retryable owner | `managed_thread_tests`, `server_start_sequence_tests` and `server_worker_shutdown_tests` over `ServerWorkers` | early worker entry, unsafe metadata access, damaged launch state, lost stop/join/failure behavior or dependence on `ServerCore`/executable startup |
| Worker pools consume explicit owners, retain existing members after failed registration, and drain before releasing workers despite diagnostic failure | `WorkerPoolOwnership`/`WorkerPoolTest` and the managed pool cases over `ThreadPool` in `ServerWorkers` | leaked/refused ownership, broken registration retry, skipped stop/join attempts, replaced startup failure, repeated completed drain or aborted cleanup after reporting |

What each worker owns once it runs is the subject of [threading](../threading/overview.md);
`CooperativeThread.h` is reached only through `ManagedThread`.
