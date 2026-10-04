---
type: Reference
title: Builds and tests
description: Find the supported build commands and choose the relevant test boundary.
tags: [server, build, testing]
source_revision: "d116b3408efff31247ca368cc037adc77d16c76b"
generated:
  by: codex/gpt-6
  at: "2026-10-04T09:28:06Z"
sources:
  - id: guide
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/CLAUDE.md
  - id: make
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/Makefile
  - id: image
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/Dockerfile.dev
  - id: tests
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/tests/CMakeLists.txt
---

# Builds and tests

The repository uses C++20 and CMake. `Dockerfile.dev` supplies the pinned Zig
compiler used by the development helper; its host GCC is for dependency tooling.
The root `Makefile` is the command entry point, and `CLAUDE.md` contains the
full toolchain, formatting and before-push instructions.[^image][^make][^guide]

| Need | Entry point |
|---|---|
| Prepare the development image | `docker build -f Dockerfile.dev -t darkeden-dev .` |
| Build and run the suite in that environment | `make dev-test` |
| Build production executables there | `make dev-build` |
| Run the suite with a suitable local compiler and library | `make test` |
| Exercise repositories against a database | `make integration-test` (separate Docker/MySQL tier) |

These are existing commands, not evidence of a run of this documentation
snapshot.[^make][^guide]

Start pure-rule changes with the corresponding isolated target in
`tests/CMakeLists.txt`. Server runtime tests exercise production handlers and
ownership with supplied collaborators; they do not require ordinary server
startup or a live database. Wire tests cover serialization and inventories;
architecture checks and ratchets enforce structural rules. Follow the existing
full-suite requirement in `CLAUDE.md` before pushing.[^tests][^guide]

See [architecture](architecture.md) to choose the owning library and
[login lifecycle](login-lifecycle.md) for account ownership test entry points.

[^guide]: [CLAUDE.md](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/CLAUDE.md)
[^make]: [Makefile](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/Makefile)
[^image]: [Dockerfile.dev](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/Dockerfile.dev)
[^tests]: [tests/CMakeLists.txt](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/tests/CMakeLists.txt)
