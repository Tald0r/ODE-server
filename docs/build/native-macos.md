---
type: Guide
title: "Native macOS builds"
description: "The servers build natively with Apple Clang and Homebrew's keg-only mysql-client@8.4 and luajit; the test suite needs the system assert.h forced in, and three tests fail only there."
tags: [build, macos]
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-23T12:00:00Z }
---

# Native macOS builds

The servers also build as native macOS executables with Apple Clang. This was verified on Apple
Silicon (macOS 27.0, Apple Clang 21, CMake 4.4) on 2026-09-27. The dependencies come from
Homebrew:

```bash
xcode-select --install
brew install cmake mysql-client@8.4 luajit
```

`mysql-client@8.4` is the MySQL client library the build was verified with; the unversioned
`mysql-client` formula is a newer series nobody has built against. Both are keg-only. CMake looks
for them under Homebrew's `opt/` directories, on Apple Silicon (`/opt/homebrew`) and Intel
(`/usr/local`) alike, preferring `mysql-client@8.4`, then `mysql-client@8.0`, then
`mysql-client`, so no extra flags are needed. From the repository root:

```bash
make debug      # or: make release
```

That writes `bin/loginserver`, `bin/sharedserver`, `bin/gameserver` and `bin/hashpw`
([build commands](build-commands.md)). `bin/hashpw` works as it does in the container:

```bash
./bin/hashpw <<< 'new-password'
```

The Docker stack is still the way to run the servers. Running the native binaries needs a MySQL
they can reach (the compose file publishes no MySQL port) and a copy of `conf/` with `HomePath`,
`DB_HOST`, `UI_DB_HOST` and `LoginServerIP` set for this machine
([configuration](../database/configuration.md)). That has not been tried on macOS yet.

# Tests on macOS

`make dev-test` is the reference run ([container builds](container-builds.md)); it works on macOS
under OrbStack (verified) and should under Docker Desktop, and on Apple Silicon the image is arm64
and the build is native to it. The suite also builds natively, with one workaround: the macOS file
system ignores letter case, so `#include <assert.h>` finds the project's own `src/Core/Assert.h`
and googletest does not compile. Forcing the system header in first works. The tree gets its own
output root so that it leaves `make debug`'s `bin/` and `lib/` alone:

```bash
cmake -B build-tests -DCMAKE_BUILD_TYPE=Debug -DDARKEDEN_BUILD_TESTS=ON \
    -DDARKEDEN_OUTPUT_ROOT="$PWD/build-tests" \
    -DCMAKE_CXX_FLAGS="-include $(xcrun --show-sdk-path)/usr/include/assert.h"
cmake --build build-tests --target wire_tests -j"$(sysctl -n hw.ncpu)"
(cd build-tests && ctest --output-on-failure)
```

The native suite has three known platform-specific failures: `ratchets`, `proxy_acceptor_tests`
and `shutdown_supervisor`. [The fix log](../reviews/FIXES.md) records each one; the pinned
container remains the reference run.
