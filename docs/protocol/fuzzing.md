---
type: Guide
title: "Packet-read fuzzing"
description: "Two libFuzzer targets mirror the game and login receive loops up to read(), deliver every input three ways, and replay their corpus and regressions in ctest."
tags: [protocol, testing, security]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Packet-read fuzzing

`tests/fuzz/` holds two fuzz targets, one per server a client talks to. Each mirrors its receive
loop (the gameserver's per-frame gate, `GameFrameGate::next`, which `GamePlayer::processCommand`
runs, and `LoginPlayer::processCommand`) up to the packet's `read()`, stops short of the
handler, and prints the packet with `toString()` as the loop does. An input is
`[code byte][status byte][raw stream]`; `StreamFuzz.h` describes it and the switches.

- **Three deliveries, one result.** Each input is delivered three times (the game target once more,
  through the production gate), to a fresh session: whole, one byte per receive, and in 1-to-48-byte
  chunks cut by a hash of the input, with the loop run after every receive; a target aborts unless
  the three read the same packets and end the same way, so a loop that depends on where TCP cut
  the bytes is a crash. The game target also links the production `GameFrameGate`, delivers every
  input byte by byte through it, and aborts unless it ends as the mirror's whole delivery did.
- **What counts as a crash.** Any exception but a `ProtocolException`, unless
  `DE_FUZZ_ALLOW_EXCEPTIONS=1`; and always a `readPacket` that returns or refuses without moving
  the stream exactly the frame its header declared ([frame gate](frame-gate.md)).
- **Switches.** The game target's validator is the gameserver's, so in `GPS_NORMAL` it reads only
  what a client sends and refuses the rest of the factory table before the read
  ([client link](client-link.md)); `DE_FUZZ_ANY_ID=1` admits every registered id there instead,
  so the GC, GS and SG reads behind that gate are fuzzed too, and `DE_FUZZ_NO_STORE_SKIP=1` reads
  the two store-info packets the gameserver refuses unread (with the gate open, since the
  validator refuses them first). The gate check is off under both, since the gate has no hook for
  them, so a gate that drifts from its mirror is caught only without them.

# In the suite

The zig suite builds the targets as `fuzz_replay_game` and `fuzz_replay_login`, a plain `main()`
over saved inputs, and ctest runs them (and `fuzz_replay_game_any_id`) over a seed corpus that
`tools/fuzz/golden2corpus.pl` writes from `tests/golden/` into the build tree, plus
`tests/fuzz/regressions/` (`.hex` files; every input that ever found a crash goes there with its
fix). `make dev-test` runs them like any other test. There is no CI fuzz job yet.

# Running libFuzzer

libFuzzer needs a Clang with compiler-rt, which `zig c++` is not, so the fuzzers are a separate
configure with `-DDARKEDEN_BUILD_FUZZERS=ON` (it refuses zig, AppleClang and a Clang that cannot
link a `-fsanitize=fuzzer` target, and instruments every target with ASan, UBSan and
`_GLIBCXX_ASSERTIONS`). `tools/fuzz/Dockerfile.fuzz` is the toolchain:

```bash
docker build -f tools/fuzz/Dockerfile.fuzz -t darkeden-fuzz tools/fuzz
docker run --rm -it -v "$PWD:/repo:ro" darkeden-fuzz bash
# in the container: build off a copy of the inputs, never the mount
mkdir /src && cp -a /repo/CMakeLists.txt /repo/cmake /repo/src /repo/tests \
    /repo/third_party /repo/tools /src/
cmake -G Ninja -S /src -B /build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 \
    -DDARKEDEN_BUILD_FUZZERS=ON -DDARKEDEN_OUTPUT_ROOT=/out
cmake --build /build --target fuzz_game_stream fuzz_login_stream \
    fuzz_replay_game fuzz_replay_login fuzz_seed_corpus
# run from a scratch directory: Assert appends to assertion_failed.log there
mkdir -p /scratch /corpus && cd /scratch
ASAN_OPTIONS=abort_on_error=1 /out/bin/fuzz_game_stream \
    -max_total_time=300 -timeout=10 -rss_limit_mb=2048 -max_len=32769 \
    -close_fd_mask=3 /corpus /build/tests/fuzz/corpus/game /src/tests/fuzz/regressions/game
```

`-close_fd_mask=3` keeps the packets' debug output off the terminal; `-fork=N -ignore_crashes=1`
keeps going past the first crash; with `DE_FUZZ_ANY_ID=1` in the environment the run fuzzes every
read in the gameserver's table. Leak detection is on: a read owns every record it allocates
(six GC reads once leaked theirs, recorded in [the fix log](../reviews/FIXES.md)), and the all-ids
replay is the test that keeps it so. Replay a crash file with `fuzz_replay_game <file>` in either
build; in the fuzz build it comes with the sanitizer's report (`ctest -R fuzz` in `/build` runs
the replay tests).
