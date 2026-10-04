---
type: Reference
title: Server architecture
description: Locate process responsibilities, library boundaries and their enforcement.
tags: [server, architecture]
source_revision: "d116b3408efff31247ca368cc037adc77d16c76b"
generated:
  by: codex/gpt-6
  at: "2026-10-04T09:28:06Z"
sources:
  - id: guide
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/CLAUDE.md
  - id: kernel
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/src/Core/CMakeLists.txt
  - id: domain
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/src/domain/CMakeLists.txt
  - id: server
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/src/server/CMakeLists.txt
---

# Server architecture

The three cooperating processes are `loginserver` (authentication and character
selection), `sharedserver` (shared data such as guilds), and `gameserver`
(gameplay). `GameServerRuntime`, `LoginServerRuntime` and `SharedServerRuntime`
contain production implementation objects; each executable supplies its own
`main.cpp`. Runtime tests reuse those objects without process startup. Different
server runtimes stay in separate executables because names and registrations
can conflict.[^guide]

| Boundary | Where to start | Responsibility |
|---|---|---|
| `de-kernel` | `tests/arch/kernel_files.txt`, `src/Core/` | Shared wire classes and utilities, compiled without server-type macros. |
| `de-core` | `src/domain/` | Pure formulas independent of transport, database and server targets. |
| `ServerCore` | `src/server/CMakeLists.txt` | Common managers and repository implementations; independent startup, worker and lifecycle helpers have their own targets. |
| Server runtimes | `src/server/{game,login,shared}server/CMakeLists.txt` | Process-specific handlers and implementation objects. |

The build definitions establish these boundaries; `tests/arch/check_includes.pl`
and `tests/ratchet/ratchets.sh` enforce the documented include and membership
rules.[^kernel][^domain][^server][^guide] Follow `CLAUDE.md` for context ownership
and thread synchronization before changing managers.

See [builds and tests](builds-and-tests.md) for verification entry points and
[packet compatibility](packet-compatibility.md) for the client-facing boundary.

[^guide]: [CLAUDE.md](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/CLAUDE.md)
[^kernel]: [src/Core/CMakeLists.txt](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/src/Core/CMakeLists.txt)
[^domain]: [src/domain/CMakeLists.txt](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/src/domain/CMakeLists.txt)
[^server]: [src/server/CMakeLists.txt](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/src/server/CMakeLists.txt)
