---
type: Guide
title: "Testing overview"
description: "make test or make dev-test before every push: the wire tests over the goldens, one runtime test binary per server over the production objects, the isolated decision tests, the script audits and the MySQL tier."
tags: [testing]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Testing overview

**Every architectural rule is owned by something that fails, never by a memory.** Each area's
concept holds the table of its rules, the owner of each and what fails when it is broken; the
suite below is where those owners run, and [test coverage](test-coverage.md) says what the
runtime and library tests exercise beyond the rule each owns.

```bash
make test              # build and run the suite with a local C++20 compiler
make dev-test          # same suite, built in the container off a local workspace
make integration-test  # MySQL-backed repository tier (throwaway MySQL 5.7 + initdb/
                       # schema + the real MySQL*Repository impls); needs docker
```

Run one of the first two before every push: CI builds master only ([CI](../build/ci.md)). On a
Windows host `make dev-test` is the one to use ([container builds](../build/container-builds.md)).

# What the suite holds

| Kind | Targets | What it exercises |
|---|---|---|
| Wire | `wire_tests` | golden byte fixtures and loopback round-trips for the registered packet factories (the inventory covers all 465, the goldens 451), the frame bounds and the read-bounds pins ([wire contract](../protocol/wire-contract.md), [frame gate](../protocol/frame-gate.md)) |
| Runtime | `game_server_runtime_tests`, `login_server_runtime_tests`, `shared_server_runtime_tests` | the production `*Runtime` objects linked without `main.cpp`, with supplied repositories, real sockets and allocation faults; their test groups own the rule tables under [game](../game/index.md), [login](../login/index.md) and [shared](../shared/index.md) |
| Startup libraries | `server_*_tests`, `listener_startup_tests`, `socket_*_tests`, `managed_thread_tests` and the rest | the entry-point composition, each library alone ([startup](../startup/index.md)) |
| Decisions | `login_decision_tests`, `character_*_tests`, `*_decision_tests`, `formula_tests` | `Outcome` values and pure formulas against fakes, no database ([exceptions and outcomes](../architecture/exceptions-and-outcomes.md)) |
| Links | `game_client_link_tests`, `login_client_link_tests`, `game_frame_gate_tests` | what a client connection admits and how a frame is consumed |
| Fuzz replays | `fuzz_replay_game`, `fuzz_replay_login`, `fuzz_replay_game_any_id` | every golden seed and every recorded crash through the mirrored receive loops ([fuzzing](../protocol/fuzzing.md)) |
| Audits | `ratchets`, `arch_includes`, `critical_section_audit`, `knowledge_bundle` | the shrink-only counts, the include graph, hand-written unlocks, this bundle |
| Integration | `make integration-test` (`tests/integration/`) | every `MySQL*Repository` implementation against a real MySQL with the `initdb/` schema |

The whole packet set is compiled once, macro-free, in `de-kernel`, the archive the deployed
servers and the tests link ([module layering](../architecture/module-layering.md)). The existing
isolated rule tests remain the faster choice for pure logic; the runtime tests are for ownership
and dispatch. A number a test prints is only as good as the test's own denominator: the ratchet
script fails on a count that drops unrecorded ([ratchets](ratchets.md)).
