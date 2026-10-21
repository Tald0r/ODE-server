---
type: Rule
title: "Frame gate"
description: "A packet's read() sees exactly the body its header declares, and the gameserver checks the sequence byte only where the frame is consumed."
tags: [protocol, security]
enforced_by: [wire_tests, game_frame_gate_tests, fuzz_replay_game, fuzz_replay_login, fuzz_replay_game_any_id]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Frame gate

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| A client's bytes reach a packet's `read()` only through the receive loops' gates, and a malformed body is refused only with a `ProtocolException` | the packet-read fuzz targets in `tests/fuzz/`; their replay ctests `fuzz_replay_game`, `fuzz_replay_login` and `fuzz_replay_game_any_id` run every golden seed and every input in `tests/fuzz/regressions/`, and the targets abort on any other exception; and the gtests in `tests/packet_read_bounds_test.cpp` (`wire_tests`), which pin each fix's refusal or value | the input, named, aborting the replay (a zig Debug UB trap, an assertion, or a non-protocol exception); the gtest, named, with the refusal or value it expected |
| A packet's `read()` sees exactly the body its header declares: `SocketInputStream::readPacket` bounds every read, peek and skip to it, refuses (with `InvalidProtocolException`) a body left unread, overrun or failed inside `read()`, and leaves the stream at the next frame on every exit | `tests/packet_frame_test.cpp` (`FrameBoundTest` in `wire_tests`), whose golden sweep reads every golden a server reads as one frame; the fuzz targets' body oracle | the case, named; the golden, named, with the bytes declared and consumed; the fuzz input, aborting the replay |
| The gameserver checks and counts a client's sequence byte only where the frame is consumed, so a frame split across TCP receives is read as if it had arrived whole, and a whole frame out of sequence still ends the connection with `DisconnectException("Packet sequence error")`, logged to `SequenceError.txt` | `game_frame_gate_tests` (`tests/game_frame_gate_test.cpp`), over `GameFrameGate` (`src/server/gameserver/GameFrameGate.h`), the gate `GamePlayer::processCommand` runs each frame through; the fuzz targets, whose three deliveries of every input hold the mirrored loops to one result wherever the bytes are cut, and whose game target delivers every input byte by byte through `GameFrameGate::next` itself | the case, named, with the step or refusal it expected; the fuzz input, aborting the replay with the delivery and the first difference |

The gate owns each case's step and refusal, the log file and the exception each refusal ends in,
not each log line's format, which `GamePlayer::refuseFrame` keeps and only reading checks. The
gate has no hook for `DE_FUZZ_ANY_ID` or `DE_FUZZ_NO_STORE_SKIP`, so the byte-by-byte check is
off under those switches. Some recorded inputs fail only under ASan: the zig Debug build does not
trap on every out-of-bounds read (`CGConnect-name-table.hex` is one), which is why each fix is also
pinned as a gtest.
