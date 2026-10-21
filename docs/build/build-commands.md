---
type: Guide
title: "Build commands"
description: "make wraps CMake: Debug by default with tests off, binaries in bin/ and libraries in lib/, and Debug is the configuration with the ownership checks armed."
tags: [build]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Build commands

```bash
make            # Debug build (default)
make debug      # same as make
make release    # Release build
make clean      # remove build/, bin/, lib/
```

The project uses CMake; the `Makefile` wraps it. `make` / `make debug` configures with
`-DCMAKE_BUILD_TYPE=Debug`, `make release` with `Release`; both build with tests off
(`-DDARKEDEN_BUILD_TESTS=OFF`). Binaries go to `bin/`, libraries to `lib/`.

**For development, always choose the debug build**: `DE_OWNERSHIP_CHECKS` is armed only there
([the mutation rule](../threading/mutation-rule.md)). `Assert` and `__BEGIN_TRY` stay live in every
configuration, because `NDEBUG` is never defined: the optimized configurations pass `-UNDEBUG`
(`zig c++` defines it from `-O1` up), and `Assert.h` and `Exception.h` refuse to compile with it
([assertions and NDEBUG](../architecture/assertions-never-ndebug.md)).

The test suite is built by `make test` and `make dev-test` ([testing overview](../testing/overview.md));
the container build that makes the latter fast is [container builds](container-builds.md). Prefer
the pinned Zig container: Ubuntu 20.04's distro GCC 9 lacks the required library facilities, and
CMake checks `jthread`, `stop_token` and stop-aware condition-variable waits at configure time
([toolchain notes](toolchain.md)).
