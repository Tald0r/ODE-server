# Fix log

Real bugs uncovered by the restructuring work (task 5.3 in
`docs/RESTRUCTURING.md`), recorded instead of fixed silently. Sidecar's
convention: every entry has a `> **Status:**` line updated in the same
commit as the fix.

The Exchange-reconcile defect set (SQL injection, string size/body
desync, `StringStream` stack overflows, unbounded listing counts, …) was
found before this file existed; task 1.4 in `docs/RESTRUCTURING.md` keeps
the rules each one left behind and the items still open, and the fixes
themselves are in the `restructuring/exchange-reconcile` branches of this
repo and the client's. Entries below are newest first; the oldest is the
1.4 max-size reconcile that followed it.

## Packet reads bounded by their frame (2026-09-30)

The frame bound itself is under "Packet-read fuzzing" below, where it was
first recorded.

- **A receive loop's `IgnorePacketException` catch covers the dispatch
  too.** `SharedServerClient::processCommand` wraps `readPacket` and
  `PacketDispatcher::dispatch` in one `try` whose
  `catch (IgnorePacketException&)` skips `szPacketHeader + packetSize`
  bytes, and `LoginPlayer::processCommand` does the same. After the read
  that frame is already consumed, so a handler that threw the exception
  would make the loop skip the next frame instead, or wait on it
  (`SharedServerClient` breaks out while fewer bytes are buffered).
  `GamePlayer` is not exposed: its dispatch turns any exception into a
  `DisconnectException`. Nothing throws it there today:
  `IgnorePacketException` comes only from `PacketIDSet::hasPacketID` for
  a `PIST_IGNORE_EXCEPT` status, reached only through
  `PacketValidator::isValidPacketID`, which only `GamePlayer` and
  `LoginPlayer` call, before the read, and no handler calls. A
  `read()` that threw it is refused by `readPacket` as an
  `InvalidProtocolException`. The catch in `SharedServerClient`, which
  never consults the validator, cannot be reached at all. Left as it is;
  a handler that starts to throw `IgnorePacketException` would need the
  catch narrowed to the checks before the read.
  > **Status:** not a defect (no handler throws it; recorded for the
  > next change to the loops)

- **`GamePlayer` counted a frame's sequence byte before its body had
  arrived.** `GamePlayer::processCommand` peeked the header, checked the
  sequence byte against `m_Sequence` and incremented `m_Sequence`, and
  only then checked that the whole body was buffered, breaking out of the
  loop if it was not. The frame stayed in the stream, so the next call
  peeked the same header, whose sequence was now one behind, and threw
  `DisconnectException("Packet sequence error")`. The
  `IgnorePacketException` catch had the same order: the sequence was
  counted before the validator threw, and a body not yet buffered ended in
  `InsufficientDataException`, so the ignored frame was re-peeked with a
  stale sequence too. The client numbers every packet it sends
  (`SocketOutputStream::write` increments its `m_Sequence`), so any game
  packet whose header arrived in one receive and its body in a later one
  disconnected the player. The loginserver checks no sequence, and the
  frame bound did not change this: `readPacket` consumes nothing of a
  frame that is not whole. `fuzz_game_stream.cpp` mirrored the same order
  but gave each input to the stream whole, so a fragment was only ever the
  input's end and the fuzzing could not reach this. Now `processCommand`
  runs each frame through `de::GameFrameGate`
  (`src/server/gameserver/GameFrameGate.{h,cpp}`), which owns the count
  and checks and counts the sequence only where the frame is consumed:
  just before `readPacket`, once the whole body is buffered, and right
  after the `skip` of an ignored frame, whose sequence is still checked. A
  wrong sequence on a whole frame is refused as before, with the same log
  line and exception. The log file and the exception each refusal ends in
  now come from the gate too (`de::refusalLogFile`, `de::throwRefusal`),
  and `game_frame_gate_tests` checks both for every refusal; each log
  line's format stays in `GamePlayer::refuseFrame`, since its fields name
  the player, and is checked by reading only. The id, validator,
  store-info and size checks keep their order and now come before the
  sequence check; a frame they refuse no longer has its sequence counted,
  and one that is also out of sequence is logged as the header refusal,
  neither of which matters because every refusal drops the connection.
  `GamePlayer.cpp` links only into the gameserver executable, which no
  test links, so the gate was split out to be tested:
  `game_frame_gate_tests` (`tests/game_frame_gate_test.cpp`) hands it
  frames in parts (a header and then its body, one byte at a time, the end
  of one frame with whole ones behind it, an ignored frame split in
  `GPS_WAITING_FOR_CG_READY`). Against the old gate 6 of its 18 frame
  cases fail (a 19th, which checks each refusal's log file and exception,
  came later). Four of them show this bug, the partial body counting the
  sequence and the whole frame then being refused: the fragmented read,
  the byte-by-byte delivery, the end of a frame followed by whole frames,
  and the fragmented ignored frame. The other two pin the new check order
  and fail on the old gate for that reason: a fragmented frame out of
  sequence now waits for its body before it is refused, and an id past the
  table is refused as such rather than for its sequence. The fuzz targets
  now deliver each input whole, byte by byte and in hashed chunks and
  abort unless all three read the same packets and end the same way; on
  the old order the game mirror aborted on the first golden seed, and a
  libFuzzer run from an empty corpus crashed in 0.7 s on a header
  declaring a four-byte body of which only the first byte arrives
  (`tests/fuzz/regressions/game/sequence-before-body.hex`). The game
  target also delivers every input byte by byte through
  `GameFrameGate::next` itself and aborts unless it ends as the mirror's
  whole delivery did, so the fuzzing covers the production gate and not
  only its mirror (outside `DE_FUZZ_ANY_ID` and `DE_FUZZ_NO_STORE_SKIP`,
  which the gate has no hook for). With the old gate built in,
  `fuzz_replay_game` aborts on the first golden seed and libFuzzer crashes
  in 0.13 s from the seeds. On the fix, 661 s of game fuzzing (621,395
  runs) and 181 s of login fuzzing found nothing, and with the production
  gate checked beside the mirror, 631 s of game fuzzing (716,961 runs) and
  91 s of login fuzzing found nothing either.
  > **Status:** fixed (fix/sequence-after-body)

## The client links read packets no client sends (2026-09-29)

- **In `GPS_NORMAL` the gameserver read any packet it registers.** The
  validator's in-game set was `PIST_ANY`, and the gameserver's factory
  table (`Concat<GameOnlyFactories, ClientLinkFactories,
  GuildLinkFactories>`, 416 factories) holds the GC packets it sends and
  the GG, GL, LG, GM, GS and SG packets of its server links as well as
  the CG packets a client sends. A logged-in client could make
  `GamePlayer::processCommand` create and `read()` any of them. The
  gameserver has no handler for most, so the dispatch disconnected the
  client, but only after the read, and the reads are where the fuzzing
  found the six leaking GC reads and the list reads that lose a record
  when it throws (under "Packet-read fuzzing" below). `GPS_NORMAL` now
  admits exactly the registered packets a game client sends on the game
  connection, 146 of the 416, folded from the factory table while
  compiling (`src/Core/GameClientLink.h`, `PacketFactoryManager.cpp`):
  every CG packet but `CGPortCheck`, which the client sends only as a UDP
  datagram (on the game connection its `read()` refuses the body anyway,
  so its handler never ran there), plus the three GC-named packets the
  live client sends server-ward and the gameserver has handlers for:
  `GCFriendChatting` and the no-op `GCAddStoreItem` and
  `GCRemoveStoreItem`, each of which the client repo builds and sends.
  `GCCannotUse`, which the client only receives, is refused too, and its
  no-op handler registration is gone. Every other id is refused with the
  "invalid packet order" disconnect before its read. A new CG packet is
  admitted once it is registered; a new client-sent GC packet needs its
  id in `GameClientLink.h`. The gameserver's `kReceivedDirections`
  narrows its GC link to that list (`DirectionSet::narrowed`), so the
  registration macros refuse to compile a GC handler the client link
  would never deliver. The store UI's `GCMyStoreInfo`
  and `GCOtherStoreInfo`, which the client does send, were refused unread
  by `GamePlayer` and are now refused by the validator first, with the
  same exception. The other `GPS_*` states already listed only CG ids.
  Pinned by
  `game_client_link_tests`, which enumerates the gameserver's factory
  table and checks every id against an oracle written from the packet
  names; on the old validator it failed, `GPS_NORMAL` admitting 269
  registered ids no client sends (270 counting `GCCannotUse`, which
  first stayed admitted and was dropped once the client repo showed no
  send of it).
  The fuzz harness uses the real validator, so `fuzz_replay_game` now
  stops the GC, GS and SG seeds and regression inputs at the gate, and
  `DE_FUZZ_ANY_ID=1` opens it again for `fuzz_replay_game_any_id`, so
  every recorded input still reaches its read. A five-minute libFuzzer
  run of the game target from the seed corpus (`-seed=1`, the command in
  `CLAUDE.md`) shows what a client can still reach: before the change
  it started at 16,213 covered edges from 1,169 useful seeds and ended at
  17,554 (26,715 features) after 3.6 million inputs; after it, 4,546
  edges from 443 seeds and 5,121 at the end (6,694 features) after 8.6
  million. Neither run crashed. Those runs predate dropping
  `GCCannotUse`, one no-op read, and were not repeated for it.
  > **Status:** fixed (fix/client-link-packet-ids)
- **The loginserver read a gameserver's `GLKickVerify` from a client.**
  `CLLoginHandler` for an account whose character is still on a
  gameserver calls `sendLGKickCharacter`, which sets
  `LPS_WAITING_FOR_GL_KICK_VERIFY` while `LoginPlayer::processCommand`
  is still in its read loop. In that status `processCommand` returns
  before reading anything, so the status's set is consulted only for
  packets the client pipelined behind that `CLLogin`, in the same pass.
  The set skipped all of them except `GLKickVerify`, which it read and
  dispatched. So a client with valid credentials for such an account
  could send `CLLogin` and `GLKickVerify` in one segment. The
  gameservers send it as a datagram to `GameServerManager`'s socket, so
  only a hostile client sends it on a `LoginPlayer`'s connection. Its
  handler runs there inside `LoginPlayerManager::processCommands`, which
  holds the manager's mutex: `GLKickVerifyHandler`'s
  `loginPlayers.lock()` throws on the same-thread relock, and its
  `catch (Throwable&)` then unlocks the mutex the loop still holds, so
  the rest of that pass runs unlocked against the datagram thread and
  the loop's guard unlocks it a second time. Found by reading the code;
  not reproduced on a running server. The set now admits nothing, like
  `LPS_AFTER_SENDING_LG_INCOMING_CONNECTION`'s, so the pipelined packets,
  `GLKickVerify` included, are skipped unread; bytes that arrive later
  still wait unread until the status changes, as before. Every
  other `LPS_*` state lists only CL ids and `CGConnectSetKey`. Pinned by
  `login_client_link_tests`, which failed on the old set.
  > **Status:** fixed (fix/client-link-packet-ids)

## Packet-read fuzzing (2026-09-29)

The fuzz targets in `tests/fuzz/` feed a client's bytes through the
gates of `GameFrameGate::next` (run by `GamePlayer::processCommand`) and
`LoginPlayer::processCommand` into the packets' `read()`; `CLAUDE.md`
says how to build and run them.
Where a fuzz run found a defect below, its input is replayed by ctest from
`tests/fuzz/regressions/`, and every fix but the validator's has a gtest
in `tests/packet_read_bounds_test.cpp` (`wire_tests`).

- **`StoreInfo::read` indexed its item vector with a count off the
  wire.** The stall record has `MAX_ITEM_NUM` (20) slots and `write()`
  sends them all, but `read()` looped over a wire BYTE of up to 255,
  writing `m_Items[i]` past the vector. It now refuses a count past the
  slots with `InvalidProtocolException` before reading any item
  (`StoreInfoTest` in `tests/packet_read_bounds_test.cpp`). On the
  gameserver it was not reachable: `GameFrameGate::next` (run by
  `GamePlayer::processCommand`) refuses `GCMyStoreInfo` and
  `GCOtherStoreInfo` unread, and even with that refusal off
  (`DE_FUZZ_NO_STORE_SKIP=1`, fuzzed for five minutes) the
  factory's packets hold no record, so their `read()` throws "no store
  record" before `StoreInfo::read`. So no recorded input reaches the
  bound and `StoreInfoTest` is its only guard; the ctest
  `fuzz_replay_game_any_id` replays the store-info seeds with the
  refusal off, which checks that refusal, not the bound. The gameserver's
  client link now refuses both packets before `GamePlayer`'s own refusal
  (see "The client links read packets no client sends"), so
  `DE_FUZZ_NO_STORE_SKIP=1` reads them only together with
  `DE_FUZZ_ANY_ID=1`, as that ctest runs. The refusal in
  `GamePlayer` stays for now. The client has the identical loop and reaches it; it is fixed
  there on the client repo's `feat/packet-fuzzing` branch (commit
  0c782d10).
  > **Status:** fixed (feat/packet-fuzzing)
- **`PacketValidator`'s constructor filled a vector it had only
  reserved.** `m_PacketIDSets.reserve(PLAYER_STATUS_MAX)` followed by
  `m_PacketIDSets[i] = NULL` wrote into capacity, not elements, and every
  later lookup read it the same way: undefined behaviour on every
  server's start-up that happened to work. The fuzz build's
  `_GLIBCXX_ASSERTIONS` aborted in the constructor. It now assigns
  `PLAYER_STATUS_MAX` null slots.
  > **Status:** fixed (feat/packet-fuzzing)
- **Debug strings indexed name tables with wire values.** Fourteen
  `toString()` bodies wrote `PCType2String[m_PCType]`,
  `HelmetType2String[getHelmetType()]` and the like, and
  `SocketInputStream::readPacket` prints every packet it reads, so one
  out-of-range byte (CGConnect's character type, CLCreatePC's slot,
  GCAddSlayerCorpse's helmet) copied a `std::string` that is not there.
  `PCVampireInfo`'s slot is one of them, reached only through `LCPCList`,
  which only the loginserver registers and no client status admits.
  Every lookup by a stored value now goes through `nameOrNumber()` or a
  range check beside it (`GCChangeWeather`, `GCUpdateInfo`'s weather, and
  `sex2String()`, `dir2String()`, `modifyType2String()`).
  `nameOrNumber()` (`src/Core/types/SystemTypes.h`) prints such a value as
  its number (`DebugNameTest` in `wire_tests`). The zig Debug build does
  not trap on the old lookup: it reads the neighbouring object and prints
  what that holds (an empty name for CGConnect's type 3, "FEMALE" for
  `PCVampireInfo`'s slot 3), so the recorded input
  (`tests/fuzz/regressions/game/CGConnect-name-table.hex`) fails only
  under the fuzz build's ASan, and `DebugNameTest` is what guards the fix
  in the zig suite.
  > **Status:** fixed (feat/packet-fuzzing)
- **Shop and stash listings wrote their slots by a wire index.**
  `GCShopList`, `GCShopListMysterious` and `GCStashList` stored each item
  at an index byte (and, for the stash, a rack byte) with no bound: 255
  against 20 slots and 3 racks, a heap overflow past the packet
  (`RackSlotTest`).
  > **Status:** fixed (feat/packet-fuzzing)
- **A wire bool could hold any byte.** `read<bool>` copied the raw byte,
  so 0x02 became a bool that is neither true nor false; the zig Debug
  build traps on loading it. It now reads the byte and stores
  `byte != 0`; writers send 1 for true, so no golden changes
  (`WireBoolTest`).
  > **Status:** fixed (feat/packet-fuzzing)
- **A repeated script parameter name stopped the gameserver.**
  `GCNPCAskVariable::read` let `addScriptParameter`'s
  `DuplicatedException` out. The receive loops catch only
  `ProtocolException`, so it left `ZonePlayerManager`, `ZoneGroupThread`
  rethrew it and the worker's failure called `ServerShutdown::fail()`. The
  read now refuses the repeat as an `InvalidProtocolException`
  (`ScriptParameterTest`).
  > **Status:** fixed (feat/packet-fuzzing)
- **Non-protocol exceptions from a read escape the receive loops.** The
  fix above closes the one the fuzzers found, not the route:
  `ZonePlayerManager::processCommands` and `IncomingPlayerManager` catch
  only `ProtocolException`, and the loginserver's `LoginPlayerManager`
  adds only `ConnectException`, so any other `Throwable` a packet read
  throws (an `Error`, an `AssertionError`, a `RuntimeException`) leaves
  the loop that runs it: on the gameserver a zone thread, whose failure
  calls `ServerShutdown::fail()`; on the loginserver `ClientManager::run`,
  which catches nothing, on the main thread. The fuzz targets, and so
  the replay ctests, abort on any other exception, so a recorded input
  that throws anything else fails them; the loops themselves are
  unchanged.
  > **Status:** recorded, not fixed (feat/packet-fuzzing)
- **Packets cast wire bytes to their enum types.** Reads store a byte
  straight into an enum (`m_PCType = PCType(pcType)`) and getters build
  one from bits (`ShieldType((m_Outlook >> n) & mask)`) without checking
  the enumerators. A value past the enum's range is undefined behaviour to
  load; the zig Debug build traps on it, so on a Debug server one such
  byte from a client aborts the process, while the optimized builds carry
  no check and Clang does not assume enum ranges without
  `-fstrict-enums`. It is a wave (the fuzz build turns `-fsanitize=enum`
  off so it does not hide everything else); each field needs its range
  checked where it is read.
  > **Status:** recorded, not fixed (feat/packet-fuzzing)
- **`GCUpdateInfo::read` leaks two records, plus one per NPC record.**
  Every read allocates one `NicknameInfo` (`GCUpdateInfo.cpp:175`), one
  `BloodBibleSignInfo` (:184) and one `NPCInfo` for each record its
  NPCInfoCount byte announces (:159-165). The sender picks that count,
  0 to 255: the factory's max size budgets all 255 records
  (`GCUpdateInfoFactory::kMaxSize`), and a nameless one costs one body
  byte. `~GCUpdateInfo` frees none of them. It only clears the NPC list,
  because on the writing side `PacketUtil.cpp` installs records that the
  creature and the zone own. Each record's own heap goes with it: the
  sign list's buffer, and a custom nickname or an NPC name longer than
  the string's inline capacity. A record whose read throws is lost as
  well: an `NPCInfo` that throws never reaches `addNPCInfo`, and the
  nickname and sign records are installed before they are read. So a
  read that reaches the end leaks 2 + N records and their buffers, N
  being the count byte. In the fuzz build (aarch64, libstdc++) the three
  records take 48, 32 and 40 bytes, so a read leaks 80 + 40N bytes
  before those buffers, up to 10,280 at N = 255. (This entry and commit
  c79da7b4 used to count only the first two records, 48 + 32 bytes.)
  A hostile client reached it on the gameserver until its client link
  was narrowed to what a client sends (see "The client links read
  packets no client sends"): `GCUpdateInfoFactory` is in the
  gameserver's table (`GameOnlyFactories`), and `GPS_NORMAL` admitted
  any registered id (`PIST_ANY`). The client link now refuses
  `GCUpdateInfo` before the read. The one other gameserver link that
  reads any registered id is `SharedServerClient`, whose peer is the
  sharedserver, so no client input reaches the leak any more; the read
  keeps it, and a change that admitted the id again would bring it back.
  The path it
  took: `GamePlayer::processCommand` reads the body in full and puts the
  packet in its history (`GamePlayer.cpp:394-404`) before it dispatches.
  The gameserver registers no `GCUpdateInfo` handler, so
  `PacketDispatcher::dispatch` throws, and the loop turns that into a
  `DisconnectException` (:433-438). `~GamePlayer` then deletes the
  packet and the records stay behind. A body that fails to read is
  deleted at once instead, by the `unique_ptr` at :394, with the same
  result. That was one leaking read per game session: a client that had
  logged in and sent `CGReady` leaked up to 257 records per connection
  and had to reconnect to send the next.
  ASan's leak check reports it in the fuzz build when the gate is open
  (`DE_FUZZ_ANY_ID=1`). The documented fuzz runs and the replay ctests
  in a `DARKEDEN_BUILD_FUZZERS` tree turn leak detection off
  (`ASAN_OPTIONS=detect_leaks=0`) for it and for the reads in the next
  entry. The fix is one ownership rule for the whole
  set: a written packet borrows its records and a read one owns them.
  `GCNPCInfo` (`m_OwnsNPCInfos`), `GCModifyNickname`
  (`m_bOwnsNicknameInfo`) and `GCBloodBibleSignInfo` (`m_bOwnsInfo`,
  with `clearSignInfo()`, the closest to the leaking sign record)
  already carry an owner flag each.
  > **Status:** recorded, not fixed (feat/packet-fuzzing) — unreachable
  > from a client since fix/client-link-packet-ids.
- **Five more GC reads leak the records they allocate.** With leak
  detection on, the game replay over the seed corpus and
  `tests/fuzz/regressions/game/` with the gate open (as
  `fuzz_replay_game_any_id` runs it) reports them beside `GCUpdateInfo`.
  `GCAddSlayer`, `GCAddVampire` and `GCAddOusters` leak
  their `NicknameInfo` on every read, and their `PetInfo` unless its
  type is `PET_NONE`, which the read deletes; their destructors free
  only the effect record and leave both to the creature, as
  `GCUpdateInfo` does. `GCPetInfo` leaks its `PetInfo` on every read,
  since its destructor frees nothing. `GCPetStashList` leaks one
  `PetInfo` per occupied slot, up to 20: its destructor frees each
  slot's `PetStashItemInfo` but not the pet in it.
  All five are in the gameserver's factory table and have no gameserver
  handler, so a client reached them the way it reached `GCUpdateInfo`,
  and the narrowed client link refuses them the same way.
  The login replay reports no leak. Only the replay inputs have been
  checked. A libFuzzer run with leak detection on has not been done.
  The list is the reads that leak when the read succeeds. A read that
  throws part-way leaks more widely: many list reads allocate a record,
  read it and only then add it to the list (`new X; x->read(); push_back`,
  e.g. `GCActiveGuildList.cpp:49-51`, and the same shape in
  `GCGuildMemberList`, `GCWaitGuildList`, `GCHolyLandBonusInfo`,
  `GCSweeperBonusInfo` and `GCGoodsList`), so a record whose own read
  throws is lost. An ownership rule for the six reads above does not
  cover that shape. Refusing on the gameserver the packet ids a client
  never sends closes both for client input, and the gameserver now does
  (all of these are GC packets no client sends); the reads themselves
  are unchanged.
  > **Status:** recorded, not fixed (feat/packet-fuzzing) — unreachable
  > from a client since fix/client-link-packet-ids.
- **`SocketInputStream::readPacket` was not bounded by the frame.** A
  packet's `read()` consumed what its fields said, not the size its
  header declared, and nothing checked that the two agree. A body
  shorter than declared left the rest to be parsed as the next header;
  a longer one read into the next frame; and a field that ran past the
  last buffered frame threw `InsufficientDataException`, which the
  receive loops take to mean "wait for more", with the frame half
  consumed. The fuzz harness's strict-body switch showed it on the first
  seed with a zero-filled body. `readPacket` now reads one frame, as the
  client's `SocketInputStream::read(Packet*)` does: it consumes nothing
  until the header and the whole declared body are buffered, bounds
  every read, peek and skip the packet's `read()` makes to that body
  (the tail is pulled in to the body's end while `read()` runs), and
  refuses with `InvalidProtocolException` a body left partly unread, a
  field that runs past it, a stream failure `read()` caught itself, an
  `IgnorePacketException` from `read()` and a nested `readPacket`. On
  every exit the stream stands at the next frame. What the refusal does
  in each receive loop: `GamePlayer::processCommand` lets it out to
  `ZonePlayerManager` (the player is kicked through `CGLogoutHandler`)
  or `IncomingPlayerManager` (disconnected and deleted);
  `LoginPlayer::processCommand` rethrows it from its
  `catch (InvalidProtocolException&)` and `LoginPlayerManager`
  disconnects the player; the sharedserver's `GameServerPlayer` lets it
  out to `GameServerManager`, which disconnects that gameserver; and the
  gameserver's `SharedServerClient` lets it out to
  `SharedServerManager`, which deletes the client and reconnects on a
  later tick. No loop resumes at the next frame. The server's reads
  were checked against every golden a server reads (653 frame goldens,
  and the 14 datagram goldens on their `Datagram` path) and consumed
  each exactly before the change as well; no read swallows a stream
  exception (no `catch` in any function that takes a
  `SocketInputStream&`). The live client's `write()` and
  `getPacketSize()` were compared field by field with the server's
  `read()` for every packet it sends on the two client links (163 CG and
  CL packets and the three client-sent GC packets), since a declared size
  the body does not fill used to pass and is now refused: none differs.
  Those after which the client hangs up or reconnects elsewhere, where a
  short read could have gone unnoticed (`CLSelectPC` before the handoff
  to the gameserver, `CLLogout`, `CGLogout`), match exactly, and so does
  `CLLogin`'s plain layout, whose golden differs between the repos only
  in its values. The client's Netmarble `CLLogin` layout (a four-byte
  length) is refused by the server's read before and after the change.
  Pinned by `FrameBoundTest` in `tests/packet_frame_test.cpp`
  (`wire_tests`) and by the fuzz targets' body oracle, which aborts
  unless `readPacket` moved the stream exactly one frame on. Two
  libFuzzer runs of 661 seconds each on the bounded read, from the
  committed seed corpus with strict exceptions (the command in
  `CLAUDE.md`), found nothing: the game target ran 21,072,354 inputs
  from 1,454 seeds, from 4,561 covered edges to 5,146 (7,009 features,
  1,175 corpus units); the login target 7,294,596 inputs from 253 seeds,
  from 856 edges to 985 (3,967 features, 435 units). The replay ctests
  pass in that ASan build too.
  > **Status:** fixed (fix/frame-bounded-packet-reads)

## The character list could not carry a cross1, a mace or a mace1 (2026-09-29)

- **A slayer's character-list outlook had four weapon bits for nineteen
  weapon shapes.** `PCSlayerInfo` (the record `LCPCList` carries, and the
  layout of the `Slayer.Shape` column) packs the outlook into one DWORD
  with the weapon at bits 11-14 and the shield at 15-16. `WeaponType`
  runs to `WEAPON_MACE1` = 18, so cross1 (16), mace (17) and mace1 (18)
  did not fit: `setWeaponType` wrote the value unmasked, so a mace's
  fifth bit landed on `SLAYER_BIT_SHIELD1` and it read back as weapon 1
  (a sword) with a shield. No production code called that setter; the
  live path, `Slayer::getShapeInfo`, clamped the shape through
  `slayerWeaponListShape` before saving it, so the list showed a cross1
  as a cross and a mace or mace1 as no weapon. Either way the
  character-select slot never showed the weapon the slayer held (the
  client's to-hit preview, commit ad559efb in the client repo, reads the
  domain level from it).
  The weapon is now two fields. Bits 11-14 keep the four-bit view: the
  weapon itself for 0..15 and the clamp's stand-in for 16..18 (a cross
  for cross1, no weapon for mace and mace1), so they hold exactly what
  the live path wrote before. Bits 17-18 (`SLAYER_BIT_WEAPON_EXT1..2`,
  past the shield; `SLAYER_BIT_MAX` 17 -> 19) hold an extension code: 0
  when the four bits are the weapon, otherwise the weapon minus 15 (1
  cross1, 2 mace, 3 mace1). `getWeaponType` reads `WEAPON_CROSS + code`
  when the code is set and the four bits otherwise, so every outlook
  decodes to 0..18. `PCSlayerInfo::weaponBits` writes both fields, so
  they cannot disagree; a value past 18 writes no weapon (both fields
  zero) and never touches the shield. `setWeaponType` writes through it,
  and so does the gameserver's encoder: `getShapeInfo`'s weapon block is
  now one call to `slayerListWeaponBits` (`SlayerListWeapon.h`), which
  takes the right-hand item's class and type to the shape
  (`slayerWeaponShape`) and the shape to `weaponBits`.
  `slayerWeaponListShape` is kept as the four-bit view
  (`types/SlayerWeaponShape.h` joined the kernel so `PCSlayerInfo.h` can
  use it), and `static_assert`s pin the enum values the code counts from
  and the round trip of every weapon.
  The outlook's other setters (hair style, helmet, jacket, pants, shield)
  now cut their value to their field through one helper,
  `setOutlookField`, and a `static_assert` checks that every value below
  each type's `_MAX` fits. Unmasked, `HelmetType`, which runs to
  `HELMET_MAX` = 4 and so holds 0..7, wrote 4..7 into the jacket's bits.
  The other types' values all fit their fields here: a shield of 4 or
  more, which unmasked would land on the extension code now that it sits
  past the shield, is not a `ShieldType` value, and the Debug build's
  UBSan stops on its load before any setter runs. The client's copy of
  the header is where the mask matters most: its `ShieldType` runs to
  `SHIELD4` and its `HelmetType` to `HELMET5`, so there `SHIELD4` would
  have written the extension code and `HELMET4`..`HELMET5` the jacket.
  Interop. Old client, new server: nothing changes. Such a client keeps
  bits 0-16 of the DWORD (`bitset<17>`), which are bit-identical to what
  the live path sent before for every weapon and shield, so it still sees
  a cross1 as a cross and a mace or mace1 as no weapon.
  New client, old server: bits 17-18 are never set, so it reads the four
  bits, the clamped values, as before. The same holds for `Slayer.Shape`
  rows saved before this, until the character is saved again; the column
  is int(10) unsigned and holds bit 18 without a migration.
  Pinned by `PCSlayerInfoOutlook` in `tests/packet_login_test.cpp`: the
  whole DWORD of every weapon with every shield against literal tables
  (the four-bit view, extended from 0..15 to the clamped values for
  16..18, and the extension code), an old client's reading of it, an
  outlook without the code, every combination of the two fields, the
  values past 18, and every value each other field's type can hold,
  written under every weapon (`everyValueAFieldsTypeHoldsStaysInTheField`).
  The gameserver's encoder is pinned by `SlayerListWeapon` in
  `tests/slayer_list_weapon_test.cpp` (run by `player_race_tests`, which
  compiles it header-only): literal DWORDs for every weapon class on each
  side of its item-type thresholds, so a cross1, a mace (type 13) and a
  mace1 (type 14) carry their code; no bits for a class that is not a
  slayer weapon; and no bit outside the two weapon fields for any class
  and type. `LCPCList.code0.hex` (a cross) did not move and the
  packet's size and inventory line are unchanged; `LCPCList.mace.code0.hex`
  pins a mace, whose outlook differs from the cross's in bits 11-14 and
  18 alone (0x00017c9d -> 0x0005049d). The zone view's `PCSlayerInfo3`
  (`GCAddSlayer`, `GCAddSlayerCorpse`, `GCMorphSlayer2`) already had five
  contiguous weapon bits; its setter now masks to them, which moves no
  byte. Its other setters are still unmasked: their types fit their
  fields there except the motorcycle's (`MOTORCYCLE_MAX` = 4, so 0..7, in
  two bits) and the shoulder's `BYTE` (two bits), whose callers pass
  `MOTORCYCLE_NONE`..`MOTORCYCLE3` and 0 or 1, so nothing spills today.
  `PCSlayerInfo2` and `CLCreatePC` carry no weapon.
  > **Status:** fixed (fix/slayer-weapon-bits)

## Shared small rules (2026-09-29)

Three small rules moved into de-core in slice 6b of the shared-rules plan:
a slayer skill's range (`src/domain/SkillRange.cpp`, which the client
vendors), the party experience bonus and the dark and light levels a
player is sent (server-only). The moves are verbatim; what they kept and
what they changed:

- **The skill range depended on the build machine's CPU.**
  `computeSkillRange` (`skill/SkillUtil.cpp`) computed
  `min + (max - min) * (level * 0.01)` in the gameserver, which is built
  without `-ffp-contract=off`, so Clang fused the multiply and the add
  wherever the target CPU has a fused multiply-add: every arm64 target, and
  x86-64 with FMA, which a Zig build without a target gets from the build
  machine. A fused result within an ulp below an integer truncates one
  lower: minimum 5, maximum 0 at level 80 was 0 there and 1 elsewhere.
  `decore::skillRange` is built without contraction, like the rest of
  de-core, so it gives the unfused value, a baseline x86-64 build's, on
  every target. It changes nothing a live server computes: for every
  `SkillBalance` seed range pair at levels 0 to 100, and for every minimum
  not above the maximum (0 to 255) at levels 0 to 100, the fused and
  unfused results agree. They differ only when the byte-narrowed maximum
  is below the minimum, at any level (4,724 of those 3,296,640 inputs at
  levels 0 to 100; the seed's two such skills, Raising Dead and Summon
  Servant, are vampire skills and never reach it). The evidence: master's body and the
  new call, compared over minimum and maximum 0 to 600 and six larger
  values up to 2^32-1, and levels 0 to 600, 1023 and 65535 (222,174,747
  inputs), give 0
  mismatches built without contraction (Apple Clang arm64, -O0 and -O2;
  Clang 18 x86-64, -O0 and -O2), and 412,797 mismatches, the same inputs
  on both, built with it (arm64 default, x86-64 `-march=haswell`). Pinned
  by the `no-fma-*` rows of `src/domain/vectors/skill_range.tsv`, which a
  fusing toolchain fails.
  > **Status:** fixed (feat/shared-small-rules)
- **The skill range's own oddities are kept.** The three inputs and the
  result are read modulo 256, as the server's 8-bit `Range_t` and
  `SkillLevel_t` held them, so a level of 300 is 44 and a range past 255
  wraps; the minimum is added before the truncation, so with a maximum
  below the minimum a partial step rounds down, away from the minimum
  (6 to 2 at level 30 is 4); and `level * 0.01` rounds low for some
  levels, so a span of 50 or more truncates one below the exact answer
  (0 to 100 at level 29 is 28). No seed skill spans more than 5. Pinned by
  the `wrap-*`, `max-below-min-*`, `level-29-*` and `level-58-*` rows.
  `skill/CrossCounter.cpp` writes the same interpolation four times, but
  for the counter's damage from the skill table's minimum and maximum
  damage, not for a range; it is a damage rule, left where it is, and
  whether its result also depends on contraction was not measured.
  > **Status:** recorded, not fixed (feat/shared-small-rules)
- **The five party shares split the raised experience three ways.** The
  five sharing functions in `Party.cpp` raised the experience by the same
  switch, now `decore::partyExpPool` (the move changes nothing: master's
  switch and the call agree on 39,908,322 amount and member-count pairs,
  amounts -100,000 to 1,000,000, 2^k +-3 on both signs, the int limits
  and 1,000,000 pseudo-random ones, counts -3 to 12, 100 and the int
  limits; 0 mismatches, also under UBSan). Each then gives a member its
  share of the pool in proportion to its level, and those splits differ:
  `shareAttrExp`, `shareVampireExp` and `shareOustersExp` compute
  `(int)((float)amount * (float)level / (float)LevelSum)`, so a pool past
  2^24 loses its low bits to the float; `shareRankExp` and
  `shareAdvancementExp` compute `amount * level / LevelSum` in int, which
  overflows when `amount * level` passes 2^31-1 (at level 150, a pool of
  about 14.3 million); and `shareAdvancementExp` sums each member's level
  plus advancement class level into `LevelSum` but shares by the level
  alone, so its shares add up to less than the pool. Whether a kill can
  grant a pool that large was not examined. The splits stayed in
  `Party.cpp`, unchanged.
  > **Status:** recorded, not fixed (feat/shared-small-rules)
- **The weather broadcast sent a vampire a wrapped dark level at night.**
  When a zone's dark or light level changes with the game clock,
  `WeatherManager::heartbeat` broadcasts `GCChangeDarkLight`: the zone's
  levels to each slayer, and to each vampire `DARK_MAX - dark` and
  `LIGHT_MAX - light` (13 and 15), without the clamps `makeGCUpdateInfo`
  applies (a dark level of at least 0, a light level of at most 13). The
  packet's fields are bytes, so a zone dark level above 13 reached a
  vampire as 256 minus the excess, and a zone light level below 2 as 14
  or 15. It was reachable with the seed data every game night: its
  `DarkLightInfo` table has dark level 14 (light 5) at 19:00 and 4:00 and
  15 (light 4) from 20:00 to 3:50 in every month, so each zone's
  broadcasts at 19:00, 20:00 and 4:00 sent vampires 255, 254 and 255. The
  light clamp was not: the seed's lowest light level is 4. The stock
  client ignores a dark level of 16 or more (`MTopView::SetDarkBits`), so
  a vampire kept the level it had, 0 from the 18:50 broadcast of dark 13
  or from the clamped `GCUpdateInfo` of its zone entry, which is what the
  clamp now sends. For a vampire without Flare nothing it sees changes,
  only the bytes. A vampire under Flare (`EffectFlare::affect` sends it
  dark 15) does see a change: the 19:00, 20:00 and 4:00 broadcasts now
  send dark 0, which the client applies, so Flare's darkness ends at that
  broadcast instead of lasting to the effect's deadline, as it already
  did at every other broadcast. The
  broadcast now calls `decore::darkLightForViewer` for a slayer and a
  vampire outside a castle and a PK zone (the heartbeat returns early
  elsewhere). The evidence: over every zone
  dark and light level 0 to 255, the slayer packet is unchanged and the
  vampire packet changes on exactly the 61,980 pairs with a dark level
  above 13 or a light level below 2, and on no other. The
  `weather-*` and `vampire-dark-*` rows of
  `src/domain/vectors/server/dark_light.tsv` pin the rule the broadcast
  now calls, not the broadcast: `WeatherManager::heartbeat` needs a
  running zone and nothing in `tests/` drives it, so nothing tests the
  call site.
  > **Status:** fixed (feat/shared-small-rules)
- **Not every dark and light message follows the viewer rule.** The rule
  `makeGCUpdateInfo` applies when a player enters a zone, now
  `decore::darkLightForViewer`, decides by the zone's type first (a castle
  shows its own levels, a PK zone 0 and 14) and then by race and effect.
  The other senders of `GCChangeDarkLight` each decide less:
  `EffectFlare::unaffect` sends a vampire the inverted levels even in a
  castle or PK zone (it now calls the rule as a vampire outside both, which
  is what it computed); the broadcast when a zone's levels change
  (`WeatherManager::heartbeat` through
  `CreatureManager::broadcastDarkLightPacket`, which runs only in a normal
  non-PK field) sends every slayer without Lightness or Yellow Poison the
  zone's own levels and every vampire the inverted ones, ignoring Flare
  for a vampire (a gap that predates this branch: a Flared vampire's
  darkness ends at the next broadcast), and sends an ousters nothing (it
  now calls the rule as a slayer and a vampire); and
  `EffectLightness::unaffect` and `EffectYellowPoisonToCreature::unaffect`
  send the zone's own levels whatever the race and the zone, so an ousters
  whose Yellow Poison wears off is shown the zone's levels in place of its
  fixed 13 and 6 until its next zone change, and a slayer in a PK zone the
  zone's levels in place of 0 and 14. (`EffectBloodDrain`'s ousters
  recovery sends 13 and the ousters' sight, at most 6, a rule of its own.)
  Making them all call the rule would change what those players see in
  castle and PK zones, and making the broadcast honor Flare would change
  what a Flared vampire sees; those are balance decisions.
  > **Status:** recorded, not fixed (feat/shared-small-rules)

## Vendorable skill output formulas (2026-09-29)

`src/domain/SkillOutputFormulas.cpp`, the per-skill `computeOutput`
bodies, joined the de-core subset the client vendors. One unchecked read
was made safe for the client's callers, and one server oddity it exposed
is recorded:

- **The party tables were indexed with no bounds check.** 23 reads in 18
  formulas (Bless, Striking, the cures, the protections, Revealer and
  others) indexed `PartyEffectBoost[7]` and `PartyDurationBoost[7]` with
  `input.PartySize` directly, so a size outside 0..6 read past the tables:
  master's `Bless` at size 7, built with AddressSanitizer, stops on a
  global-buffer-overflow read. The server never passes such a size. The
  four `SkillInput` constructors (`skill/SkillHandler.cpp`) set 1 for a
  slayer and an ousters and 0 for a vampire and a monster; nothing else
  in `src/` writes the field; the default constructor, which leaves it
  unset, is never used to call a formula, and the de-core adapter
  (`toFormulaInput`, `skill/SkillFormula.cpp`) copies the field from one
  of those four. A client caller could pass any int, so every read now
  goes through `partyEffectBoost` and `partyDurationBoost`
  (`SkillOutputFormulas.h`), which clamp the size to 0..6: below 0 reads
  0, above 6 reads 6. On 0..6 nothing changes: master and the new file
  agree on all 293 formulas over the scratch grid in the branch's first
  commit (1,655,145,280 comparisons, x86-64 and arm64), and every
  formula at sizes -2^31, -100, -2, -1, 7, 8, 100 and 2^31-1 gives what
  it gives at 0 or 6 (46,880,000 comparisons, under AddressSanitizer and
  UBSan too). Pinned by
  `SkillOutputFormula.PartySizeOutsideTheTablesReadsTheNearestEnd` in
  `tests/formula_test.cpp`, and by the `party-*` rows (sizes -2^31, -100,
  -1, 7, 8, 100 and 2^31-1 beside 0 to 6) and the `bless-*-party-*` rows
  of `src/domain/vectors/skill_output.tsv`.
  > **Status:** not a defect (the clamp is defensive, for the client;
  > feat/vendorable-skill-output)
- **No party bonus is ever granted.** Because every caller passes 0 or
  1, both tables read 100 and `getPercentValue(x, 100)` is `x`: the 23
  party-bonus reads are the identity on the live server, and the larger
  percentages for a party of 2 to 6 (up to 150% effect and 200%
  duration) are never applied. The real party size (`Party::getSize`)
  never reaches `SkillInput`. Passing it would be a balance change for 18
  skills; until then a client that shows these skills' numbers must pass
  0 or 1 as the server does, not its party's size.
  > **Status:** recorded, not fixed (feat/vendorable-skill-output)

## Shared castle tax (2026-09-29)

The castle tax the shop buy handler adds to a purchase moved from
`CGShopRequestBuyHandler::executeNormal` into de-core
(`decore::applyCastleTax`, `src/domain/ItemPrice.cpp`) so the client can
quote the same charge. The handler keeps the tax it logs and credits to
the castle. The move defined one conversion and kept the rest:

- **A taxed total past the int range was converted by undefined
  behaviour.** The handler computed
  `(int)(itemMoney * (itemTaxRatio / 100.0))` with `itemMoney` a 32-bit
  unsigned `Price_t`, so a product of 2^31 or more had no int. x86-64
  builds gave INT_MIN, which the handler stored back as a `Price_t` of
  2147483648; arm64 builds give INT_MAX, 2147483647; Zig's checked Debug
  build panics on either target ("outside the range of representable
  values of type 'int'"), so a Debug server aborted on such a purchase.
  de-core now returns 2147483648 on every target, x86-64's value. It is
  balance-neutral on both architectures: either value is more than
  `MAX_MONEY` (2,000,000,000), so the purchase is refused as not enough
  money before the tax is logged or credited. The seed data does not
  reach it at the ratios a guild master can set (101 to 110): at 110 it
  takes a total of 1,952,257,862 before tax, and the dearest seed item
  lists at 10,000,000. A ratio set by the GM command `ItemTaxRatio`
  can reach it. The evidence: the old block and the new call, built by
  the container's Zig for x86-64 at -O0 (without UBSan) and -O2 and run
  under emulation, give the same `itemMoney` and `itemTax` for 58,398,350
  total and ratio pairs (totals 0 to 20,000, 2^k +-3, the int and
  `Price_t` limits, the totals whose product straddles 2^31 at each
  ratio, 120,000 pseudo-random; ratios -2 to 400, 500, 999, 1000, 10000,
  100000 and the int limits), 0 mismatches. Built for arm64 they differ
  on exactly the 14,679,318 pairs whose taxed product is out of range.
  Pinned by `tax-first-past-int-at-110`, `tax-exactly-2-to-31-at-200`,
  `tax-int-max-at-101`, `tax-uint-max-at-101` and
  `tax-uint-max-at-int-max` in `src/domain/vectors/price.tsv`, beside
  `tax-last-in-int-at-110` and `tax-last-in-int-at-200` just inside it.
  > **Status:** fixed (feat/shared-castle-tax)
- **A ratio of 100 or below is no discount, but the shop tells the client
  it is.** The handler taxes only above 100, while `ActionSell` sends the
  player's ratio (`NPC::getTaxRatio`) as `GCShopVersion`'s
  `MarketCondSell` whenever it is not 100 (the normal and mysterious
  racks' only source of it) and `CGShopRequestListHandler` sends it as
  `GCShopList`'s and `GCShopListMysterious`'s, so a client pricing by that
  market condition quotes a discount the server does not give;
  `ActionBuy`'s `GCShopMarketCondition` writes the NPC's untaxed market
  condition into the same client field. The guild
  master's command sets 100 to 110; the GM command sets any int, and the
  wire's `MarketCond_t` is a 16-bit `short`, so a GM ratio outside
  -32768..32767 reaches the client truncated (40000 arrives as -25536,
  an untaxed quote for a x400 charge). A separate tax field (open
  decision 7) needs at least 32 bits. Pinned
  by `tax-ratio-90-no-discount`, `tax-ratio-0-untaxed` and the other
  `tax-*-untaxed` rows.
  > **Status:** recorded, not fixed (feat/shared-castle-tax)
- **The tax is taken once on the whole total, in double.** Three items at
  17 each cost 56 at 110 (51 x 1.1 = 56.1), where taxing each item would
  cost 54. The ratio is divided by 100.0 before it multiplies, and a ratio
  whose hundredth rounds low in binary truncates one below the integer
  answer: 100 at 115 costs 114. No ratio a guild master can set does that.
  Pinned by `tax-three-at-17-taxed-once` and the `tax-hundredth-low-*`
  rows.
  > **Status:** recorded, not fixed (feat/shared-castle-tax)

## Shared equip rules (2026-09-29)

The three races' item requirement (what an item asks of its wearer once
its options have raised it, and whether the wearer meets it) moved from
`Slayer`, `Vampire` and `Ousters::isRealWearing` into de-core
(`src/domain/EquipRequirement.cpp`) so the client can build the same
source. The move is verbatim, and it kept these:

- **The races raise and cap a requirement differently.** A vampire's
  options raise its level requirement even when the item table asks for
  level 0, while the slayer's and the ousters' options raise only a
  requirement that is not 0. The ousters' STR, DEX, INT and sum are not
  capped at all, and their level is capped at 150 whatever the table's
  level, where a vampire's is capped at 100 unless the table's is above
  100. Pinned by `vampire-zero-level-raised`, `ousters-attrs-not-capped`,
  `ousters-sum-not-capped` and `ousters-no-old-level-cap` in
  `src/domain/vectors/equip.tsv`.
  > **Status:** recorded, not fixed (feat/shared-equip-requirements)
- **A level requirement wraps at 256, and a requirement that wraps to 0
  stops rising.** The level is an 8-bit `Level_t` and each option is added
  to it before the cap, so a level-150 item with an option raising it by
  120 asks for level 14; STR, DEX, INT and the sum wrap the same way at
  65536, and a wearer's STR + DEX + INT wraps at 65536 before the
  comparison. For the slayer and the ousters, whose options skip a
  requirement of 0, a requirement that wraps to exactly 0 skips every
  option after it. The seed data reaches the level wrap only with options
  raising a level-120 item (the largest table level) by 136 or more, and
  the largest option raise is 50; the 16-bit wraps it does not reach.
  Pinned by the `*-wrap-*` rows.
  > **Status:** recorded, not fixed (feat/shared-equip-requirements)

## Shared stat rules (2026-09-28)

The client is about to call de-core's stat, skill MP and HP regen
functions, and its test suite asserts `src/domain/vectors/stats.tsv` on
every client toolchain, so the one piece of undefined behaviour in those
formulas is defined first:

- **The steal ratio narrowed a negative double to a byte.**
  `slayerStealRatio`, `vampireStealRatio` and `oustersStealRatio` computed
  `(Byte)(base - (float)amount * 1.4)`, which truncates below zero from
  amount 48 on the 65% base (a slayer with a cross, mace, gun or other
  weapon) and from 65 on the 90% base (bare hands, sword, blade, and every
  vampire and ousters). Converting such a value straight to an unsigned
  byte is undefined. x86-64 builds convert through a 32-bit int and keep
  the low byte, so production returned 255 at 90% and amount 65, 254 at
  65% and amount 48, and 245 at 90% and amount 255. The arm64 builds did
  not: a scratch build of the old source returned 0 at -O0 and the
  negative int itself at -O2, and Zig's checked Debug build traps on every
  target. The conversion is now `(Byte)(int)(...)`, which is defined and
  is what x86-64 did, so it is balance-neutral in production. The
  evidence: the old and new functions, built for x86-64 by the container's
  Zig at -O0 and -O2 and run under emulation, agree on every weapon family
  for amounts -300 to 1000 (11,709 calls, 0 mismatches); on arm64 they
  disagree on 1,787 of the 2,304 calls for amounts 0 to 255.
  `formula_tests` built at -O2 for x86-64 against the old `Formulas.cpp`
  passes every new row. The wrap is pinned by the
  `slayer-steal-<family>-<amount>`, `vampire-steal-<amount>` and
  `ousters-steal-<amount>` rows of `stats.tsv` and by
  `StealRatio.RatioBelowZeroWrapsToAByte`. Whether a steal ratio that
  wraps to 255 should instead be 0 is a balance question this change does
  not answer.
  > **Status:** fixed (feat/shared-stat-rules)

## Review follow-ups (2026-09-28)

Defects a review of the four preceding changes found in them or next to
them.

- **The relic holder announcement passed four arguments to a
  five-conversion format.** `EffectHasSlayerRelic::affect` and
  `EffectHasVampireRelic::affect` format GSStringPool rows 7 and 8,
  `"%s (%s) holds the ... Relic at %s (%d, %d)."`, with the name, the race
  and the coordinates but no zone, so the third `%s` read the x
  coordinate's argument slot as a string pointer and `%d` read past the
  arguments: undefined behaviour on every announcement while a player
  holds a relic. The mismatch is as old as the
  source; the legacy seed rows had the same three `%s`. The zone's full
  name is passed now, the race of a creature of no known race is `""`
  rather than a null `%s`, and the buffer holds the longest message a
  `GCSystemMessage` carries.
  > **Status:** fixed (fix/review-followups)
- **Three system messages were cut off at 49 bytes.** The `sprintf` to
  `snprintf` change kept their `char msg[50]`, which the seed data
  overflows, so they arrived truncated where they used to overrun the
  stack. The relic that leaves the relic table (`RelicUtil.cpp`, GSStringPool
  row 32, 42 fixed bytes) is announced to every zone group as `Relic
  (Rommels Tag) has appeared at the Relic Shr`, 53 and 61 bytes for the two
  seed relics; the transport countdown (`EffectTransportCreature.cpp`,
  `EventTransport.cpp`, row 14, 34 fixed bytes plus the seconds) cuts every
  zone name longer than about 13 bytes, and seed zone names run to 27.
  These and the other seven `char msg[50]` buffers filled from a
  GSStringPool format (`EventKick`, `EffectSlayerRelic`,
  `EffectVampireRelic`, four in `gm/ConsoleCommands.cpp`, none of which the
  seed data overflows) now hold `de::wire::kMaxByteStringLength + 1`
  bytes, the longest message a `GCSystemMessage` carries, so the string
  pool, not the buffer, bounds the text. So do the guild war's start, end
  and status messages (`war/War.cpp`, 80 bytes; `war/WarSystem.cpp`, 100):
  a guild war's name is the guild's name (up to 30 bytes on the wire),
  " guild attacks ", the castle and " (guild war)", which a long guild name
  pushes past both.
  > **Status:** fixed (fix/review-followups)

## Per-class grade policy (2026-09-28)

Which item classes keep a grade and a durability, and what a grade does to
each attribute, moved into de-core (`src/domain/ItemGrade.cpp`) so the
client can build the same table; `ConcreteItem` reads it instead of taking
grade and durability policies as template arguments. The table is the
server's choices as they were, and these are kept:

- **The grade policy of five classes is not what their neighbours use.**
  ShoulderArmor, Persona, Fascia and Mitten move with the grocery armor
  (defense `grade - 4`, protection `(grade - 4) / 2`, durability 500 a
  grade, no luck), not like the accessories or the cloth; CoreZap keeps a
  grade that moves nothing (`HasGrade`), though the three races' stat
  code reads it as the option-class level. Each is pinned by a `policy-<Class>` row in
  `src/domain/vectors/item_grade.tsv`.
  > **Status:** recorded, not fixed (feat/shared-grade-policy)
- **Five gear classes keep no durability.** Dermis, Fascia,
  CarryingReceiver, CoreZap and VampireAmulet read a durability of 1 and
  ignore a new one, while Fascia's grocery siblings and every other
  accessory track theirs. The server reads no durability for Dermis,
  Fascia, CarryingReceiver or CoreZap: their item infos are loaded without
  one (`loadGearInfosNoDurability` for the first three, `loadIntInfos` for
  CoreZap, whose table has no such column), so `ItemInfo::getDurability`
  reads 1 and their maximum before the durability options is 1, the value
  a client must pass as their table durability. The first three tables do
  have a `Durability` column, 0 in the seed data, which nothing reads.
  They sell at the full price. VampireAmulet's item info does read its
  table durability, 1000 to 24000, so its maximum is that and it sells at
  1 / maximum of its price, a few gold.
  Pinned by the `durability-<Class>` rows.
  > **Status:** recorded, not fixed (feat/shared-grade-policy)
- **A class not built on `ConcreteItem` reports a maximum durability of
  1.** The motorcycle, the relics, the castle symbols, the blood bibles
  and the sweepers store a durability, but `Item::getMaxDurability` is 1
  for them, so `getPrice` never discounts their wear (`> 1`). The table
  gives them no grade policy and no durability, which matches their grade
  and grade offsets; their maximum is not `maxDurabilityBase`, and
  `ItemGrade.h` says so for the client, whose motorcycle computes one from
  its item table. The client also departs from the table on purpose for
  the two couple rings (`CoupleRing`, `VampireCoupleRing`, built on
  `CoupleRingBase`, not `ConcreteItem`): it gives them the accessories'
  grade policy and a durability, where the server has neither.
  > **Status:** recorded, not fixed (feat/shared-grade-policy)

## Shared price rules (2026-09-28)

The shop buy, sell and repair price and an item's maximum durability moved
into de-core (`src/domain/ItemPrice.cpp`, `ItemDurability.cpp`) so the
client can build the same source. The move is verbatim; what it kept and
what it changed:

- **The price rules' oddities are kept as the server charges them.**
  `getPrice` treats a maximum durability of 1 as none (`> 1`) while
  `getRepairPrice` does not (`!= 0`); the low-stat slayer's potion discount
  and the Blood Bible adjustment truncate the price in the middle of the
  calculation; the head price multiplies by `bonus / 100` in integers, so a
  150% head price bonus pays x1; a vampire portal is repaired by durability
  while the slayer portal and the ousters summon item are charged for their
  missing charges; a charge count above the maximum makes the repair price
  negative, which `decore::repairPrice` returns as an `int` (-5000 in the
  `slayer-portal-overcharged-wraps` row) and the server's unsigned
  `Price_t` receives wrapped (4294962296), as the old unsigned arithmetic
  gave it. Each is pinned by a named row in
  `src/domain/vectors/`, and changing one is a balance decision.
  > **Status:** recorded, not fixed (feat/shared-price-rules)
- **de-core's results depended on the build machine's CPU.** Clang, and
  so `zig c++`, fuses `a * b + c` into one rounding wherever the target CPU
  has a fused multiply-add: every arm64 target, and x86-64 with FMA, which a
  Zig build without a target gets from the build machine. So the server's
  own formulas (`Formulas.cpp`, `SkillOutputFormulas.cpp`) could round
  differently on the arm64 dev container, an FMA x86-64 host and a
  non-FMA one. All of de-core compiles with `-ffp-contract=off
  -fno-fast-math` now (`src/domain/CMakeLists.txt`), for the existing
  files as well as the shared ones. `formula_tests` passed unchanged on the
  arm64 container, where contraction had been on, so no pinned result
  moved; a result within an ulp of an integer that a formula truncates
  could still come out one lower or higher on an FMA host than before.
  > **Status:** fixed (feat/shared-price-rules)
- **`getPrice` dereferenced a null creature** on the premium half-price and
  Blood Bible branches (the race branch checked it, those two did not), and
  the premium branch also dereferenced the creature's `GamePlayer` without a
  check. No caller passes either. The adapter now reads the creature only
  when it is not null: the potion ratio for any player creature, the pay
  state only for a player creature with a `GamePlayer`. So a null creature
  prices as none, and a player creature without a `GamePlayer` as not
  paying, with its own potion ratio.
  > **Status:** fixed (feat/shared-price-rules)
- **Flat-priced items now read the item tables first.** An item given away
  by the game, a time-limited item and the crown moon card used to return 1,
  50 or the crown price before any table read. The adapter now gathers every
  input, the item table, the option table and the maximum durability
  included, before de-core picks the flat price, so an item whose option id
  is corrupt now reaches the option table's existing `Assert` instead of
  returning early. `getRepairPrice` changed the same way: a slayer portal
  or an ousters summon item used to be charged for its missing charges
  before the option loop and `computeMaxDurability` ran, and now both run
  first. Every shop item has all three, so no valid item prices
  differently.
  > **Status:** recorded, not fixed (feat/shared-price-rules)

## Compiler-warning cleanup (2026-09-27)

Clearing the server's ~1,000 compiler warnings (Apple Clang 21 and the Zig
build; `sprintf` alone was 490 of them) was done behaviour-preserving, so a
warning that pointed at a bug had the bug kept and written down here. The
exceptions are the undefined behaviour below, which has no behaviour to keep,
and the optimized builds' `NDEBUG`, which changed what the deployed servers
do.

- **The Zig optimized builds compiled `Assert` and the exception handlers
  away.** `zig c++` defines `NDEBUG` itself from `-O1` up, so the Release,
  RelWithDebInfo and MinSizeRel builds ran with it despite the rule that
  no build defines it, and the Docker image is a Zig Release build by
  default (`Dockerfile`'s `BUILD_TYPE`, `docker-compose.yml`). Under
  `NDEBUG`, `Assert` and `ProtocolAssert` evaluated their expression and
  never fired, and `__BEGIN_TRY`/`__END_CATCH`/`__END_CATCH_NO_RETHROW`
  and `__BEGIN_DEBUG`/`__END_DEBUG` compiled to nothing. The optimized
  flags now pass `-UNDEBUG`, which changes the deployed servers: a failing
  `Assert` throws `AssertionError` (logged to `assertion_failed.log`), a
  failing `ProtocolAssert` disconnects the client, and the 344
  `__END_CATCH_NO_RETHROW` sites swallow the exceptions that used to escape
  them. Debug builds, and so the test suite, always had the handlers.
  Since fix/review-followups, `Assert.h` and `Exception.h` stop the compile
  with an `#error` when `NDEBUG` is defined, so this cannot recur silently.
  > **Status:** fixed (fix/compiler-warnings)
- **Undefined behaviour removed.** `Zone::load` freed three `new char[]`
  buffers with scalar `delete`; they use `SAFE_DELETE_ARRAY` now.
  `PlayerCreature::setPetInfo` deleted a `Centauro`, `Stirge` or `Pixie`
  through `Pet*` without a virtual destructor; `Pet` has one now (the
  subclasses' destructors are empty). `SocketInputStream::fill_RAW` and
  `SystemAPI::fork_ex` ran off the end of a non-void function on non-Linux
  builds; they throw `UnsupportedError` there, as before the non-Linux branch
  was deleted (neither is called on macOS today). Locals read uninitialised on
  a path for a creature of no known race now start at zero, null or empty
  (`EffectHasSlayerRelic`/`EffectHasVampireRelic` race name,
  `GQuestGiveEventQuestItemElement` base, `ActionStashSell` price,
  `CGLotterySelectHandler` quest id). The relic announcement stayed
  undefined behaviour after this: its format lacked an argument, fixed
  under *Review follow-ups*.
  > **Status:** fixed (fix/compiler-warnings)
- **`sprintf` became `snprintf` with the destination's size.** Output is
  identical whenever it fits; where it overflowed before (undefined behaviour)
  it is now truncated. The login server's forced-port strings were 5-byte
  arrays, which a base port of 9900 plus an `-i` of 100 or more overflows;
  they are sized for any `int` now, so those values are no longer cut. The
  game server's 50-byte message buffers were not sized to their data and
  cut three seed messages short; see *Review follow-ups*.
  > **Status:** fixed (fix/compiler-warnings)
- **Wolf form rejects every skill.** The four `isAbleToUse*Skill` guards test
  `SkillType != A || SkillType != B ...`, true for any skill, so the early
  return is now unconditional. The commented-out `&&` versions, removed with
  the dead test, allowed Howl, Eat Corpse and Un-transform (self), Howl, Eat
  Corpse and melee attack (object), and Howl and Eat Corpse (tile, inventory).
  > **Status:** recorded, not fixed (fix/compiler-warnings)
- **Checks that could never fire were removed with their tautologies**, each
  of which evidently meant something else: `CGLearnSkillHandler` rejected a
  skill type `>= SKILL_MAX && < SKILL_DOUBLE_IMPACT` (never true; `||` would
  reject out-of-range types before `getSkillInfo`); `opset item_prob_ratio`
  returned on `value < value`; `CGAddItemToItemHandler` asserted a
  `CoordInven_t` (unsigned char, 255 when the pet item is missing) was not
  `-1`, so the missing case reaches `makeGCCreateItem` with X = 255.
  > **Status:** recorded, not fixed (fix/compiler-warnings)
- **`getRandomMysteriousItem` tests `Item::ITEM_CLASS_VAMPIRE_COAT && sex ==
  FEMALE`**, a constant that is always true, so female vampires get item type
  1 for every item class; `itemClass ==` was evidently meant. Kept.
  > **Status:** recorded, not fixed (fix/compiler-warnings)
- **`Tile::deleteCreature(MoveMode)` calls the `ObjectID_t` overload of
  `deleteObject`** (the sum `OBJECT_PRIORITY_WALKING_CREATURE + mode` is an
  `int`), which looks for an object with ID 1..3 and asserts when none
  matches; `getCreature(MoveMode)` wraps the same sum in `ObjectPriority`,
  which was evidently meant.
  > **Status:** recorded, not fixed (fix/compiler-warnings)
- **Overrides that do not override.** `DummyQuestStatus::isSuccess()` lacks the
  base's `const` (the class is never instantiated). `SharedServerClient`,
  `GameServerPlayer` and mofus `MPlayer` declare `processCommand()` without the
  base's `bool`; every call goes through the derived type, and each now also
  forwards `processCommand(bool)` to `Player`'s, which is what a call through
  `Player*` ran before.
  > **Status:** recorded, not fixed (fix/compiler-warnings)
- **Smaller findings, kept as they are:** `Resource::write` stores the file
  name's length in a `BYTE`, so a name of 256 characters or more puts the
  wrong length on the wire; seven senders put a duration of 999999, meant
  as "permanent", through the 16-bit `Duration_t`, so 16959 arrives
  (`static_cast<Duration_t>(999999)` in the GM `opGhost` in
  `gm/ConsoleCommands.cpp`, `EffectGrandMasterSlayer`,
  `EffectGrandMasterVampire`, `EffectGrandMasterOusters` and
  `EffectDonation200501`, and the tile portals of `DynamicZoneAlterOfBlood`
  and `DynamicZoneGateOfAlter`); `MotorcycleLoader::load` allocates a
  `Motorcycle` per row and never attaches or frees it; `~PlayerCreature` never
  deletes `m_pPet`; `EffectSatelliteBombAim` uses the skill slot from
  `hasSkill` without a null check; `Guild`'s `ExpireDate` is built from
  `tm_year`/`tm_mon` and is not a real date.
  > **Status:** recorded, not fixed (fix/compiler-warnings)

## Native macOS build (2026-09-27)

- **The 64-bit encrypted stream overloads did not compile on macOS.**
  `SocketEncryptInputStream::readEncrypt(ulong&)` and
  `SocketEncryptOutputStream::writeEncrypt(ulong)` assumed `ulong` is
  `std::uint64_t`. It is on LP64 Linux; on macOS `std::uint64_t` is
  `unsigned long long`, so the in-class overloads instantiated
  `read<unsigned long>` / `write<unsigned long>`, which `de::WireScalar`
  rejects, and every file including either header failed to compile. Both
  are spelled `std::uint64_t` now, the same type as before on Linux; the
  goldens pass on both platforms.
  > **Status:** fixed (fix/macos-native-build)
- **CMake did not find Homebrew's keg-only `mysql-client`, nor the root
  `LuaState.h` includes LuaJIT from.** The MySQL search named only
  `Cellar/mysql-client/*`, which misses `mysql-client@8.4`; and
  `<luajit-2.1/lua.h>` needs the directory above the one found for the
  headers, which Linux supplies as `/usr/include` and Apple Clang does not.
  The search now names Homebrew's `opt/` prefixes, and the luajit keg's
  `include/` is added on macOS. At first it named only the Apple Silicon
  prefix and put the unversioned `mysql-client` (a newer series than the
  verified `mysql-client@8.4`) first; since fix/review-followups it
  prefers `@8.4`, then `@8.0`, and searches Intel's `/usr/local/opt/` too.
  > **Status:** fixed (fix/macos-native-build)
- **`tools/devbuild.sh` stopped before starting its container on macOS.**
  macOS ships bash 3.2, where expanding an empty array under `set -u` is an
  unbound-variable error, and the `docker run` line expands two arrays that
  are empty outside a terminal or without `DEVBUILD_JOBS`.
  > **Status:** fixed (fix/macos-native-build)
- **`src/Core/Assert.h` answers `#include <assert.h>` on a case-insensitive
  file system.** `src/Core` is on every target's include path, so on macOS
  the C header resolves to the project's own and googletest's `gtest.cc`
  loses `assert`. The servers build regardless; the test suite needs the
  forced include the README gives. Keeping the project's include path off
  the googletest targets would end it.
  > **Status:** recorded, not fixed (fix/macos-native-build)
- **`tests/ratchet/ratchets.sh` rejects BSD `wc -l` output.** macOS's `wc`
  pads its count with spaces, and `check_ratchet` refuses `'       0'` as
  not a number, so every count piped through `wc -l` fails natively on
  macOS. The container run is unaffected.
  > **Status:** recorded, not fixed (fix/macos-native-build)
- **`proxy_acceptor_tests` assumes loopback delivery is immediate.** Two or
  three of its nine tests fail natively on macOS, varying run to run: they
  send on a loopback socket and poll at once, before the bytes have always
  arrived there.
  > **Status:** recorded, not fixed (fix/macos-native-build)
- **`shutdown_supervisor` times out natively on macOS** (90 s). It drives
  `docker/start.sh`, which is written for the Linux container. Its
  `is_alive` reads `/proc/<pid>/stat`, which macOS does not have, so every
  server always looks dead: the first scenario still passes, but in the
  second, whose loginserver ignores SIGTERM, the supervisor never reaches
  its "did not drain; forcing termination" branch and its final `wait`
  blocks on the loginserver until ctest's timeout. The script's
  `${pids[-1]}` is also an error in macOS's bash 3.2, which only loses the
  log mirroring; `stdbuf` exists on macOS 27 (`/usr/bin/stdbuf`).
  > **Status:** recorded, not fixed (fix/macos-native-build)

## Connection admission: gateway header and socket ownership (2026-09-25)

- **`ProxyAcceptor` accepted a PROXY header whose client address held a
  NUL byte.** `inet_pton` reads a C string, so `203.0.113.21\0junk` was
  admitted as `203.0.113.21`. A header containing a NUL is now refused;
  `tests/proxy_acceptor_test.cpp` covers it.
  > **Status:** fixed (feat/rust-websocket-proxy)
- **The loginserver ended on a connection whose descriptor its player
  table cannot hold.** `LoginPlayerManager::acceptNewConnection` had no
  bound check, so `PlayerManager::addPlayer`'s `Assert` threw out of
  `processInputs()` and out of the client loop. It now refuses such a
  connection as the gameserver's `IncomingPlayerManager` does, and holds
  the socket in a `unique_ptr` until the `LoginPlayer` takes it, so a
  refusal, or an exception before the player takes it, closes it instead
  of leaking it.
  > **Status:** fixed (feat/rust-websocket-proxy)
- **`SocketImpl::close()` released the descriptor twice.** `Player`'s
  destructor closes its socket and then deletes it, and `~SocketImpl`
  closes again with the old number, which another thread may have been
  given in between (the gameserver's zone and DB threads open descriptors
  concurrently). Closing now marks the socket closed before the call and a
  second close is a no-op; `tests/socket_close_test.cpp` covers it. The
  first version of this fix wiped the descriptor number instead, which
  ended the loginserver on every character selection: the player managers
  index their tables by that number and delete a player by `getSOCKET()`
  after `disconnect()` has closed the socket, so `deletePlayer_NOLOCKED`
  threw `OutOfBoundException` and the retried `disconnect()` asserted. The
  number now stays readable after close.
  > **Status:** fixed (master)
- **`GatewayProxyPort` was parsed with `atoi`.** `19099abc` bound 19099,
  `4294986395` bound 19099 and `1e3` bound port 1. The value is now parsed
  strictly (surrounding blanks and a CRLF file's `\r` allowed) and anything
  else refuses to start.
  > **Status:** fixed (feat/rust-websocket-proxy)
- **`IncomingPlayerManager::acceptNewConnection` deleted the socket twice
  when the descriptor slot was already taken.** The `DuplicatedException`
  branch deleted the socket and then the `GamePlayer` that owned it. It
  now deletes only the player, marked `GPS_END_SESSION` so its destructor
  does not assert.
  > **Status:** fixed (feat/rust-websocket-proxy)

## Character gear flags are read before initialization (2026-09-25)

- **The Slayer, Vampire and Ousters constructors initialized gear pointers but left the matching
  `m_pRealWearingCheck` flags uninitialized.** The first stat recalculation
  copied invalid boolean values and aborted the Debug server during login.
  Value-initialize the flag array in all three constructors' initializer lists.
  > **Status:** fixed (fix/browser-startup-crashes)

## Slayer stat loading reads past its skill-domain array (2026-09-25)

- **`Slayer::initAllStat` and `computeStatOffset` copied all races' skill
  domains from an array containing only the six Slayer domains.** The
  first Slayer login aborted the Debug server on index 6. Both copies now
  stop at `SKILL_DOMAIN_VAMPIRE`, the source array's declared bound, and
  value-initialize the calculation inputs so the other races' slots stay zero.
  > **Status:** fixed (fix/browser-startup-crashes)

## Character handoff generates a key with an invalid shift (2026-09-25)

- **The incoming-connection handlers used a random integer as a shift
  count.** Selecting a character aborted the Debug game server before it
  could return the reconnect address. Both the game-server handoff and
  the reverse login-server handoff now draw a full-width `DWORD` from
  `std::random_device` through `std::uniform_int_distribution`, matching
  the entropy source already used for password salts. Packet layouts and
  key checks are unchanged.
  > **Status:** fixed (fix/browser-startup-crashes)

## Waypoint quest loading overwrites narrow fields (2026-09-25)

- **`GQuestTouchWayPointElement::makeElement` bound `DWORD&` to four
  16-bit fields and an 8-bit direction.** Loading `TouchWayPoint` from the
  shipped quest XML aborted the Debug server on a misaligned reference;
  the 32-bit writes also overwrote adjacent fields. Read through checked
  `WORD`/`BYTE` XML attribute overloads with the actual destination types.
  Invalid, missing and out-of-range values leave the destination unchanged.
  Regression tests cover adjacent-field preservation and numeric boundaries.
  > **Status:** fixed (fix/browser-startup-crashes)

## A logout whose relic drop threw left the player's name in the finder (2026-09-25)

- **`~GamePlayer` dropped a departing holder's relics before the
  player-creature finder forgot him, and `dropRelicToZone` throws when no
  free tile is found, which the destructor's catch rethrew into a
  `__END_CATCH_NO_RETHROW` that only recorded the stack,** so the finder
  removal, the mailbox's `ifGone` drain and the creature's deletion were
  skipped: the finder kept a name whose `GamePlayer` was gone, and a later
  `de::postToPlayer` for it queued a command on a destroyed object. The
  drop's failure is caught where it happens and logged to `WarError.log`,
  and the session ends as it does for every other player; the relic goes
  with the creature.
  > **Status:** fixed (fix/war-broadcasts)

## A dragon eye sent home across groups can outrun its return's three attempts (2026-09-25)

- **`DragonEyeManager::warpToDefaultPosition` saves the eye's default tile
  to its row the moment it hands the eye to another group's zone through
  the delayed add, while the add itself lands a tick later and, when that
  tile is taken, on a neighbouring one, saving the real tile then,** so a
  return (`de::war::postItemReturn`) already in flight can spend its three
  attempts on the row's stale answers: the first misses at the holder, the
  second at the default tile before the add has run, and the third at the
  default tile the second re-read, one tick before the add saved the real
  one. The eye then stays in the world after `removeAllDragonEyes`. It
  needs the war's end to fall within the two ticks of a holder's death or
  transport and an occupied default tile. Closing it means the retry
  reading the row when it runs rather than when it is posted, or the
  delayed add carrying the row's save with it.
  > **Status:** recorded, not fixed (fix/flag-relic-returns)

## `*command LevelWar` pushes a schedule into a level war zone's queue from another thread (2026-09-25)

- **`opLevelWar` (`gm/ConsoleCommands.cpp`) calls
  `LevelWarManager::manualStart` on the level war zone it names, from
  whichever thread runs the command -- a GM's zone thread or the login
  server link's for a relayed `*command` -- while that zone's own thread
  runs the manager's heartbeat.** `manualStart` pops the queued work and
  adds a new `Schedule`, and replaces `m_pLevelWarSchedule`, on a
  `Scheduler` that has no lock of its own, so the two threads can both
  change the queue at once. Closing it means posting the start to the zone's
  group (`de::war::postToZone`), as the war's other zone work is.
  > **Status:** recorded, not fixed (fix/war-broadcasts)

## A war relic's row outlives the run that made it (2026-09-25)

- **No loader reads a castle symbol's, blood bible's or dragon eye's row at
  startup: the shrine managers and `DragonEyeManager` make every war relic
  anew, with a new ItemID, at each boot, and `ItemLoaderManager::load(Zone*)`
  and `WarItemLoader` are empty,** so the rows an earlier run left -- a
  shrine's, the ground's, and a holder's after a crash -- are never read
  again and pile up (the shipped `initdb/DARKEDEN.sql` carries such
  `BloodBibleObject` rows). A slayer's or vampire's login deletes the war
  item rows naming him (`deleteWarItemsOfOwner` in the loaders'
  `load(Creature*)`); `ItemLoaderManager::load(Ousters*)` runs none of those
  loaders, so an ousters' rows stay. They are garbage, not relics: no
  return reads an id of an earlier run. Item ids are partitioned per server
  (every id is the server's number modulo the server count, from
  `initItemIDRegistryFromTable`), so a boot sweep could tell this server's
  rows from another's; it is left undone because nothing reads them.
  > **Status:** recorded, not fixed (fix/flag-relic-returns)

## A dragon eye held with a relic dropped only the eye (2026-09-25)

- **`dropRelicToZone(Creature*)` sent a dragon eye home and returned at
  once, before it looked for the relics the holder carried,** and holding a
  plain relic does not stop a dragon eye's pickup (`isAbleToPickupItem`
  refuses a relic to a holder of a bible, a symbol or an eye), so a
  player carrying both who died, was transported or logged out kept the
  relic, or lost it with the player object. The eye goes home and the
  relics drop after it.
  > **Status:** fixed (fix/flag-relic-returns)

## `DragonEyeManager` wrote its tables past their size (2026-09-25)

- **The constructor only `reserve()`d `m_DragonEyes` and
  `m_DefaultPositions` and then wrote nine entries of each through
  `operator[]`,** past the vectors' size of zero: undefined behaviour that
  worked because the storage was there. Both are sized now.
  > **Status:** fixed (fix/flag-relic-returns)

## A dragon eye has no place in the relic handlers (2026-09-25)

- **`CGRelicToObjectHandler` lets any relic class past its first check and
  then has no branch for `ITEM_CLASS_WAR_ITEM`,** so a player who offers a
  dragon eye to an object is disconnected (`DisconnectException`), and
  `deleteRelicEffect(Corpse*, Item*)` falls from its `WAR_ITEM` case into
  `default: return false` for want of a `break`. No path puts a dragon eye
  in a corpse, so the second is unreachable; the first costs only the
  sender's own session, which drops the eye home as it ends.
  > **Status:** recorded, not fixed (fix/flag-relic-returns)

## `*CTF` edits the flag war's schedule from a zone thread (2026-09-25)

- **`opCTF` (`gm/ConsoleCommands.cpp`) runs on the GM's zone thread, or on
  the login link's thread when relayed (`Relay::Allowed`), and
  calls `FlagManager::manualStart`, which pops and pushes the manager's
  schedule queue while the main thread's `ClientManager` loop runs
  `FlagManager::heartbeat` over the same priority queue with no lock,** and
  `startFlagWar` reads the queue's top from the main thread too. A GM
  pulling the flag war forward while the heartbeat looks at the queue can
  corrupt the heap. `WarScheduler` has the shape that closes it: a mutex
  over the queue, the due schedule popped under it and run after it is
  released, and every `addSchedule` from the war taken under it.
  > **Status:** recorded, not fixed (fix/flag-relic-returns)

## A newbie flag war's end rewarded the race that planted last (2026-09-25)

- **`NewbieFlagWar::executeEnd` summons its reward monster at the winning
  race's end of zone 1122 by `getWinnerRace()`, but it ran after
  `FlagWar::executeEnd`, which had zeroed the counts,** so every race tied at
  zero and the tie-break -- the race whose flag was planted last -- chose the
  reward. The end no longer zeroes the counts; the next start does, as it
  always did, so the end, and the returns it posts, which pay a pole of the
  winning race its gem stone as they take its flag, read the round's own
  result.
  > **Status:** fixed (fix/flag-relic-returns)

## A flag war's pole sweep took the pole off its tile (2026-09-25)

- **`FlagManager::resetFlagCounts` swept the pole fields for flags whose
  return had missed them: it took the pole's treasure out with
  `getTreasure()` and then called `Zone::deleteItem(flag, x, y)`, which
  deletes whatever item lies on the tile -- the pole corpse, not the flag,**
  with no broadcast: the pole stayed on every client's screen and in the
  pole table, gone from the tile. It also took the first treasure of any
  monster corpse lying within twice a field's extent and, when that was
  not a flag, lost it: the item left the corpse for nowhere. The sweep now
  looks only at flag poles, destroys a flag it takes and puts anything else
  back, and leaves the tile alone.
  > **Status:** fixed (fix/flag-relic-returns)

## A flag war's counts, status and allowed zones were shared with no lock (2026-09-25)

- **`FlagManager`'s flag counts and status packet are written by the zone
  threads that plant and pull flags, under `m_Mutex`, but were read and
  zeroed with no lock by the main thread and by any zone thread sending the
  status;** `getFlag` tested a count before taking the lock it decremented
  it under, so two poles pulled at once in different groups could take the
  same last flag and wrap the count; the zones a war allows flags in were a
  `std::map` the main thread cleared and refilled at every start while zone
  threads looked zones up in it; and `m_bHasFlagWar` was a plain `bool`. The
  counts, put times, end time and status packet are read and written under
  `m_Mutex`, the zone threads send a copy of the status, the allowed zones
  are a `de::Snapshot` and the flag an atomic.
  > **Status:** fixed (fix/flag-relic-returns)

## A broadcast packet whose write() throws leaks the stream it was to be queued in (2026-09-25)

- **`ZonePlayerManager::pushBroadcastPacket` allocates the
  `SocketOutputStream` it queues before `writeHeaderNBody` fills it, and a
  write that throws leaves the function with the stream neither queued nor
  freed.** No player receives anything of the refused packet, since the
  stream is the packet's own; the cost is one buffer, sized from the
  packet, leaked per refusal. Closing it means holding the stream in a
  `std::unique_ptr` until it is queued.
  > **Status:** recorded, not fixed (fix/db-lookups-stream)

## A packet whose write() throws leaves its first bytes in the player's stream (2026-09-25)

- **`SocketOutputStream::writePacket` puts the packet id, a zero size
  placeholder and the sequence byte into the stream before `write()`
  runs, and nothing takes them back when `write()` throws.** A
  `de::wire::writeString` bound refusal is an `InvalidProtocolException`,
  and `GamePlayer::sendPacket` catches and drops exactly that type, so the
  send looks like a skipped packet while the id, the placeholder, the
  sequence and whatever fields preceded the refused one stay in the
  output buffer, framed by a size of 0 that `patchField` never
  overwrote. The client reads the next packet from the middle of that
  debris. Every server-side write that can refuse a field is exposed; the
  one found is the Gilles de Rais echo below. `writePacket` now records
  the packet's start, as a distance from the head (which no write moves
  and a buffer growth keeps), and the sequence counter before it writes
  the id; when anything in the framing throws it moves the
  tail back to that start and restores the counter, then rethrows, so a
  refused packet leaves nothing in the stream and the next one takes its
  sequence byte. `PacketFrameRollback` in `wire_types_test.cpp` pins it
  on a contiguous buffer, across a buffer growth and across the wrap
  point. The other framing paths, `Packet::writeHeaderNBody` and
  `Datagram::write`, fill a stream or a datagram of their own that the
  throw abandons, so they carry no debris into a player's stream.
  > **Status:** fixed (fix/db-lookups-stream)

## The Gilles de Rais lair's global-chat echo overflows its 128-byte message (2026-09-25)

- **`GDRScene::ActionSay` echoes each line Gilles de Rais says as a
  `GCGlobalChat` of the name, a space and the line, and the Korean name
  made the longest seed line too long for the packet.** `GCGlobalChat`
  refuses a message past 128 bytes; the prefix was 10 bytes of UTF-8, and
  `GSStringPool` 348, which scene 5 says, is 120 bytes, so its echo
  was 130 and was refused for every player in the lair (with the stream
  damage above). The prefix is `Gilles ` now, 7 bytes, and the longest
  line any scene says fits at 127.
  > **Status:** fixed (r17/gameserver-core)

## The GM summon command never recognised a chief monster's name (2026-09-25)

- **`opsummon` (`gm/ZoneCommands.cpp`) decides a typed name is a chief
  monster's by finding the Korean word for "chief" in it, and only then looks
  the name up in the chief table, keyed by `MonsterInfo.HName`.** The
  literal came through the encoding migration with its first syllable's two
  CP949 bytes intact, because they happen to be valid UTF-8, and its second
  syllable replaced by U+FFFD, so no name ever contained it: a chief's name
  fell through to the sprite lookup, which summons a random regular monster
  of the chief's sprite instead of the chief. The literal is now the word's
  UTF-8 bytes, the encoding the `initdb/` names are in.
  > **Status:** fixed (r17/quest-gm)

## `*pay` formats its message into the buffer it reads the prefix from (2026-09-25)

- **`oppay` (`gm/PlayerCommands.cpp`) writes a `[Metrotech][...]` prefix into
  `str`, then for a period or time account calls `sprintf(str, "%s...", str,
  ...)`,** passing the destination as a source. Overlapping `sprintf`
  arguments are undefined behaviour; it works only as long as the C library
  copies the leading `%s` onto itself. Both branches now append at
  `str + strlen(str)` with an `snprintf` bounded by the space left in the
  80-byte buffer, so the prefix is only read where it stands and a long
  date or count is cut rather than written past the end; the message text
  is unchanged.
  > **Status:** fixed (fix/db-lookups-stream)

## An Altar of Blood offering never answers a relic (2026-09-25)

- **`CGRelicToObjectHandler` accepted a relic brought to an offering
  (monster types 793..795) only when the offering's name was a key of its
  line table, and the keys were the offerings' Korean names while
  `DynamicZoneAlterOfBlood::addOffering` named them from
  `OfferingTemplate`, which held the names the Chinese build gave the same
  fifteen captives (GBK bytes decoded as Latin-1),** so no name ever matched
  and every relic was refused with `GCCannotAdd`. The two tables list the
  captives in the same order, the handler's fifteen keys, top to bottom,
  being the template's three rows of five. The template names the captives
  in English and the handler's keys are those same fifteen names, so an
  offering answers its relic again.
  > **Status:** fixed (r17/gameserver-core)

## The donation nicknames did not fit the nickname field (2026-09-25)

- **`CGDonationMoneyHandler` grants six custom nicknames, and in UTF-8 each
  of the Korean ones was 26 to 32 bytes, past the 22 `MAX_NICKNAME_SIZE`
  allows on the wire** (they fitted in CP949). `NicknameInfo::setNickname`
  cuts a name to 22 bytes, so the book stored and sent each of them cut in
  the middle of a Hangul syllable, a name ending in invalid UTF-8. The
  English nicknames are 15 to 21 bytes. A book that already stores one of
  the Korean names keeps its cut 22-byte form.
  > **Status:** fixed (r17/handlers-war-misc)

## A war's whole-zone broadcasts walk the zones' player lists with no lock (2026-09-25)

- **`HolyLandManager::broadcast` and `CastleInfo::broadcast` send a packet
  to every player of every holy-land or castle zone through
  `Zone::broadcastPacket`, which walks the zone's `CreatureManager` map with
  no lock,** and they run on threads that do not own those zones: the main
  thread's war heartbeat (the race war's start and end, the war end messages,
  `RegenZoneManager::broadcastStatus`) and, now that a relic's return runs
  on its holder's thread, a zone thread announcing a symbol's or a bible's
  return in another group's zones. A zone thread adding or removing a player
  while another thread walks the map is undefined behaviour, not a stale
  message. This is older than the posted zone work, which moved the writes;
  a broadcast only reads. Closing it means posting each broadcast to the
  zone's group too (`de::war::postToZones` with the packet's bytes), which
  the routing already supports, or a published copy of the player list a
  reader may walk.
  Each of the two broadcasts, and
  `LevelWarZoneInfoManager::broadcast` (whose send also marks each player
  it reaches) and the GM `*set` premium notice to zones 61, 64 and 1007,
  posts through `de::war::postBroadcast`: the packet's body is captured
  once on the calling thread (`war/CapturedPacket.h`), bytes shared by
  every group's command, and each player's stream frames it with its own
  id, size, sequence byte and encryption; a copy of the packet was not an
  option, since the bonus-info packets own and delete their entries. The
  whole-server broadcasts (`ZoneGroupManager::broadcast`) stay direct: they
  walk each group's players under its `ZonePlayerManager` mutex and only
  send, as does `IncomingPlayerManager::broadcast` on the main thread's own
  players.
  > **Status:** fixed (fix/war-broadcasts)

## A relic held where no item position reaches stays put when its war ends (2026-09-24)

- **A war's end returns each castle symbol and blood bible, and takes each
  dragon eye out of the world, from the holder its item-object row names: a
  zone for the ground or a corpse, a player for the inventory or the mouse
  (`de::war::postItemReturn`).** A row naming any other storage (gear, belt,
  stash, motorcycle, garbage), a zone this server does not have, or a
  player who is not logged in is logged to `WarError.log` and the item is
  left where it is, as is an item the return missed three times because it
  kept moving before the holder's step ran. A holder who logs out drops his
  relics to the zone before he is gone (`~GamePlayer`, `dropRelicToZone`)
  and a transported one drops them as he leaves, so such a row is one
  nothing keeps current: a relic somewhere the game should not let it go.
  Closing it means proving those places unreachable for a relic, asserted
  where an item moves, or returning from them too.
  Every gateway into another storage refuses a relic or has no writer, as
  `de::war::relicMayLieIn` (`war/WarZoneRouting.h`) lists them, and a
  relic's `create`/`save` logs a row naming any other storage to
  `WarError.log` (`logRelicStorage`). A row naming a player who is not
  logged in is read again, like a missed step: a holder drops his relics
  and saves where they fell before the player-creature finder forgets him,
  and a dragon eye sent home to another group now names its default tile
  until that group adds it. No row names a zone of another server: the
  relics are made in this server's zones, `transportCreature` moves a
  player only between them, and a logout drops the relics before the
  player moves to another server. No boot-time sweep: no loader reads a
  relic's row (the entry above). What still stops a return is an item that
  keeps moving for three attempts, or a logout whose drop throws for want
  of a free tile, each logged; the relic of such a logout goes with the
  creature, and its row names a player nobody finds.
  > **Status:** fixed (fix/flag-relic-returns)

## The regen zone status packet is written by every tower's zone thread (2026-09-24)

- **`RegenZoneManager` keeps one `GCRegenZoneStatus` for all the holy
  land's towers, and `changeRegenZoneOwner` (a tower captured) and
  `reloadOwner` (the race war's end, posted to each tower's zone) set a
  tower's entry in it and broadcast it from the tower's own zone thread,**
  with nothing ordering towers in different groups, so two groups may write
  and send it at once; each `RegenZoneInfo`'s owner is set the same way and
  read by every zone thread asking `canRegen`. The race war's end used to
  replace the packet from the main thread, leaking the old one while the zone
  threads used it, and now keeps the one packet. A leaf mutex over the
  packet and the owners, held for a set and for the copy a broadcast sends,
  would close it.
  The status is a value under
  `RegenZoneManager::m_StatusMutex`, a leaf held while a tower's owner and
  its entry change together and while a copy is taken; the owners are
  atomics, which `canRegen` and `canTryRegenZone` read without it. The copy
  a broadcast sends is taken by the command `broadcastStatus` posts to each
  holy land group, when it runs rather than when it is posted, so a group
  that receives two towers' broadcasts in the opposite order to their
  changes still ends on the status carrying both; a player entering the
  holy land is sent a copy too.
  > **Status:** fixed (fix/war-broadcasts)

## A flag war's end takes its flags out of the zones from the main thread (2026-09-24)

- **`FlagWar::executeEnd` runs on the main thread (`FlagManager::heartbeat`,
  driven by `ClientManager`) and pops every flag it made from wherever the
  flag's row says it lies, under that zone's own mutex only,** the shape
  the castle and race wars' ends had: the zone's group's CG handlers are not
  excluded, and a flag a player carries is taken from a player another
  thread owns. `de::war::postItemReturn` with a step that destroys the flag
  is the fix; its start side needs the same reading.
  The end returns each flag through `de::war::postItemReturn`, whose step
  destroys it on the holder's thread, and posts each pole zone's sweep after
  the returns (`ctf/FlagWarPlan.h`), so a flag planted on a pole is taken by
  its return, which pays the winner's gem stone, before the sweep looks.
  The start posts each zone's flag drop to its group (`postToZone`), the
  drops writing the flags' ids into a ledger the end takes; the start's
  pole sweep, every status broadcast (from the main thread or a zone
  thread planting a flag) and a newbie war's reward summon are posted the
  same way, and the notices go through `ZoneGroupManager::broadcast`, which
  takes each group's player-manager lock, as before.
  > **Status:** fixed (fix/flag-relic-returns)

## A castle war's start kills the monsters of dungeons in another group (2026-09-24)

- **`GuildWar::executeStart` runs on the castle zone's thread and clears the
  castle's dungeons (`killAllMonsters`, every monster's HP set to 0) under
  each dungeon zone's own mutex alone,** and in the seed three castles'
  dungeons belong to the other zone group: 1211/1212 and 1231/1232 are in
  group 2 while castles 1201 and 1203 are in group 1, and 1261/1262 are in
  group 1 while castle 1206 is in group 2. The zone's own mutex excludes that
  dungeon's heartbeat but not its group's CG handlers, which damage the same
  monsters. The siege zones the siege start fills sit in their castle's group.
  The start posts each dungeon's clear to the group that owns the dungeon
  (`de::war::postToZones`, one command per group, the zone looked up by id
  when it runs), and `Zone::killAllMonsters` asserts that group.
  > **Status:** fixed (fix/war-end-zones)

## Two wars registered at once could take the same war id (2026-09-24)

- **`War::War` advanced the static war-id registry with no lock,** while
  castle wars are registered from the castle NPCs' zone threads
  (`ActionWarRegistration`, `ActionRegisterSiege`) and the race war is made
  on the main thread, so two registrations at once could read the same
  registry value and hand out one id twice. `WarScheduleInfo` is keyed on
  the id and the insert is an `INSERT IGNORE`, so the second war was dropped
  from the table -- scheduled in memory only, lost on a restart, its status
  saves landing on the other war's row. The id is now taken under
  `War::m_Mutex`, the registry's own mutex, a leaf that only the startup
  load took before.
  > **Status:** fixed (fix/castle-balance-schedule)

## A forced union quit fails on the guild's own offer row (2026-09-24)

- **`CGQuitUnionHandler`'s forced quit writes the ESCAPE penalty with a
  plain `INSERT` into `GuildUnionOffer`, whose key is `OwnerGuildID`,**
  after `removeGuild` has already taken the guild out of the union. A guild
  with a pending QUIT offer, or with any older row -- an ESCAPE from an
  earlier forced quit, which nothing ever deleted -- hits the duplicate key:
  `END_DB` throws, and the handler dies with the guild out of the union but
  no penalty written, no notice sent and nobody told. The tier's list of
  pinned, unfixed bugs in `docs/RESTRUCTURING.md` named it. The penalty now
  replaces whatever row the guild had (`deleteOffers` before
  `insertEscapeOffer`), and `removeGuild` deletes the leaving guild's QUIT
  row itself.
  > **Status:** fixed (fix/union-offers)

## A guild's offer rows outlive the standing they ask to change (2026-09-24)

- **Only an answered offer was ever deleted.** A member guild expelled, or
  taken out of its union with its guild deleted, kept its QUIT row, so the
  former member was answered `ALREADY_OFFER_SOMETHING` on its next offer and
  its master was still listed a quit request from a guild no longer in the
  union; a union dissolved with its master's guild left the JOIN and QUIT
  rows naming it; and a deleted guild's JOIN offer kept the union it
  targeted alive and could still be accepted, adding a guild that no longer
  exists. Now a guild leaving its union by any road takes its QUIT row
  (`GuildUnionManager::removeGuild`, `removeGuildFromUnion`), a guild going
  away takes all its rows and the abandoned-union rule is applied to the
  union its JOIN offer named, and a dissolved union takes the JOIN and QUIT
  rows naming it (`destroyUnion_LOCKED`; an ESCAPE row is the former
  member's penalty and runs its time).
  > **Status:** fixed (fix/union-offers)

## Worker threads register their database connections under no reader's lock (2026-09-24)

- **`DatabaseManager::addConnection` and `addDistConnection` insert into
  `m_Connections` / `m_DistConnections` under `m_Mutex`, but
  `getConnection(const string&)` and `getDistConnection` look the calling
  thread up with a bare `find` on the same `unordered_map`,** so a worker
  that registers its connection when it starts (each `ZoneGroupThread`,
  the login- and shared-server links, `MPlayerManager`, `GDRLairManager`)
  can rehash the map while an already-running zone thread is inside a
  lookup: a data race on the container, undefined behaviour rather than a
  stale answer. The window is startup, while the workers come up one by one
  and the first zone threads already tick; the loginserver has the same
  shape, its `GameServerManager` worker registering while `ClientManager`
  looks up. The three tables (`getConnection(int)`'s per-world one
  included) are guarded by a `std::shared_mutex` of their own, a leaf held
  for the map access alone: every lookup takes it shared, so lookups never
  wait on one another, and a registration takes it exclusive; the
  sharedserver registers nothing and only looks up. `database_manager_tests`
  registers and looks up from sixteen threads at once.
  > **Status:** fixed (fix/db-lookups-stream)

## A union dissolves under the join offers still pending to it (2026-09-24)

- **`GuildUnion::removeGuild`, the expel and quit handlers' count-then-delete
  and `removeGuildFromUnion`'s dissolve all dissolve a memberless union
  even while JOIN offers to it are pending,** against the rule that an
  offer keeps a union alive. The offers then name a union that no longer
  exists; offers never expire, and the deny handler answers `NOT_IN_UNION`
  to a master who leads no union, so the applicant is refused with
  `ALREADY_OFFER_SOMETHING` for good. The seed holds one such offer
  already, guild 4950's JOIN to a union 28 that is absent. The fix routes
  every removal through the abandoned-union rule (`dissolveIfAbandoned`)
  and clears or expires offers to a union that is gone; the seed check
  should then also fail on a JOIN offer naming no union.
  Every removal now applies the abandoned-union rule and nothing else:
  `GuildUnionManager::removeGuild` (expel, accepted quit, forced quit) and
  the member branch of `removeGuildFromUnion` call
  `dissolveIfAbandoned_LOCKED` once the member row is gone, so a pending,
  unexpired JOIN offer keeps the union (`decideUnionTeardown` no longer
  dissolves on the last member; it dissolves only with the master guild,
  and that dissolve clears the offers naming the union). The expel and quit
  handlers' own count-then-delete of the `GuildUnionInfo` row and their
  local reload are gone, with the two spelled statements that served them
  (`countUnionMembersSpelled`, `deleteUnionInfoOnly`); the forced quit's
  "union dissolved" notice follows what `removeGuild` reports. An offer to
  a union that has gone is dropped, with a `GuildUnion.log` line, by the
  offer purge every load and every offer action runs (`decideUnionOfferPurge`),
  and `tests/ratchet/ratchets.sh` fails on a seed JOIN offer naming no
  union; guild 4950's row went with the seed's other expired offers.
  What a purge or a removal dissolves is published -- the master told, the
  other servers refreshed -- after the mutex is released, on the normal path
  only: a statement that throws after the dissolve (an offer insert refused
  on the guild's key, say) leaves the dissolve on record and the other
  servers' copies stale until the next refresh.
  > **Status:** fixed (fix/union-offers)

## Accepting or denying a union offer does not check which union it targets (2026-09-24)

- **`CGAcceptUnionHandler` and `CGDenyUnionHandler` test only that the
  caller masters some union, then accept or deny the offer of the guild the
  packet names,** so any union master can accept or clear another union's
  offer, and a deny that clears that union's last offer dissolves it. The
  handlers must match the offer's union to the caller's.
  `acceptJoin`, `denyJoin`, `acceptQuit` and `denyQuit` take the union the
  answering master leads and apply `decideUnionOfferAnswer`
  (`guild/GuildUnionJoinOffer.h`, pinned by
  `tests/guild_union_join_offer_test.cpp`) before anything is cleared: an
  offer naming another union is answered `NOT_YOUR_UNION` and left for its
  own master, and a guild with no offer of that kind -- none made, already
  answered, or expired -- is answered `NO_TARGET_UNION`. Both codes are
  ones the client already knows. The quit pair had the same hole (the
  quitting guild's union was checked, the answering master's was not), and
  three of the four answered `OK` for a guild with no offer at all, after
  which the handler wrote the "accepted" or "denied" notice to that guild's
  master; the quit-deny handler wrote it even for a refusal and now writes
  it only for an `OK`.
  > **Status:** fixed (fix/union-offers)

## Showing a castle's war schedule from another thread closes a lock cycle (2026-09-24)

- **`Zone::heartbeat` takes a castle scheduler's mutex under the zone's
  own, and `WarScheduler::makeGCWarScheduleList` takes the war system's
  mutex under the scheduler's (it adds the race war's line), while the war
  heartbeat locks castle and holy-land zones under the war system's
  mutex,** so a schedule shown from any thread but the castle's own group
  thread can close the cycle zone → scheduler → war system → zone. Every
  seed `ShowWarSchedule` NPC stands inside its own castle zone, so the
  action runs on that thread today and the cycle was latent. The race war's
  line needs nothing from the scheduler and is now asked for after its
  mutex is released, so the scheduler → war system edge is gone. The castle
  war start ran under the same mutex too (`Scheduler::heartbeat` executes
  the due work inside it), broadcasting to every group and working on the
  castle's dungeons and siege zone; a due war is now taken out of the queue
  under the mutex and started after it (`Scheduler::popDueSchedule`,
  `Schedule::run`), and put back if the start throws. Under a scheduler's
  mutex only the database and the guild manager's and a guild's mutexes are
  taken now, and `WarSystem.h` records the order without the castle-thread
  restriction.
  > **Status:** fixed (fix/castle-balance-schedule)

## A castle war's end handles the castle's zones from the main thread (2026-09-24)

- **Besides the owner change, `GuildWar::executeEnd` returns the castle's
  symbols (`returnAllCastleSymbol`) and restores the guard shrine's shield
  (`addShrineShield`), and `SiegeWar::executeEnd` resets the siege zone
  (`SiegeManager::reset`: every monster killed, every player transported
  out), all on the main thread,** while those zones' threads run them. The
  shrine and symbol paths take the zone's own mutex, which excludes the
  zone's heartbeat but not its group's CG handlers; the siege reset takes no
  lock at all. The race war's start and end do the same across the holy
  land. Posting them to the owning groups is not one command per war: a
  castle symbol may lie in any zone or sit in a player's inventory in any
  group, so each return has to reach the group that holds it, which the
  item position loaders do not say today. Every such change is now posted to
  the owner (`war/WarZoneWork.h`), capturing ids only. A relic's return
  reads its row, which names a zone (the ground, a corpse) or a player (the
  inventory, the mouse), and posts the step that takes it out to that
  zone's group or through the player's mailbox; the step takes it only if
  it is still that item, follows the row again if it moved, and hands it on
  to the guard shrine through `Zone::transportItemToCorpse`, whose effects
  carry it to the shrine zone's own thread. The shields, the siege zone's
  set-up and reset, the castles' safe zones and transports, and the holy
  land's time, monsters, players, towers and join flags go one command per
  owning group.
  > **Status:** fixed (fix/war-end-zones)

## A castle's balance row can be saved out of order (2026-09-24)

- **Each change to a castle's tax balance saves the balance it produced
  (`TaxBalance=%d`),** and two changes on two zone threads -- a shop's tax
  and a resurrection fee, say -- may save theirs in the opposite order to
  the one they were made in, leaving the row one change behind the balance
  in memory until the next change saves again; a restart in between loses
  the difference. Every change now saves the amount it actually applied,
  relatively (`TaxBalance = TaxBalance + delta`, `addCastleTaxBalance` in
  the castle seam): the compare-and-swap reports what it moved after its
  clamps at zero and at the maximum, the four save paths are one
  (`CastleInfoManager::increaseTaxBalance`/`decreaseTaxBalance`, which the
  withdrawal now goes through too), and the owner change's reset saves the
  debit of what it took (`takeTaxBalance`, an exchange) in the same
  statement as the new owner. Row and memory are then the load's value plus
  the same changes, so the order the saves land in no longer matters, the
  reset included: a credit made just before it is taken with it and one made
  just after stays, whichever save lands first. Between saves the row can
  stand below zero (a withdrawal saved ahead of the credit it drew on), which
  the old `int unsigned` column refused as out of range, so `TaxBalance` is a
  signed `BIGINT` (`initdb/migrations/003-castle-tax-balance-signed.sql`) and
  the load clamps it into range. What remains is a save that never lands --
  a database error, or a crash between a change and its save -- which leaves
  the row off by that change until someone corrects it, where the absolute
  save used to be overwritten by the next change. The unused whole-row
  `CastleInfoManager::save`, which wrote the balance absolutely, is gone.
  > **Status:** fixed (fix/castle-balance-schedule)

## A castle's tax balance was read and rewritten by every thread that moved it (2026-09-24)

- **`CastleInfo::increaseTaxBalance` and `decreaseTaxBalance` read the
  balance, computed the new one and stored it back with no lock, while a
  castle-taxed shop (`CGShopRequestBuyHandler`), a resurrection fee charged
  in the resurrecting player's group, a guild master's withdrawal and a
  war's registration fee each move it from their own zone thread,** so two
  changes at once could lose one; the `Ex` forms also formatted the row's
  update into a static buffer every thread shared. The balance is an atomic
  changed by compare-and-swap, each change returning the balance it
  produced, and the buffers are local. Found with the castle-state entries
  below.
  > **Status:** fixed (fix/war-threads)

## Adding a queued war locked zones under the war queue's mutex (2026-09-24)

- **`WarSystem::addQueuedWar` held `m_MutexWarQueue` while it added each
  queued war, and adding the race war keeps only its participants in the
  holy land (`remainRaceWarPlayers`, under each holy land zone's own mutex),
  while a castle zone -- every castle is holy land -- hands its starting war
  over through `addWarDelayed`, which takes the queue's mutex, from inside
  its heartbeat, which holds that zone's mutex:** a deadlock when a castle
  war starts as the race war is added with the participant limiter on. The
  queue is swapped out under its mutex and the wars are added after it is
  released, so the queue's mutex is a leaf, as the lock order in
  `WarSystem.h` records.
  > **Status:** fixed (fix/war-threads)

## A union offer never expires (2026-09-24)

- **`GuildUnionOffer` holds one row per guild (its key is `OwnerGuildID`),
  and the only statement that ages rows out, `deleteStaleOffers`, runs
  after the pending-offer check has already found the guild has none,** so
  it never deletes anything. A JOIN offer the union master never answers
  keeps the applicant at `ALREADY_OFFER_SOMETHING` for good, and keeps the
  union it targets alive (a pending JOIN offer is what a memberless union
  exists for). An ESCAPE row, the ten-day penalty a guild that left a
  union by force gets, is an offer row too, so the same check answers
  first: the guild hears `ALREADY_OFFER_SOMETHING` rather than
  `YOU_HAVE_PENALTY`, and after the ten days as well, since nothing
  deletes an ESCAPE row -- `clearOffer` runs only on a guild with a JOIN or
  QUIT row, which the key rules out. Seventeen seed guilds carry one
  from 2006-2007. Making offers expire means purging a guild's stale rows before
  the checks (in `GuildUnionManager::recordJoinOffer`, under the manager
  mutex), dissolving the union a purged JOIN offer leaves abandoned, and
  deciding whether the penalty should be answered before the pending-offer
  check; that is a decision about the offer's lifetime, not a mechanical
  move.
  An offer lives ten days, the age the old statements already used, stated
  once in `GuildUnion.h` and spelled `kUnionOfferLifetimeDays` in every
  statement that ages a row. `GuildUnionManager::purgeOffers` deletes every
  expired row, of any type, under the manager mutex and applies the
  abandoned-union rule to each union an expired JOIN offer named; it runs
  before `recordJoinOffer` reads its facts, before the four answers and
  `offerQuit` read an offer, before a union's offers are listed to its
  master, and at every load, so a restart carries no expired offer. The
  startup load owes the other game servers a refresh for a union it
  dissolves and sends it once the login link is up
  (`sendOwedRefresh`); a reload tells them at once. The pending-offer count
  that keeps a union (`countPendingJoinOffers`) counts only unexpired rows,
  so the rule holds between purges too. `decideUnionJoinOffer` answers the
  penalty before the pending-offer check, so a guild inside its ten days
  hears `YOU_HAVE_PENALTY`, and its ESCAPE row is purged after them. A QUIT
  row is a member asking to leave; it lapses the same way, leaving the
  guild a member free to ask again or quit by force. Every offer row in the
  seed was from 2006-2007 and is gone, with the 19 unions only those
  offers kept: the seed now holds what the server holds after its first
  load, 24 unions, each with a member, and no offer rows.
  > **Status:** fixed (fix/union-offers)

## The PC finder's removal tested the wrong iterator (2026-09-24)

- **`PCFinder::deleteCreature` found the account entry with `m_IDs.find`
  and then compared the name table's iterator against `m_IDs.end()`
  before erasing,** a comparison between two containers' iterators, which
  is undefined, and an erase of `end()` had the account entry been
  missing. It tests the account iterator now, and erases the entry only
  while it still names the creature being removed. The account table is
  what `de::postToAccount` looks players up in.
  > **Status:** fixed (fix/union-refresh)

## The Exchange point statements never reach the account database (2026-09-24)

- **The ledger statements ask `getConnection("USERINFO")`, whose string
  overload ignores its argument and answers the thread's DARKEDEN
  connection, where `initdb/` creates neither `AccountPoint` nor
  `PointLedger`,** so every ledger read throws and no buy gets past its
  decision: the `DatabaseError` leaves `CGExchangeBuyHandler` and the
  buyer is disconnected. `ExchangeRepository.h` and
  `PointTablesAreNotReachableOnTheConnectionTheyAskFor` have said so since
  the seam was cut; this entry records what the fix has to decide. The
  only account-database connection is `getUserInfoConnection()`, one per
  process and shared by every thread without a lock, so running the ledger
  on it from the zone threads would interleave their transactions on one
  session. It needs either an account connection per thread, registered by
  every thread that buys, the point tables moved beside the listings, or,
  where both schemas sit on one server, the statements naming the tables
  by schema (`USERINFO.PointLedger`) on the game connection, which also
  keeps a purchase in one transaction.
  The third was chosen: the point statements name `AccountPoint` and
  `PointLedger` by the account schema (`UI_DB_DB`) on the thread's game
  connection, so a purchase is one transaction on one connection and the
  transaction pair is one `START TRANSACTION`/`COMMIT`/`ROLLBACK`. The
  gameserver opens the ledger at startup (`ExchangeService::openPointLedger`),
  checking that both connections reach one server (`@@server_uuid`) and that
  every ledger statement shape runs there against no row; a failed check
  leaves the ledger closed, logs which check failed against which two
  configuration blocks, and `decideBuyListing` then refuses every buy as a
  database error instead of disconnecting the buyer. The MySQL tier runs
  every purchase case on the tables `initdb/USERINFO.sql` creates, and
  `ALedgerOnASchemaWithoutThePointTablesStaysClosed` pins the old shape.
  > **Status:** fixed (fix/exchange-account-db)

## Two point adjustments of one account could lose one (2026-09-24)

- **`adjustPoints` read the balance with a plain `SELECT` and wrote the
  new one back whole with `REPLACE`,** so two adjustments of one account in
  flight together — two purchases paying one seller from two zone threads —
  each wrote their own sum, and the first was lost while its ledger row
  still recorded it. The read is `SELECT ... FOR UPDATE` now: inside a
  transaction the second adjustment waits for the first to end and adds to
  its result. In autocommit mode the lock ends with the read; the service's
  own `ExchangeService::adjustPoints` runs that way and has no caller.
  `ConcurrentAdjustmentsOfOneAccountBothCount` in the MySQL tier runs the
  two from two threads.
  > **Status:** fixed (fix/exchange-rollback)

## Four CG handlers cast the player to a creature class it never is (2026-09-24)

- **`CGExchangeBuyHandler`, `CGExchangeListHandler`, `CGQuitGuildHandler`
  and `CGExpelGuildMemberHandler` did `dynamic_cast<PlayerCreature*>` on
  the `Player*` they are dispatched, a `GamePlayer`, which no class shares
  with `PlayerCreature`,** so the cast answered NULL on every packet: the
  two Exchange handlers returned before reaching the service, and the two
  guild handlers asserted, so quitting a guild and expelling a member never
  ran. All four reach the creature through the game player
  (`dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature())`), as the
  other CG handlers do.
  > **Status:** fixed (fix/exchange-residue)

## The guild resync handler kept every popped record (2026-09-24)

- **`SGGuildInfoHandler` pops every `GuildInfo2` and `GuildMemberInfo2`
  out of the packet, copies it into a `Guild` or `GuildMember`, and never
  deletes it,** so the whole guild table leaked on each resync from the
  sharedserver. Each record is deleted once copied.
  > **Status:** fixed (fix/exchange-residue)

## A database error inside an Exchange purchase leaves its transactions open (2026-09-24)

- **`ExchangeService::buyListing` runs its steps inside transactions on
  the thread's two connections with no handler around them, and `END_DB`
  rethrows a `DatabaseError`,** so a duplicate ledger key from
  `adjustPoints` or the `ExchangeOrder.ListingID` unique violation from
  `createOrder` leaves both transactions open; the thread's next
  `START TRANSACTION` commits them implicitly, which can commit a point
  adjustment with no order behind it. The purchase's writes are
  `completeExchangePurchase` (`exchange/ExchangePurchase.h`) now, under an
  `ExchangeTransaction` guard that rolls the pair back on every way out
  but its commit: a refusal, a thrown `DatabaseError`, any other
  exception. A `DatabaseError` from any step, the begin and the commit
  included, comes back as the purchase's refusal rather than disconnecting
  the buyer. The pair issues each statement once per distinct connection,
  and today both names reach the thread's DARKEDEN connection, so a
  purchase is one transaction and its commit is atomic. The steps run
  market first — the claim (`markListingSold`, which now answers whether it
  matched an ACTIVE row, and holds that row's lock), then the order — and
  ledger second, the buyer's debit then the seller's credit; the commits
  run market first too. (The ledger has since been reached on the same
  connection, so a purchase is one transaction and the two-commit argument
  is gone from `ExchangeRepository.h`.) The collisions are refusals: two buyers of one
  listing, on one server or two of a world, meet at the claim, where the
  second waits for the row lock and is refused as no longer available
  before either reaches the order's or the ledger's unique key; a ledger
  key another purchase holds (a client key reused across listings) fails
  its leg, and a probe of the ledger after the rollback refuses it as a
  replay; a debit the balance no longer covers is refused as such.
  `tests/exchange_decision_test.cpp` pins the failure-to-refusal map, the
  guard and a failure at every step against the fake, which now honours
  the transaction; the MySQL tier runs a failing order write, a refused
  ledger leg and an unreachable ledger through real transactions and
  checks that the next transaction on the connection commits nothing of
  them.
  > **Status:** fixed (fix/exchange-rollback)

## A war's end reloads the castle's schedule on the main thread (2026-09-24)

- **`GuildWar::executeEnd` and `SiegeWar::executeEnd` run on the main
  thread under `WarSystem::m_Mutex` (from `WarSystem::heartbeat`) and call
  `CastleInfoManager::modifyCastleOwner`, which, when the winner's race
  differs from the owner's, reaches `cancelGuildSchedules()` and so
  `WarScheduler::load()`,** the reload that frees the wars the castle
  zone's thread executes. It could deadlock as well: the reload takes the
  castle scheduler's mutex under `WarSystem::m_Mutex`, and a zone thread
  building a castle's war schedule list
  (`WarScheduler::makeGCWarScheduleList`) holds that scheduler's mutex
  while it asks the war system for the race war's line. Both `executeEnd`s
  still decide the winner on the main thread, and post the rest to the
  castle's group through `CastleInfoManager::postCastleWarEnd`, capturing
  values only: the owner change, then the registration fee credited to the
  balance the change has just reset -- the order the two always had, which
  the fee depends on; a war that changed no owner credits the fee alone.
  The end message, the symbol return and shrine shield, the siege zone's
  reset and the history row stay on the main thread and come first; the
  change lands at the top of the castle group's next tick, and until then
  the castle keeps its old owner with the war already over. Nothing on the
  main thread reads the owner after `executeEnd`: the heartbeat drops the
  war from its lists and frees it, and the war list it rebuilds no longer
  holds it. The history row records the winner decided on the main thread,
  not the castle's state; the external `recordGuildWarHistory.py` it runs
  afterwards is not in the tree, so whether it reads the castle row, which
  now lags by a tick, is not known. A winning guild deleted between the
  war's end and the command hands the castle to the winner race's common
  guild (`castleWarWinnerOwner`), the owner the deletion's own change makes
  of it, so either order of the two ends the same;
  `tests/scheduler_test.cpp` pins both. The relays still go out from the
  main thread: the NetMarble `*world` relay and the siege's `GGCommand` to
  the castle-following servers both arrive as the GM `setCastleOwnerGuild`
  command, which posts the change to the receiving server's castle group;
  that relayed command applies no `castleWarWinnerOwner`, so a winner
  deleted around the relay's flight leaves the remote castle with its old
  owner, or hands it to a guild that is gone, a window only the NetMarble
  relay and the castle-following servers have. Until the posted command
  runs, at the top of the castle group's next tick, the castle keeps its
  old owner, and a withdrawal or a fee credited in that tick is reset by
  the change. With every caller on the castle's thread, `modifyCastleOwner`
  asserts the castle group's ownership (`ZoneGroup::assertOwned`, Debug
  builds).
  > **Status:** fixed (fix/war-threads)

## Retired unions accumulate on every reload (2026-09-24)

- **`GuildUnionRegistry::replaceAll` retires every live union on every
  `load()`, and a reload follows every union change on any game server of
  the world** (`sendRefreshCommand()` makes the others reload, and three CG
  handlers reload themselves), so each reload keeps one more copy of every
  union -- with the 116 unions the seed held then, about 20 KB per reload
  per server --
  until the process exits. `replaceAll` now keeps every live union whose
  id and master the tables still hold -- the same object, so a pointer a
  reader took stays live -- and gives it the fresh member list in one step
  under the union's own mutex (`GuildUnion::replaceMembers`), so a reader
  copies either the old list or the new one. Only a union that vanished
  from the tables, or whose id now names another master, is retired, and a
  new one is published; the id and guild maps are rebuilt from the result
  in load order under the registry mutex, so the lock order (manager,
  registry, union) and a retired union's answers are unchanged.
  `tests/guild_union_registry_test.cpp` covers a kept union (same pointer,
  new members), a vanished one, a changed master, a new one, a hundred
  reloads that retire nothing, and readers holding a kept union across two
  thousand reloads; `retiredCount()` is what they measure.
  > **Status:** fixed (fix/union-refresh)

## A player's guild id was read from other threads while its owner wrote it (2026-09-24)

- **`PlayerCreature::m_GuildID` was a plain `WORD` that the thread owning
  the player writes -- a guild joined or left, or deleted by the
  sharedserver (`SGDeleteGuildOKHandler`'s posted commands) -- while
  `PCFinder::getGuildPlayerNames_LOCKED` reads it for every logged-in
  player on whatever thread walks a guild,** holding the finder's lock,
  which the owner does not take to write it: a data race on every union
  broadcast and every guild recall. The finder's lock keeps the creature
  alive, not its fields. The member is a `std::atomic<GuildID_t>` now, its
  accessors unchanged. Found while converting the guild walk's callers.
  > **Status:** fixed (fix/union-lifetime)

## A GM reload of a castle's war schedule freed wars under the castle's thread (2026-09-24)

- **The GM `reloadinfo war_schedule_info` event ran `WarScheduler::load()`
  on the main thread,** and the reload clears the scheduler, deleting every
  war it holds, while the castle zone's thread runs that scheduler's
  heartbeat and the castle NPCs' quest actions (`ActionRegisterSiege`, the
  reinforcement actions, `ActionAskVariable`) use its next war without the
  scheduler's mutex. The event now posts the reload to the castle zone's
  group with `ZoneGroup::post()`, capturing the zone and looking the
  scheduler up inside, as the guild deletion's cancel does, and the GM's
  answer with the reloaded count goes to the GM by name through
  `de::postToPlayer` (`Scope::Player`) once the reload has run. Two
  behaviours change with it: an unknown zone id, which the lookup throws as
  an `Error`, is answered with the "no such zone" message the event always
  meant to send instead of escaping the event, and a reload raised with no
  GM player now reloads where it used to do nothing. `load()` had one more
  caller on a foreign thread: the GM `setCastleOwnerGuild` command names any
  castle by id and ran `CastleInfoManager::modifyCastleOwner` on the GM's
  own zone thread, and a change of the owning race reaches
  `cancelGuildSchedules()` and so `load()`. That change is posted to the
  castle's group now too. A war's end takes the same path from the main
  thread (`WarSystem::heartbeat` runs `executeEnd`, whose owner change
  reloads when the winner's race differs from the owner's); that one is
  posted to the castle's group as well now, see "A war's end reloads the
  castle's schedule on the main thread". The rest of a
  castle scheduler's readers are on its group's thread or take the
  scheduler's mutex (`makeGCWarScheduleList`, `hasSchedule`), and a war the
  scheduler hands to `WarSystem` has been popped from it first, so a reload
  cannot free it.
  > **Status:** fixed (fix/war-residue)

## A castle war's schedule row could be saved only for a siege (2026-09-24)

- **`WarSchedule::save()` cast its war to `SiegeWar` and asserted the
  cast,** while a `GuildWar` has lived in a `WarSchedule` since a 'GUILD'
  row reloads as one. Its one caller, the join branch of
  `ActionRegisterSiege`, reaches it only with a siege, so the assert never
  fired; the first caller to save a guild war's schedule would have thrown.
  `War` now names a schedule row's attackers itself --
  `getAttackerCount()` and `getAttackerGuildIDAt(slot)`: one guild in the
  first slot for a war with a single attacker, the challengers in joining
  order for a siege -- and `save()` builds the row from the war's virtuals
  with no cast, so a guild war rewrites exactly the row `create()` wrote
  for it: `AttackerCount` 1 (the column default `create()` leaves it at),
  its one guild, zero in the other four slots, its fee and the kind
  'GUILD'. A siege writes what it wrote before.
  `tests/integration/mysql_repository_test.cpp` pins that those values
  write back the row the insert wrote, column for column, and reload with
  the kind a `GuildWar` is rebuilt from. The failure log `save()` wrote in
  Korean under `create()`'s name now names `save()`.
  > **Status:** fixed (fix/war-residue)

## The race war flag was a plain bool every zone thread read (2026-09-24)

- **`WarSystem::m_bHasRaceWar` was a plain `bool` that the main thread's
  heartbeat writes as a race war starts and ends, and some twenty call
  sites on the zone threads read through `hasActiveRaceWar()` with no
  lock,** a data race; `m_bRaceWarToday`, the twenty- and five-minute
  warnings behind `canApplyBloodBibleSign()` and `isSkyBlack()`, and the
  race war's time parameter were read the same way. All five are atomics
  now; their writes already ran under `m_Mutex`. The flag is a hint, and
  the header says so. Of its readers only `mayModifyShrineOwner` goes on
  to use the war, and it took the pointer from `getActiveRaceWar()`, which
  released `m_Mutex` before returning, and asked the war afterwards, while
  the heartbeat frees the race war under `m_Mutex` when it ends. It now
  looks the war up and asks it under the lock -- `RaceWar`'s answer reads
  only the variables table and the player's flag, so no lock order
  changes -- and `getActiveRaceWar()`, left without a caller, is gone.
  > **Status:** fixed (fix/war-residue)

## A running castle war is handed out past the lock that frees it (2026-09-24)

- **`WarSystem::getActiveWar(zoneID)` finds a castle's running war under
  `m_Mutex` and returns the pointer after releasing it,** and its callers
  on the zone threads use it afterwards: `isModifyCastleOwner` asks the war
  whether a player takes the castle, `CastleShrineInfoManager` reads its
  type when a castle symbol is picked up, and `ActionEnterSiege` asks the
  siege for the player's side. The main thread's heartbeat frees a castle
  war under `m_Mutex` when its hour runs out, so a use that straddles the
  end reads a freed war. `getActiveWar` is gone, and with it
  `getActiveWarSchedule`, which handed out the schedule the same way and
  had no caller: a running war is found only under `m_Mutex`
  (`getActiveWar_LOCKED`, protected) and asked there. `isModifyCastleOwner`
  looks the war up and asks it under the lock, answering no when no war
  runs rather than asserting; `getSiegeGuildSide` answers
  `ActionEnterSiege`'s side question the same way; and the castle gate
  (`canPortalActivate`) and the symbol pick-up (`canPickupCastleSymbol`,
  which has no caller today) only ever asked whether a war ran, which
  `hasCastleActiveWar` answers from its own list. The lock order is
  written in `WarSystem.h`. The heartbeat holds `m_Mutex` while it locks
  zones (`Zone::lock`) to return castle symbols and blood bibles and to
  restore shrine shields, so a thread holding a zone's own mutex -- which
  `Zone::heartbeat` holds over NPC, monster and effect processing -- must
  not take `m_Mutex`: that is the deadlock the old comment meant, reached
  from `EffectHasBloodBible::affect` in the original, and the reason
  `hasCastleActiveWar` reads a list under a leaf mutex. The heartbeat
  takes no zone group's mutex and, with the owner change posted, no castle
  scheduler's, so taking `m_Mutex` under a group's mutex is safe, and every
  caller of the two questions is a CG handler or a quest action answering a
  player (`CGRelicToObjectHandler` through `SiegeManager::putItem`, and
  `CGNPCAskAnswerHandler` for `EnterSiege`), which holds its group's mutex
  and no zone's. The relic handler's `endWar`, right after the same
  question, has taken `m_Mutex` from there all along. The answers read the
  castle table and the player, and take no lock of their own.
  > **Status:** fixed (fix/war-threads)

## A deleted guild's reinforcement registrations outlived it (2026-09-24)

- **`purgeGuild`, the sharedserver's deletion of a guild everywhere, left
  the guild's `ReinforceRegisterInfo` rows,** and the gameserver's
  `cancelGuildSchedulesOf` denies them only on the waiting wars of its own
  castles. A waiting registration is still offered to the castle owner
  (`loadWaitingReinforceGuild`), an accepted one is read back as a siege's
  reinforcement on any later reload (`loadAcceptedReinforceGuild`, which
  is not even scoped to a server), and a denied one keeps refusing a guild
  of the same id, which a restart can hand out again because a new guild
  takes the highest stored id plus one. `purgeGuild` now deletes every row
  that names the guild as the reinforcing guild, in every status and on
  every server, as a fifth statement after the schedule cancel. The
  gameserver's deny stays: whichever of the two runs first, the reloaded
  siege comes back without the reinforcement. A siege already under way
  keeps it in memory until it ends, as it keeps a deleted challenger.
  Pinned by `tests/integration/mysql_sharedserver_repository_test.cpp`.
  > **Status:** fixed (fix/war-residue)

## A deleted guild's castle changes hands on the shared-server link thread (2026-09-24)

- **`GuildManager::deleteGuild` turns a castle the deleted guild held into
  its race's common castle by calling `CastleInfoManager::modifyCastleOwner`
  on the shared-server link thread,** which writes the castle's
  `CastleInfo` and broadcasts the tax change into the castle's zone
  (`setItemTaxRatio`, `Zone::broadcastPacket`) while the castle's zone
  thread runs it. The race does not change on this path, so it never
  reaches the war scheduler reload the GM command could. The change now
  rides the command the deletion already posts to every castle's group for
  the scheduler cancel, ahead of the cancel, and is decided there
  (`CastleInfoManager::settleGuildDeletion` over
  `castleOwnerAfterGuildDeleted`): the castle turns its race's common
  castle only if the deleted guild still holds it when the command runs,
  so a war the guild won a moment earlier, whose change reached the
  mailbox first, is turned common too, and a change that handed the castle
  on is left alone. The command captures ids only; a change to a common
  guild looks no guild up, and posting takes no group mutex, so nothing
  new is taken under the guild manager's mutex. Who owns the castle's
  state is settled with it: every owner change -- the war's end, this
  deletion, the GM commands -- runs on the castle's own thread, and the
  item tax ratio is set from the castle's own zone, so the owner, race,
  entrance fee and ratio have one writer, and they are atomics every other
  zone thread reads without a lock, getting a value some writer stored, two
  reads possibly straddling a change, as with the race war flags. The tax
  balance has many writers and is changed by compare-and-swap (above).
  > **Status:** fixed (fix/war-threads)

## Every variable without a stored row read zero instead of its default (2026-09-24)

- **`VariableManager::load()` replaced the table the constructor had
  filled with the coded defaults by a zeroed one before applying the
  `AttrInfo` rows,** so a variable with no row read 0. The stock seed
  stores ids 1 to 142 and has no row for id 0, `STAR_RATIO`, whose default
  is 1000 and which read 0; a GM command only displays it. The rule is
  explicit now: a row overrides the default and a missing row keeps it,
  and the table still reaches the highest stored id, so the seed's rows
  past the last named variable (136 to 142) stay readable by number. The
  overlay is a pure function beside the manager, `overlayStoredVariables`,
  pinned by `tests/repository_test.cpp`, and it shares its one refusal, a
  `RACE_WAR_TIMEBAND` outside 0 to 3, with `setVariable()` through
  `isAcceptedVariableValue`, so a refused stored timeband keeps the
  default 2 where it used to leave 0. The load no longer writes each row
  back to itself through `setVariable()`.
  > **Status:** fixed (fix/war-residue)

## A keyless Exchange buy was not replay-proof (2026-09-24)

- **The key a buy is recorded under did not make a repeated buy a
  replay.** The client sends `CGExchangeBuy` with an empty idempotency key,
  so `ExchangeService::buyListing` minted a fresh key per request from the
  time, a counter, the process id and `_getServerID()` -- a literal 1 -- and
  the double click the field exists for never met its own key. The same
  key was not unique across game servers either: every server answered
  server 1, and every containerised one is pid 1. `decideBuyListing`'s
  replay check looked the bare key up while only the suffixed `_buy` /
  `_sale` rows are ever written, so it could not fire. A keyless buy is now
  recorded under a key derived from what the repeat repeats:
  `EX_` + the configured `WorldID` and `ServerID` (two hex digits each) +
  the listing id (sixteen), 23 characters
  (`exchangeServerIdempotencyKey`). A listing sells at most once
  (`ExchangeOrder.ListingID` is `UNIQUE` and nothing returns a listing to
  active), so the key names one purchase, and the existing
  `UNQ_Ledger_IdempotencyKey` plus `adjustPoints`' own check refuse the
  second click with no schema change. The replay check looks up the buyer's
  row under the key `buyListing` writes (`exchangeLedgerKey(key, "_buy")`),
  and a client key is recorded under a `C_` prefix of its own, so that no
  purchase can plant the key another listing's keyless buy derives in any
  letter case, the ledger's collation folding case (the column holds 64
  characters, so a client key past 57 overflows it with its suffix, as a
  key past 59 did before; the client sends none). A second buyer of a
  listing already sold is refused as a replay rather than as unavailable,
  since the key names the listing, not the buyer; the client shows neither
  message today. The
  key never travels: `GCExchangeBuy` carries no key and no golden holds
  one. `_getServerID()` is `_marketServerID()` now, still 1 by design --
  it scopes listings, orders and the browse, which every game server shares
  -- and the list handler browses through it instead of its own literal.
  Pinned in `tests/exchange_decision_test.cpp`.
  > **Status:** fixed (fix/exchange-residue)

- **The client sends no key** (`client/VS_UI/src/UiRuntime.cpp`,
  `RequestExchangeBuy` never calls `setIdempotencyKey`). With the server
  deriving a per-listing key that is no longer a defect: a client key
  would only replace one the server already derives.
  > **Status:** not a defect

## The Exchange browse dropped the seller filter it was sent (2026-09-24)

- **`CGExchangeListHandler` passed every field of `CGExchangeList` to
  `ExchangeService::getListings` except the seller filter**, which the
  service has filtered on since the seam extraction. The handler builds the
  whole filter from the packet now (`exchangeListingFilterOf`), and the
  match itself (`matchesExchangeListingFilter`) is in `ExchangeDecision`,
  pinned in `tests/exchange_decision_test.cpp`. An empty filter matches
  every seller, so what the client sends today browses exactly as before.
  The filter still narrows the page after it is read, as the item and price
  filters always have: a filtered page is the matching part of that page.
  > **Status:** fixed (fix/exchange-residue)

- **The client could not send a seller filter**: its `ExchangeFilter`
  (`client/VS_UI/src/header/UiRuntime.h`) had no seller field and
  `RequestExchangeList` (`client/VS_UI/src/UiRuntime.cpp`) never called
  `setSellerFilter`. Both carry it now (client branch
  `fix/server-round-48-reads`), empty meaning no filter, which is what the
  Exchange window sends: it has no filter controls at all, and in the live
  client it is not reachable (`RunPointExchange()` has no caller and the
  list packet's handler is a no-op), so the control is the remaining piece.
  > **Status:** fixed (client fix/server-round-48-reads)

## GCExchangeBuy's success message was the order id again (2026-09-24)

- **A successful buy put the order id in the message field as a decimal
  string**, beside the same id in its own field, while every refusal puts
  its `formatExchangeError` text there and `ExchangeDecision.h` names those
  texts, `"Success"` included, as what the packet carries. The handler's
  comment said the client reads the decimal; the client reads nothing:
  `client/Client/PacketHandler/GCExchangeBuyHandler.cpp` has an empty body.
  Success carries `"Success"` now. The layout is untouched (a BYTE-length
  string either way) and the goldens are built from the test fixture, not
  from the handler, so none changes.
  > **Status:** fixed (fix/exchange-residue)

- **The client acted on no `GCExchangeBuy`**: its handler
  (`client/Client/PacketHandler/GCExchangeBuyHandler.cpp`) was an empty
  stub, so neither the result nor the message reached the player. It shows
  the message in the free message dialog and refreshes the Exchange
  window's listing on success (client branch `fix/server-round-48-reads`).
  > **Status:** fixed (client fix/server-round-48-reads)

## A statement of exactly 2048 characters was truncated and executed (2026-09-24)

- **`Statement`'s three printf-style forms formatted into a 2048-byte
  window and refused only a result longer than 2048**, so a statement of
  exactly 2048 characters came back from `vsnprintf` cut to 2047 and was
  stored and executed. One helper formats all three now, into a buffer
  sized from `Statement::kMaxStatementLength` (2048) plus the terminator,
  and refuses anything longer with an `Error` before it is stored or run;
  a statement of the full length is kept whole. On the way: the refusal
  threw with the `va_list` still open, and the formatting constructor left
  the connection and result pointers uninitialised for the destructor to
  delete (it has no caller). Pinned in `tests/database_error_test.cpp`.
  > **Status:** fixed (fix/exchange-residue)

## Owned objects leaked on their error paths (2026-09-24)

- **`ExchangeService::prepareClaimList` never freed the listing
  `getListing` handed it for each paid order.** The service's `getListing`
  returns a `std::unique_ptr` now, its only callers being in the same file;
  the repository's raw-pointer contract is unchanged.
  > **Status:** fixed (fix/exchange-residue)

- **A packet whose `read()` threw was leaked by every connection reader
  that created it.** `GamePlayer::processCommand` leaked it when the read
  threw; `LoginPlayer::processCommand` also when its handler threw, since
  the history takes the packet only after dispatch;
  `GameServerPlayer::processCommand` (sharedserver) on either; and
  `SharedServerClient::processCommand` leaked every SG packet it read,
  having no delete at all. Each holds the packet in a `std::unique_ptr`
  until the history takes it or the handler has run. On the way:
  `SharedServerClient`'s two `filelog` calls named `%d`s they passed no
  argument for.
  > **Status:** fixed (fix/exchange-residue)

## GCExchangeList's maximum against the client's input ring (2026-09-24)

- **`GCExchangeList`'s 37114-byte maximum is 4.5 times the client's
  default 8 KB input ring** (`DefaultSocketInputBufferSize` in
  `client/Client/Packet/SocketInputStream.h`; the game connection starts
  at 32 KB, `Player::setSocket` at the default), which used to grow only
  opportunistically. The client's `SocketInputStream::fill`
  (`client/Client/Packet/SocketInputStream.cpp`) now grows a full ring by
  the socket's backlog up to `MaxSocketInputBufferSize` (16 MiB), and its
  wire-layout test checks that four maximum frames fit below that, so a
  full page arrives whole. Nothing to change on the server.
  > **Status:** fixed (client master, `2d04b713`)

## Two seed guilds each led two guild unions (2026-09-24)

- **`initdb/DARKEDEN.sql` gave guild 9648 two unions, 119 and 166, each
  with guild 11294 as its one member, and guild 13981 two empty ones, 141
  and 181.** Nothing in the code makes that state: `offerJoin` and
  `acceptJoin` answer `ALREADY_IN_UNION` for a guild already in a union,
  `offerJoin` reuses the union the master guild already leads rather than
  making another, and `GuildUnionRegistry::addMember` refuses a guild the union
  holds. It only reads inconsistently: `GuildUnionManager::load` walks
  `GuildUnionInfo` in the order InnoDB returns it, key order, and each
  union overwrites the guild map,
  so the running server put 9648 and 11294 in 166 while `loadUnionOfGuild`
  answers 11294's first member row, 119, and union 119 stayed loaded with
  no guild mapped to it. The later rows are gone -- `(166,9648)` and
  `(181,13981)` from `GuildUnionInfo`, `(166,11294)` from
  `GuildUnionMember`; no offer row names either union -- so the older
  union of each pair is the one left. `tests/ratchet/ratchets.sh` fails on
  a seed guild that belongs to more than one union as master or member.
  > **Status:** fixed (fix/manager-residue)

## A union join offer creates the union on one game server only (2026-09-24)

- **`GuildUnionOfferManager::offerJoin` creates a union for a master guild
  that leads none (`GuildUnionManager::recordJoinOffer` now, which checks
  and opens under the manager mutex, so two offers on one server open one
  union) and tells no other game server,** where `addGuild` and both removal paths
  end in `sendRefreshCommand()`. Each game server keeps its own copy of the
  union tables, so until something else refreshes them, a second offer to
  the same master guild made on another server of the world finds no union
  there and creates another. That is the likeliest source of the duplicate
  seed unions above, whose masters appear twice in `GuildUnionInfo`. The
  union is also created before the checks that can still refuse the offer
  (a pending offer, the escape penalty, a full union), so a refused first
  offer leaves an empty union behind, and an empty union keeps its master
  guild "in a union", so that guild can join no other. The rule is now a
  union exists for its member guilds and for the guilds asking to join
  it. `GuildUnionManager::recordJoinOffer` reads the facts, applies
  `decideUnionJoinOffer` (`gameserver/guild/GuildUnionJoinOffer.h`, pinned
  by `tests/guild_union_join_offer_test.cpp`: every refusal, in the order
  the client was always answered in, comes before a union is opened) and
  writes the union and the offer, all under the manager mutex, then
  refreshes the other game servers when it opened a union. Two windows
  stay: two offers to one master made on two servers within the refresh's
  flight still open two unions, and a union row is published only once
  its offer is on record, a failed offer insert deleting the row it
  opened. The last offer
  going takes an empty union with it: `denyJoin`, and `acceptJoin` when the
  join is refused after the offer is cleared, call `dissolveIfAbandoned`,
  which under the manager mutex dissolves a union with no member row and no
  pending JOIN offer (`unionIsAbandoned`) and refreshes the other servers.
  That replaces the deny handler's own count-then-delete of the
  `GuildUnionInfo` row, which dissolved a memberless union even with other
  offers pending and reloaded only its own server. The removal paths
  dissolve a union whose last member leaves even while JOIN offers to it
  are pending, which strands those applicants; that gap is older than this
  rule and has its own entry. Offers do not time out (see "A union offer
  never expires"), so that case does not arise.
  Of the seed's 116 unions, 73 had neither a member row nor a JOIN offer:
  their `GuildUnionInfo` rows are gone, leaving 43, of which the 19 without
  members each have a pending JOIN offer, and `tests/ratchet/ratchets.sh`
  fails on a seed union with neither.
  > **Status:** fixed (fix/union-refresh)

## The login link's LG handlers keep an incoming player past its lock (2026-09-24)

- **`LGIncomingConnectionOKHandler` and `LGIncomingConnectionErrorHandler`
  take a `GamePlayer*` from `IncomingPlayerManager::getPlayer` or
  `getReadyPlayer`, which release `m_Mutex` before they return, and then
  read its socket and status and write its reconnect packet and penalty
  flag on the `LoginServerManager` thread,** while the main thread may be
  disconnecting and deleting that player in its walks or in `heartbeat()`,
  where `disconnect()` also reads and deletes the reconnect packet. The
  lookup is locked; what follows it is not, and doing the work under
  `m_Mutex` would not close it either, since the walks disconnect a player
  before they take `m_Mutex` to remove it. Both handlers now post their
  work to the player's mailbox as a `Scope::Player` command through
  `de::postToAccount`, the by-account form of `de::postToPlayer`
  (`PCFinder::getCreatureByID_LOCKED` under the PCFinder lock; a logging-out
  player keeps its PCFinder entry until it is destroyed). The incoming
  manager's command walk runs it on the main thread right before
  `processCommand`, which sees the kick flag and disconnects the player in
  the same pass, sending the stored reconnect packet: the OK path's
  status check, reconnect packet, kick flag and bonus point and the error
  path's status assertion and kick are what they were. A reply that arrives
  while the player is still queued between managers waits in the box
  rather than being dropped. `getPlayer`, `getPlayer_NOBLOCKED` and
  `getReadyPlayer`, which nothing else called, are gone.
  > **Status:** fixed (fix/union-refresh)

## The incoming and sharedserver managers polled outside their mutex (2026-09-24)

- **`IncomingPlayerManager::pollSockets` and the sharedserver
  `GameServerManager::pollSockets` ran the whole poll with no lock, and the
  incoming manager's walks carried commented-out `m_Mutex` sections,**
  where `ZonePlayerManager` fills and collects under its mutex. Not a race
  in either: the thread that polls and walks is the only thread that
  changes the table, the descriptor range and the poll set -- the main
  thread for the incoming manager (accepts, the `heartbeat()` merge of the
  players zone threads queue, the removals in its walks and in `CGReady`'s
  handler, which it dispatches), the worker for the sharedserver -- and
  every one of those writes takes `m_Mutex`, which the login link's lookups
  take to read. Both polls now take the ZonePlayerManager shape anyway,
  `fill()` and `collect()` under `m_Mutex` and the wait without it, so a
  writer on another thread would be safe there. The walks stay unlocked, as
  ZonePlayerManager's do: they call `deletePlayer`/`addPlayer` (and the
  sharedserver's handlers `broadcast`), which take the non-recursive mutex
  themselves, and the incoming walks destroy players, which saves to the
  database under the player finder, guild and SharedServerManager locks --
  a zone thread queuing a player with `pushPlayer()` under its group mutex
  would wait on that. Each manager's header states the rule and the lock
  order; the commented-out sections are gone.
  > **Status:** not a defect (the polls take the ZonePlayerManager shape in
  > fix/manager-residue)

## The `DIST_DB_*` configuration block is read by nothing (2026-09-24)

- **The login and shared server configurations, in `conf/` and
  `docker/conf/`, carried a `DIST_DB_HOST`/`PORT`/`DB`/`USER`/`PASSWORD`
  block that no code reads.** The only connections the servers open are
  `DB_*` and `UI_DB_*` (`de::connectionSettings` in `DatabaseManager::init`);
  the gameserver's "dist" connection is built from `UI_DB_*`, and its own
  configurations never had the block. Nothing under `src/`, `tests/`,
  `docker/`, `tools/` or `initdb/` names the keys, and `docker/start.sh`
  reads `DB_HOST`/`DB_PORT` with an anchored match. The four blocks are
  gone; a deployment's own copy of them is ignored as before.
  > **Status:** fixed (fix/manager-residue)

## The union broadcasts took zone-group mutexes on the thread that called them (2026-09-24)

- **`sendGCOtherModifyInfoGuildUnionByGuildID` and its single-creature
  form walked the player finder with no lock, then took each member's
  zone-group mutex to broadcast in its zone, from whatever thread called
  them,** so the union teardown, called from `SGDeleteGuildOKHandler`
  under `SharedServerManager::m_Mutex`, took a group mutex that the zone
  threads hold while they send to the sharedserver through that same
  manager mutex -- the two orders the mailbox exists to keep apart. The
  quick-quit handler also handed the union master's `Creature*` to the
  broadcast after leaving the finder's critical section. Both helpers now
  name the guild's players under the finder's lock
  (`PCFinder::getGuildPlayerNames_LOCKED`) and post each broadcast to the
  thread that owns the player (`de::postToPlayer`), which runs it under the
  owner's group mutex; the quick-quit handler broadcasts by guild id. On
  the way: a member row the union object had already lost was left in
  `GuildUnionMember` when only that guild's membership was removed, and
  goes now.
  > **Status:** fixed (fix/union-teardown)

## The player finder's guild walk is unlocked and hands out raw pointers (2026-09-24)

- **`PCFinder::getGuildCreatures` iterates the finder's table with no
  critical section, stops after examining its first `Num` players rather
  than after `Num` matches, and returns `Creature*`s the caller uses after
  the walk,** so a login or logout on another thread can invalidate the
  iteration or a pointer. Its two callers did take the finder's critical
  section around the walk and used the pointers inside it; what they did
  with them was the rest of the defect. The GM `*command GuildRecall`, on
  the GM's zone thread, deleted and added each member's siege effects under
  `Zone::lock()` -- the narrower zone mutex, which does not exclude the
  member's own zone-group tick -- and transported members out of zones
  ticking on other threads. `getGuildCreatures` is gone. The console
  command names the guild's players under the finder's lock
  (`PCFinder::getGuildPlayerNames_LOCKED`) and posts each recall, effects
  and transport, to the thread that owns that player (`de::postToPlayer`),
  considering every online member rather than the first 200 players the
  walk used to examine, and stopping after the requested number of posts
  (one when the count is below one). The other caller,
  `SiegeManager::recallGuild`, had no caller of its own and was deleted.
  The walk reads each player's guild id from other threads; that race is
  "A player's guild id was read from other threads", above.
  > **Status:** fixed (fix/union-lifetime)

## Accepting the last member's quit used the union after it was freed (2026-09-24)

- **`CGQuitUnionAcceptHandler` kept `pUnion` across `acceptQuit`, which
  dissolves and frees the union when the last member leaves, and then read
  `pUnion->getUnionID()` twice,** on the zone thread, on exactly the path
  that ends a union. The id is read before the call now and the pointer is
  not used past it.
  > **Status:** fixed (fix/union-teardown)

## A signal in flight disconnected every connected player (2026-09-24)

- **`select` leaves its descriptor sets untouched when it fails, and the
  four connection managers read them anyway,** so an interrupted call made
  every watched descriptor look ready in every set -- the sets were a copy
  of the membership the managers had just written into them. The
  out-of-band set is the one that hurts: each manager treats a descriptor
  in it as having sent OOB data and cuts that connection, so one `EINTR`
  disconnected every player the manager owned, `ZonePlayerManager` marking
  each of them `PENALTY_TYPE_KICKED` on the way out. It was reachable:
  `SIGTERM` and `SIGINT` are installed with `sa_flags` zero, and `select`
  is interrupted by a handled signal whether or not `SA_RESTART` is set, so
  a shutdown signal delivered to a zone thread inside the call kicked that
  group before the loop read the shutdown request. Found while replacing
  `select`. A failed wait now leaves every descriptor unready
  (`de::DescriptorPollSet::collect`), so the tick processes nothing and the
  next one asks again.
  > **Status:** fixed (fix/poll-descriptor-tables)

## A union's guild lookup is dereferenced unchecked (2026-09-24)

- **Four sites called `GuildManager::getGuild(id)->getMaster()` on an id
  read from the union tables without testing the result** --
  `GuildUnionManager::removeMasterGuild` three times
  (`src/server/gameserver/GuildUnion.cpp`) and `CGQuitUnionHandler`'s
  QUIT_QUICK branch on `pUnion->getMasterGuildID()`. `getGuild()` answers
  NULL for an id the table does not hold, and nothing kept the union rows
  in step with the guilds: `GuildManager::deleteGuild` carried the comment
  `// Clear the GuildUnion information` over no code at all, so a guild
  disbanded while it was in a union left `GuildUnionInfo` /
  `GuildUnionMember` rows naming it, and quitting that union dereferenced
  NULL on the zone thread. The shorter path needed no stale row:
  `SGDeleteGuildOKHandler`'s waiting-guild branch called `deleteGuild` and
  then `removeMasterGuild` for the same id (its active-guild branch did no
  teardown at all), so a waiting guild that mastered a union with members
  dereferenced NULL on the sharedserver-link thread the moment it was
  deleted.

  The teardown rule is now stated once, as a pure function
  (`decideUnionTeardown`, `gameserver/guild/GuildUnionTeardown.{h,cpp}`,
  covered by `tests/guild_union_teardown_test.cpp`): a member guild leaving
  gives up its own `GuildUnionMember` row and the union carries on, unless
  it was the last member, because the master guild alone is not a union; a
  master guild takes the union with it, since the tables name one master
  and nothing says which member would inherit it. `removeMasterGuild` is
  `removeGuildFromUnion`, which reads the union's id, its master's id and
  its member rows first, asks `decideUnionTeardown` what to do and then
  performs it -- so it needs no live `Guild*` for the guild going away.
  `SGDeleteGuildOKHandler` calls it before `deleteGuild`, for an active
  guild as well as a waiting one, so the guild masters it notifies are
  still in the `GuildManager`; `GuildManager::deleteGuild` does not do the
  teardown itself because it holds the guild mutex the notification reads
  under, and its comment now says so. A guild that is gone anyway is
  logged to `GuildUnion.log` and not notified, in
  `removeGuildFromUnion` and in `CGQuitUnionHandler`, which also stops
  writing the escape penalty for the guild id the packet carries rather
  than the one it took out of the union, and no longer abandons the quit
  half-done when the union master is offline.
  > **Status:** fixed (fix/union-teardown)

## A dissolved union leaves its member guilds pointing at the freed object (2026-09-24)

- **`GuildUnionManager` freed a union while its member guilds still
  reached it**: `removeGuild` and the old `removeMasterGuild` set
  `m_GuildUnionMap[gID] = NULL` for the guild that left -- and the master
  path set it for the guild it was called with, not for the member it had
  just removed -- then, once the union emptied, deleted the `GuildUnion`
  while every other member's entry still held the pointer. The next
  `getGuildUnion()` for one of those guilds answered with freed memory, on
  a zone thread, for every packet that reports a character's union
  standing. The lookups made it worse by reading the map with
  `operator[]`, which inserts: a lookup for a guild in no union left a NULL
  entry behind and could rehash the map under another thread's read.
  `destroyUnion()` now takes every guild of the union out of both maps and
  out of the list before the object goes, `removeGuild` erases the leaver's
  entry, and both lookups are `find()` on a const method.
  > **Status:** fixed (fix/union-teardown)

## A member guild's disbandment dissolved its whole union (2026-09-24)

- **`GuildUnionManager::removeMasterGuild` took the master branch for any
  guild the lookup map held,** and the map holds the union's member guilds
  as well as its master, so a member guild being disbanded emptied the
  union of every other guild and destroyed it. Only the else branch --
  reached solely by a guild the maps had already forgotten -- removed a
  single member. Found while giving the four unchecked `getGuild()` calls
  a rule to follow. The branch is `decideUnionTeardown`'s now, taken on
  `removedGuildID == unionMasterGuildID` rather than on the map holding an
  entry.
  > **Status:** fixed (fix/union-teardown)

## The guild union manager mutates its tables from any thread without a lock (2026-09-24)

- **`GuildUnionManager::m_Mutex` is taken by `reload()` and by nothing
  else,** so the union list and the two lookup maps are written by
  `removeGuildFromUnion` on the sharedserver-link thread, by
  `addGuild`/`removeGuild` and `reload()` on a zone thread, and read by
  every zone thread that builds a character's union standing --
  concurrently, with no lock between them, and with `SAFE_DELETE` of a
  `GuildUnion` among the writes. The tables are a `GuildUnionRegistry`
  now (`gameserver/guild/GuildUnionRegistry.{h,cpp}`, covered by
  `tests/guild_union_registry_test.cpp`), which takes its own mutex for
  every lookup and every change, and a union it lets go of -- dissolved,
  or replaced by a reload -- is retired rather than freed, as the guild
  managers do. A retired union keeps its id and its master: that is what
  a reader read a moment earlier, and all it hands back to the manager,
  which resolves the id again under the lock and finds nothing. It names
  no guild as a member (`hasGuild()` false, `getGuildList()` empty), and
  the two readers that act on the member list alone, union chat and the
  union info window, treat a retired union as none. A `GuildUnion` is an
  in-memory object only -- id and master fixed at construction, member
  list under its own leaf mutex -- and the manager writes the rows, except
  the count-then-delete of an emptied union's `GuildUnionInfo` row that
  four CG union handlers (deny, expel, quit, quit-accept) still run before
  their reload, outside the mutex. `GuildUnionManager::m_Mutex` now
  serialises the manager's changes, each holding it across its table
  reads, row writes and registry writes, so those no longer interleave:
  `offerJoin` could open two unions for one master
  (`openUnion` checks and opens under the mutex), and `acceptJoin` and
  `acceptQuit` answered OK for a join or quit a concurrent change had
  already made impossible, which they now report. The lock order --
  manager, registry, union, all taken after anything a caller holds, with
  nothing else taken under them -- is written in `GuildUnion.h`; the
  guild-master lookups, the notifications and the refresh to the other
  game servers run after the manager mutex is released. A reload reads the
  tables before it swaps anything, so a failed read leaves the old set in
  place rather than an empty one.
  > **Status:** fixed (fix/union-lifetime)

## A siege's challenger array is written one past its end (2026-09-24)

- **`SiegeWar::addChallengerGuild` refuses a challenger only once the
  count is already past five, so the sixth is written to
  `m_ChallangerGuildID[5]` of a five-element array,** and
  `WarScheduler::load` feeds it `AttackerCount` straight from the schedule
  row, reading the row's own five-slot array the same number of times. The
  registration paths happen to check the count themselves, so the overrun
  needs a row whose `AttackerCount` exceeds its filled slots — which the
  table's `int(10) unsigned` column allows and nothing validates. Found
  while giving the schedule row a kind. The bound is `>= 5` now and the
  loader clamps the count to the five slots the row has.
  > **Status:** fixed (fix/war-schedule-lifecycle)

## The player tables hold four times the descriptors an fd_set does (2026-09-24)

- **`PlayerManager`'s table has 2000 slots and the managers admit every
  descriptor below that into their `fd_set` members,** but `fd_set` is the
  platform's own and holds `FD_SETSIZE` descriptors, 1024 on glibc, which
  no build raises. A gameserver or loginserver that reaches a descriptor of
  1024 has `FD_SET` write past `m_ReadFDs`, `m_WriteFDs` and `m_ExceptFDs`
  into whatever follows them in the manager, and calls `select` with an
  `nfds` the interface does not define. The accepted-descriptor bound at
  `IncomingPlayerManager::acceptNewConnection` and the one in
  `PlayerManager::addPlayer` both say 2000, so nothing refuses the
  descriptors in between. The sharedserver was not affected: its table is
  100 slots and it refuses anything above them. The limit is gone rather
  than the players: the four managers multiplex with `poll` now, through
  `de::DescriptorPollSet` (`src/server/DescriptorPollSet.h`), which holds
  one slot per table slot and so watches every descriptor the table admits.
  Each tick it fills one `pollfd` per watched descriptor, waits once, and
  answers the same three questions the `fd_set` walks asked, in the
  directions the manager registered: readable, writable, and out-of-band
  data. `POLLERR`, `POLLHUP` and `POLLNVAL` come back unasked and are
  reported as ready to read, as `select` reported an errored or hung-up
  descriptor, and as ready to write as well, so the manager's own read or write
  runs and fails and its existing disconnect path takes the connection
  down; none of them is reported as out-of-band data, which only `POLLPRI`
  is. Dropping a descriptor drops what the last wait reported for it, which
  is what `FD_CLR` on the result sets did for a connection removed part way
  through a tick. `tests/descriptor_poll_set_test.cpp` covers the mapping,
  the empty set, a descriptor past `FD_SETSIZE`, and a real round against a
  connected pair.
  > **Status:** fixed (fix/poll-descriptor-tables)

## The client heartbeat is verified twice for every heartbeat packet (2026-09-24)

- **`GamePlayer::processCommand` ran `verifySpeed(pPacket)` and threw the
  answer away, and `CGVerifyTimeHandler` then ran it again on the same
  packet,** so the heartbeat state machine advanced twice per `CGVerifyTime`.
  The first run set the next expected time to *this* arrival plus the
  60-second interval, which makes the second run's test (`now > next - 5`)
  false for every heartbeat, so the run that actually decides always took the
  early branch: an on-time heartbeat decremented and incremented the error
  count in the same breath, and an early one counted twice. The run of early
  heartbeats needed to disconnect a player was two rather than the five the
  rule is written for. The discarded call arrived with the recovered sources
  (`3fccc6c6`) as part of the abandoned move/attack check below and is gone;
  the handler is the only caller again, as it was at import, and
  `tests/speed_hack_decision_test.cpp` pins the counting.
  > **Status:** fixed (fix/shrine-and-speed)

## A deleted guild's pending war comes straight back (2026-09-23)

- **`GuildManager::deleteGuild` reloads the castle zone's war scheduler
  when the guild has a war scheduled, but reloads it from the same table
  rows, where the war still waits,** so the same war returns. Only
  `cancelGuildWarSchedules` cancels rows, and this path never reaches it.
  The posted command now cancels before it reloads: a new
  `WarScheduler::cancelGuildSchedulesOf(guildID)` walks the waiting castle
  wars under the scheduler's own mutex, writes each one the guild attacks
  to 'CANCEL' through the repository's new
  `cancelWarSchedule(warID, serverID)`, denies the guild's reinforcement
  registration on every other one (`denyReinforceRegistration`, which also
  reaches a registration the defenders had not answered yet), and only
  then reloads, so the war cannot come back and a siege the guild merely
  reinforced keeps its challengers. A siege several guilds joined is
  cancelled whole, because a scheduled siege has no way to drop one
  challenger; a war already under way is left alone, since the zone thread
  is running it, and the log names only the rows that changed.
  The per-war cancel is used rather than `cancelGuildWarSchedules`, which
  would take every guild war of the castle. The sharedserver needs no
  telling: it is the side that deletes the guild in the first place, and
  its `purgeGuild` already cancels the rows the guild holds the first
  attacker slot of — the ones it joined as a later challenger or as the
  defenders' reinforcement are what this path now catches.
  > **Status:** fixed (fix/war-schedule-lifecycle)

## The sharedserver's descriptor walk starts from an unchecked listener (2026-09-23)

- **`GameServerManager` seeds its descriptor range from the listening
  socket's descriptor with no bound,** so a listener at or above the
  hundred-slot table makes every input and output walk read past it. The
  bound added for accepted connections covers player adds only. The
  gameserver's `IncomingPlayerManager` and the loginserver's
  `LoginPlayerManager` seed the same way over their 2000-slot player
  tables. All three now refuse at startup a listening socket their table
  cannot hold: clamping the walks alone would hide the listener from every
  one of them, and a manager that never sees its own listening socket
  accepts nothing. Beyond that seed, each sweep and each min/max rescan
  runs the range through `de::descriptorRange`
  (`src/server/DescriptorTable.h`), which clamps it to the table it
  indexes, so no walk can index past the table however the range was
  reached; `GameServerManager::deleteGameServerPlayer` refuses a
  descriptor outside the table the way its add already did.
  `tests/descriptor_table_test.cpp` pins the clamp, the listener case
  included.
  > **Status:** fixed (fix/config-and-listener)

## The account database connection takes the game database's port (2026-09-23)

- **`DatabaseManager::init` builds the USERINFO connection from the
  `UI_DB_*` host, database, user and password keys but reads its port from
  `DB_PORT`,** the game database's key, where the two other places that
  open a connection to that server read `UI_DB_PORT`. An account database
  on a port of its own is reached on the wrong one. Both connections are
  read as whole blocks now, by `de::connectionSettings`
  (`src/server/database/ConnectionSettings.h`), which takes the prefix and
  the five keys behind it, so one block's address cannot be built from
  another's; a block that names no port still means the driver's default,
  which is what the other two readers already did, so a site configuration
  that named only `DB_PORT` must name `UI_DB_PORT` for the account block
  now, as the shipped ones do. The login and shared
  server configurations repeated `DB_PORT` inside their `UI_DB` and
  `DIST_DB` blocks, which is where the confusion came from; each port key
  is named after the block it sits in now, at the same value, so those
  deployments reach the same ports as before.
  `tests/connection_settings_test.cpp` covers the block reader; the rest of
  `DatabaseManager::init` opens sockets to MySQL and runs its first query,
  so it has no seam a unit test can hold.
  > **Status:** fixed (fix/config-and-listener)

## The Netmarble flag is read with opposite senses (2026-09-23)

- **`GamePlayer::logLoginoutDateTime` selects the Netmarble dimension when
  `IsNetMarble` is zero, against its own comment,** while the other six
  readers of the flag treat nonzero as Netmarble. Every shipped
  configuration sets the flag to zero, so every login and logout row in the
  web log was written under dimension 2 in place of the configured one. The
  site takes the majority sense now, and all seven readers ask
  `de::isNetMarbleDeployment()` (`src/server/Deployment.h`), so the sense
  has one place to be wrong in; the three readers that tested `== 1` test
  nonzero with the rest now, the value being a single bit in every
  configuration. Nothing on the wire depends on it: the
  client decides its own Netmarble build from `Netmarble != 0` in its
  Netmarble.inf, the same sense, and a Netmarble client never parses a
  dimension out of its command line at all -- `g_Dimension` keeps the zero
  it was initialised with, and it only selects which login-server address
  list the client reads. The dimension the server writes is its own
  `Dimension` key, which every other reader of that key passes straight
  through.
  `ZoneLoad.cpp`'s pay-zone message was recorded here as a second reversed
  site and is not one: its zero branch sends STRID_CANNOT_ENTER_PAY_ZONE,
  which is what the file's other pay-zone portal sends unconditionally, so
  zero already meant "not Netmarble" there. Its comment says so now.
  > **Status:** fixed (fix/config-and-listener)

## An ousters saved an indeterminate silver damage on destruction (2026-09-23)

- **`Ousters::Ousters()` never initialized `m_SilverDamage`, and
  `~Ousters()` passes it to `saveExps`,** which writes it to the
  character's `SilverDamage` column. Only `load()` ever set the field, so
  a character constructed and destroyed without a successful load wrote
  whatever the allocation happened to hold. `Vampire::Vampire()` zeroed
  its own copy. The field now lives once on `PlayerCreature` with a zero
  initializer, which closes it for all three races.
  > **Status:** fixed (refactor/race-load)

## A guild's deletion walks and rebuilds the war schedules from another thread (2026-09-23)

- **`WarScheduler::hasSchedule` walks the recent schedules with no lock,
  unlike every sibling, and `GuildManager::deleteGuild` calls it from the
  shared-server link thread,** then on a hit calls `load()`, which clears
  and rebuilds the schedules, deleting war objects the zone thread may be
  about to execute under its own lock. Deleting a guild whose castle has a
  pending war frees a war under the zone thread's scheduler heartbeat. The
  fix is to post the cancel to the owning zone group rather than reach
  across. `hasSchedule` takes the mutex now, and the reload is posted to
  the zone's group with `ZoneGroup::post()`, so it runs on the thread that
  executes those wars, under the group mutex; the command captures the
  zone and looks the scheduler up again, because a zone reload replaces
  it. The GM `reloadinfo` of a war schedule (`EventReloadInfo`), which
  called `load()` from the main thread the same way, is posted to the
  castle's group too (fix/war-residue).
  > **Status:** fixed (fix/recorded-defects-6)

## The sharedserver's player table is indexed by raw descriptor with no bound (2026-09-23)

- **`GameServerManager::addGameServerPlayer` writes `m_pGameServerPlayers[fd]`
  into a table of a hundred slots with no check against its size,** and the
  input and output loops walk the table from the lowest to the highest
  descriptor seen. A game-server link whose socket descriptor is a hundred
  or more overruns the array. The add bound-checks the descriptor now, the
  way the gameserver's `PlayerManager` does, and the accept path logs the
  refused host and deletes the player, which closes the socket. The
  loginserver's `GameServerManager` owns a datagram socket and no table,
  and both `ClientManager`s reach their players through `PlayerManager`,
  whose add, delete and get all bound-check.
  > **Status:** fixed (fix/recorded-defects-6)

## A duplicate game-server connection would be freed twice (2026-09-23)

- **`GameServerManager::acceptNewConnection` frees the accepted socket and
  then the player built on it when the add reports a duplicate,** but the
  player's destructor closes and deletes that socket itself. Latent: the
  add never reports a duplicate today, having no duplicate check. The
  branch for a missing element also builds a message with the offending
  host and port that nothing prints. The socket now has one owner: the
  duplicate branch deletes only the player, the outer handler deletes
  whichever of the two exists, and the missing-element message goes to the
  log the file already writes.
  > **Status:** fixed (fix/recorded-defects-6)

## Registering a siege beside a scheduled guild war schedules a second war (2026-09-23)

- **`ActionRegisterSiege` looks up the castle's recent schedule, casts its
  war to `SiegeWar` and, when the cast fails, creates a new siege schedule
  instead of joining,** so a castle whose recent schedule holds a guild
  war gets two wars scheduled. The two registration actions are
  alternatives in quest data; it bites a server whose scripts use both.
  The registration now asks a decision function what the castle's next war
  means for it (`war/SiegeRegistrationDecision.h`, covered by
  `tests/scheduler_test.cpp`): nothing scheduled opens a siege, a siege
  with a free slot is joined, and anything else is refused. A guild war
  waiting on the castle is refused, because it is applied for by one guild
  and has no slot another can join, and merging one into a siege would
  have to rewrite its row under a new class; the refusal is the existing
  `NPC_RESPONSE_WAR_SCHEDULE_FULL`, which this action already sends when a
  siege has no slot left and which `ActionRegisterReinforce` sends when the
  castle's next war is not a siege at all, so the client needs no change.
  The shipped `Triggers` data registers
  `RegisterSiege` at all six castles and `WarRegistration` at none, so a
  stock server meets this only through a guild war left by a server whose
  scripts do use the other action.
  > **Status:** fixed (fix/war-schedule-lifecycle)

## A guild war reloads after a restart as a siege with no challengers (2026-09-23)

- **`WarScheduler::load()` rebuilds every `WAR_GUILD` schedule row as a
  `SiegeWar`,** and a guild war's row carries one attacker guild and no
  challenger count, so it comes back as a siege nobody attacks. No existing
  column tells the two apart: both write `WarType='GUILD'`, and a siege
  with a single challenger has the same `AttackerCount` of 1 and the same
  one filled attacker slot a guild war does, because `WarSchedule::create`
  never writes `AttackerCount` at all and the column defaults to 1. So
  `WarScheduleInfo` gained `CastleWarKind enum('GUILD','SIEGE')`
  (`initdb/DARKEDEN.sql`, migration
  `initdb/migrations/002-war-schedule-castle-war-kind.sql`), written by
  both schedule writers from the war's own
  `War::getCastleWarKind2DBString()` and read back by `loadWarSchedules`.
  `load()` builds a `GuildWar` with the row's single attacker guild and its
  fee for a 'GUILD' row and a `SiegeWar` with its challengers and accepted
  reinforcement for a 'SIEGE' one. The column defaults to 'SIEGE', which is
  the class every row was rebuilt as before it existed, so a database
  written by an older server keeps the behaviour it had. The repository
  round trip is pinned in `tests/integration/mysql_repository_test.cpp`.
  > **Status:** fixed (fix/war-schedule-lifecycle)

## A vampire's silver damage cleared by a GM is never saved (2026-09-23)

- **The exps save skips the `SilverDamage` column for a vampire when the
  value is zero, so it cannot reset the column,** and the one clear that
  does not go through `saveSilverDamage`, whose `tinysave` writes the
  column unconditionally, is the GM heal command's `setSilverDamage(0)`:
  a vampire a GM healed keeps the stale value across logout and reloads
  with it, while every in-game cure persists its result at once. The
  ousters row writes the column unconditionally. The repository header
  documents the skip and the integration tier pins it as it stands. The GM
  heal calls `saveSilverDamage(0)` now, for both races, so the clear is
  written the moment it is made, like every cure's.
  > **Status:** fixed (fix/recorded-defects-6)

## A reinforcement condition null-checked the wrong pointer (2026-09-23)

- **`ConditionExistReinforce` cast the castle's next scheduled war to
  `SiegeWar`, tested the uncast work pointer for null, and dereferenced
  the cast result,** so a next war that is not a siege, a guild war or a
  race war, crashed the condition. The four sibling actions test the cast
  result; the condition does now too.
  > **Status:** fixed (refactor/game-context-14, stack top)

## A retired guild member is read with its last rank (2026-09-23)

- **A `GuildMember` a thread may still hold is retired, not freed:**
  `Guild::deleteMember` and `retireAllMembers` move the object to
  `m_RetiredMembers` and erase it from the map, and `getMember()` returns
  a raw pointer after releasing the guild mutex, so a zone thread that
  took one before the removal keeps reading it. The object stays valid --
  that is what retirement buys -- but its rank, name and join date are
  frozen at the moment it left the guild, and nothing marks it as gone.
  A member kicked or a guild deleted while a zone thread is mid-tick can
  therefore still pass a rank check on that tick. The same holds for a
  retired `Guild` in `GuildManager::m_RetiredGuilds`.
  Both now carry an atomic retired flag, set where the retirement happens
  and under the lock that already covers it -- `Guild::deleteMember`,
  `Guild::retireAllMembers`, and `Guild::retire()` from
  `GuildManager::deleteGuild` and `retireAll_NOBLOCKED`, which marks the
  guild and every member still in its map. The answers a reader gets are
  what enforce it rather than a check each reader must remember:
  `GuildMember::getRank()` reads `GUILDMEMBER_RANK_LEAVE` once retired,
  `Guild::getState()` reads `GUILD_STATE_BROKEN`, and `Guild::getMember()`
  answers NULL for every name. The guild's own bookkeeping and its
  database rows take `getStoredRank()` / `m_State`, so the counters, the
  teardown list and the rows still carry what the member and guild had.
  `tests/guild_retirement_test.cpp` pins the answers.
  > **Status:** fixed (fix/manager-concurrency)

## A castle shrine's defender check asserts on a siege (2026-09-23)

- **`CastleShrineInfoManager::isDefenderOfGuardShrine` asks `getActiveWar`
  for the castle's war and then casts the result to `GuildWar` and asserts
  it,** and `getActiveWar` returns either castle war class, so the check is
  right for a guild war and throws for a siege, the defender side a siege
  knows being on `SiegeWar`. Latent: nothing calls the castle manager's
  method; the dissection handler reaches `ShrineInfoManager`'s, which
  consults no war. The castle manager's method and its declaration are
  gone, so the cast that could not answer for a siege is gone with them.
  > **Status:** fixed (fix/recorded-defects-6, deleted as uncalled)

## The string pool is rewritten under readers on reload (2026-09-23)

- **`StringPool::load()` clears and refills its map, the class has no
  mutex, and the GM reload event calls it on the main thread** while the
  zone threads and the two manager threads read `getString` and `c_str`
  from the same map. A `reloadinfo` of the string pool can race every
  reader. The gameserver's pool now publishes its map copy-on-write
  through `de::Snapshot`: `load()` builds a whole new map and swaps it in,
  and a reader loads an immutable one it may hold for as long as it likes.
  Because `c_str()` hands out a pointer into the map the call read, every
  map the pool has published is kept until the pool is destroyed rather
  than freed, the way a retired guild is -- a reload is a human-paced
  event and a pool is a few hundred kilobytes. `clear()` and `addString()`
  are gone with the piecewise filling they were for; the duplicate-row
  check moved into `load()`. The sharedserver has its own `StringPool`
  with the same shape, but its `load()` is called once, from
  `SharedServer::init()`, before a thread exists, so nothing there races.
  > **Status:** fixed (fix/manager-concurrency)

## The variable manager is written by a GM command with no lock (2026-09-23)

- **`VariableManager` has no mutex; the GM `opset` command sets a variable
  on a zone thread** while the incoming-connection log macro reads it on
  the main thread and the login-link handlers read it on their own
  thread. Every variable is an independent `int`, read and written one at
  a time, so the table is now a `vector<std::atomic<int>>` and every
  getter and `setVariable()` is an atomic load or store -- no lock on a
  path taken thousands of times a second. The table's own size is the one
  thing a writer cannot change under a reader, so `load()`, which
  replaces it, is private and reached only through `init()`, which the
  object manager calls during single-threaded startup; the name-to-type
  map it also fills is read-only from then on.
  > **Status:** fixed (fix/manager-concurrency)

## A guild war is scheduled but never becomes active (2026-09-22)

- **`WarSystem::addWar` and `WarSystem::heartbeat` make the same cast the
  lookups made: every `WAR_GUILD` war is `dynamic_cast` to `SiegeWar` and
  the result asserted,** and in `addWar` the assert sits after
  `addSchedule`, so a `GuildWar` is put on the recent schedules and the
  throw aborts the rest: it never reaches the active wars, never triggers
  the holy-land refresh, `hasCastleActiveWar`
  stays false for its castle, and at its end the heartbeat asserts once
  more on the same cast. The lookups no longer throw on it; the registration is the rest of
  the defect. `GuildWar` carries a castle zone id and its own owner-change
  and end-war overrides, so matching it by zone the way a siege is matched
  is the shape a fix would take.
  > **Status:** fixed (fix/recorded-defects-5) — `War` now answers for its
  > own castle, attacking guild, registration fee and participants
  > (`getCastleZoneID`, `getAttackerGuildID`, `getRegistrationFee`,
  > `isWarParticipant`; zero and no-one for a war fought over no castle), and
  > every `WAR_GUILD` site that cast to `SiegeWar` to read them asks the war
  > itself instead: `WarSystem::addWar`, `WarSystem::heartbeat`, the two
  > active-war lookups, `WarScheduler::hasSchedule` and
  > `WarSchedule::create`. A siege reads the same values through the virtuals
  > that it read through the cast, so the siege path is unchanged. A guild
  > war now writes its schedule row, reaches the active wars with its castle
  > and attacker, fires the holy-land refresh and the `GCWarList` broadcast,
  > answers `hasCastleActiveWar` and `getActiveWar` for its castle — so
  > `isModifyCastleOwner` and `endWar` dispatch to `GuildWar`'s own
  > overrides — and is erased from the active wars when its hour runs out.
  > `WarSchedule::save()` stayed siege-only here; it builds its row from the
  > war's own virtuals now (fix/war-residue).

## Two servers link different classes under one name (2026-09-22)

- **`GameServerInfoManager` exists as two different classes of the same
  name, one in ServerCore and one in the sharedserver, both reachable from
  the sharedserver link.** The archive member is never pulled in, so the
  program holds only the sharedserver's copy today; five member functions
  (the constructor, the destructor, `init`, `load` and `toString`) share a
  mangled name, so pulling it would be a duplicate-symbol link error rather
  than a silent mis-bind. The sharedserver's `g_pGameServerInfoManager`
  definition duplicated ServerCore's until its retirement onto
  `SharedServer`; the twin classes remain. The three
  `GameServerGroupInfoManager` classes live in three different executables
  and are no hazard.

  > **Status:** fixed (refactor/shared-twin-classes) — the sharedserver's
  > copies are renamed `SharedGameServerInfoManager` and
  > `SharedGameServerInfo`, file names included: the element class was a
  > twin too, ServerCore's `GameServerInfo` carrying two members and four
  > accessors the sharedserver's does not, so two definitions of one class
  > name no longer meet in that link. `GameServerGroupInfoManager` is not
  > the same case: ServerCore has no copy of it, the gameserver, the
  > loginserver and the sharedserver each compile their own into their own
  > binary, and three classes that never share a link are not an ODR
  > problem, so they stay as they are. No loginserver class shadows a
  > ServerCore one. The sharedserver's `GameServerManager::heartbeat()` is
  > gone as well: it locked the manager's mutex, did nothing and had no
  > caller, the worker loop calling the guild manager's heartbeat instead.

## A motorcycle that cannot be placed is paid for and never delivered (2026-09-22)

- **`CGShopRequestBuyHandler::executeMotorcycle` charges the player and
  advances the shop version before it asks the zone for a tile, and when
  no tile is free it prints a banner and returns:** the gold is gone, no
  motorcycle or key follows, the probe key it created to test the
  inventory is never freed, and no `GCShopBuyOK` or `GCShopBuyFail` is
  sent, so the client's shop dialog waits for a reply that never comes.
  Every other exit of the function frees the probe and answers. Whether
  to refund and roll the shop version back, or to place the item first,
  is a design decision.
  > **Status:** fixed (fix/recorded-defects-4) — the no-tile branch now
  > refunds the price with `increaseGoldEx`, the exact inverse of the
  > `decreaseGoldEx` that charged it, frees the probe key and answers
  > `GCShopBuyFail` with `GC_SHOP_BUY_FAIL_NOT_ENOUGH_SPACE`, the way the
  > function's earlier exits do. The shop version is deliberately left
  > raised: a raised version only makes the client re-fetch the rack,
  > which still holds the motorcycle.

## A scheduled guild war makes every active-war lookup assert (2026-09-22)

- **`WarSystem::getActiveWar` and `getActiveWarSchedule_LOCKED` walk the
  recent schedules and, for every war whose type is `WAR_GUILD`,
  `dynamic_cast` it to `SiegeWar` and assert the result,** but `GuildWar`
  is a sibling of `SiegeWar` that reports the same type and reaches the
  schedules through `ActionWarRegistration` and the zone heartbeat. While
  a guild war sits there, every lookup for any zone throws
  `AssertionError`, including the castle shrine's owner change and the
  siege manager's checks.
  > **Status:** fixed (fix/recorded-defects-4) — both loops now skip a war
  > whose cast to `SiegeWar` fails instead of asserting it, so a scheduled
  > guild war is passed over and the lookups keep answering for every
  > other zone. Every caller already handles the `NULL` a zone with no
  > siege returns. A `GuildWar` is matched by its own `getCastleZoneID`
  > since fix/recorded-defects-5, so both loops answer for it too.

## A shrine set with no owner names its race from an uninitialised pointer (2026-09-22)

- **`ShrineInfoManager::putBloodBible` chose the race name for its
  broadcast in three branches, Slayer, Vampire and Ousters, and left the
  pointer indeterminate for a shrine set nobody owns,** then handed it to
  `sprintf`. The pointer now starts as an empty string, so an unowned
  shrine set is announced with no race name rather than whatever the stack
  held.
  > **Status:** fixed (refactor/game-context-12, stack top)

## The monster name manager's event names are neither zeroed nor freed (2026-09-22)

- **`MonsterNameManager` initialised its first, middle and last name arrays
  in the constructor and freed them in the destructor, but not the event
  last-name array `init()` also loads:** an `init()` that threw before that
  block left the pointer indeterminate, and every shutdown leaked it. Both
  now match the other three.
  > **Status:** fixed (refactor/game-context-12, stack top)

## The war system's end condition is always met (2026-09-22)

- **`WarSystem::isEndCondition` asserts its item and corpse and returns
  true;** the comparison of the blood bible's monster type against the
  corpse's is a comment above the return. Nothing calls it today, so the
  function is a stub a future caller would trust; the comparison it meant
  names an accessor that does not exist in that spelling.
  > **Status:** fixed (fix/recorded-defects-5) — deleted, uncalled. The
  > function had no caller in any of the three servers and none in the
  > history: it arrives with the original import and is untouched since, so
  > nothing loses a check. A caller that needs the comparison writes it
  > against the blood bible and the corpse it holds.

## The event monster name overload returns before its retry loop can retry (2026-09-22)

- **The event `MonsterNameManager::getRandomName(Monster*, bool)` returns on
  the first pass of its retry loop and spells its fallback as a comparison
  (`Name == "..."`) rather than an assignment,** the loop shape the non-event
  overload had: an empty event-name row comes back as an empty name, the
  trial count is spent on nothing and the fallback after the loop is both
  unreachable and a no-op. Nothing calls this overload today; the one
  `getRandomName` call site takes the non-event one.
  The loop now draws again while the row that came up is empty and the
  fallback is an assignment the 300th empty draw reaches. The name itself
  is unchanged: a non-empty first draw returns that event part alone, as
  before, with nothing appended around it.
  > **Status:** fixed (fix/recorded-defects-3)

## Placing a blood bible on the holy shrine flips its owner without the war check (2026-09-22)

- **`ShrineInfoManager`'s holy-shrine branch tested `isMatchHolyShrine`
  and the war system's `isModifyCastleOwner` together; the second conjunct
  was commented out with the closing parenthesis inside the comment,** so
  the live condition is the match alone, or a guard-shrine defender, and
  its only surviving statement sets the shrine set's owner race. Any
  player who places a matching blood bible there changes the owner with no
  war-eligibility test; the `endWar` and `returnBloodBible` that followed
  are commented out as well. The castle counterpart in
  `CastleShrineInfoManager` still applies the check.
  The war the holy shrines are fought over is the race war, not a castle
  war: the comment on `canPickupBloodBible` (a stub that answers true) says
  the bible is used only in race wars,
  `RaceWar::executeStart` drops every guard-shrine shield and broadcasts the
  bible positions, and `RaceWar::executeEnd` returns them all and tallies the
  shrine owners into `RaceWarHistory`. So the test restored is the race
  war's own -- whoever the holy land is open to while it runs, which is
  everyone when the participant limiter is off and otherwise the players
  carrying the join ticket, the same pair `ConditionEnterHolyLand` and the
  waypoint handler ask -- and it is asked through
  `War::mayModifyShrineOwner`, which `RaceWar` answers and every other war
  refuses, over a `WarSystem::mayModifyShrineOwner` that reads
  `hasActiveRaceWar()` first and answers no when no race war is running
  rather than asserting. The castle's other two statements are deliberately
  not restored: a race war does not end when one shrine changes hands, and
  the bible travels back to its guard shrine either way, which is what the
  unconditional `returnBloodBible` below the branch already did. The
  guard-shrine arm of the same condition, a defender of the castle's race
  returning the bible to its own shrine, keeps the rule it always had and
  asks no war.
  > **Status:** fixed (fix/shrine-and-speed)

## A random mine item is read off a row that may not exist (2026-09-22)

- **`ItemMineInfoManager::getRandomItem` draws an id in the caller's range
  and dereferences `getItemMineInfo(id)` without checking it,** so a gap
  in the `ItemMineInfo` rows faults the caller: the five hunting-pouch
  branches of the use-item handler and the GQuest inventory handler, each
  on a zone thread. The lair trade guards the same lookup one frame up.
  The function now answers null for a missing row; the GQuest handler
  already refuses a null item through `fitToPC`, and the pouch handler
  refuses it too.
  > **Status:** fixed (refactor/game-context-11, stack top)

## A third event action crashes on an Ousters (2026-09-22)

- **`ActionGiveTestServerReward` chooses its Lua item selector in a Slayer
  branch and a Vampire branch and calls `prepare()` on it,** the defect
  `ActionGiveEventItem`, `ActionGiveAccountEventItem` and
  `ActionTradeGiftBox` had before they refused the race instead.
  It refuses the race the same way now: the action reads a `SlayerFilename`
  and a `VampireFilename` and nothing else, its Lua scripts are the slayer
  and vampire pair, so a null selector logs the character to
  `TestServerRewardError.txt`, closes the NPC dialogue and returns. The
  reward flag is left set, so a character that reaches the action again
  with a selector still gets its reward.
  > **Status:** fixed (fix/recorded-defects-3)

## A vision info manager's debug string fell off the end of the function (2026-09-22)

- **`VisionInfoManager::toString()` returned `string` and had no `return`:**
  its whole body was a commented-out block, so the live function was a
  try/catch frame around nothing. Falling off the end of a non-void
  function is undefined behaviour; nothing calls it today. It now returns
  the class name, the manager having no state to print.
  > **Status:** fixed (refactor/game-context-10, stack top)

## The lottery handler used the item it failed to roll (2026-09-22)

- **`CGLotterySelectHandler` walks the treasure list and, when no entry
  rolled an item, still called `setItemGender` on the item pointer and, in
  its else branch, `setUnique`, `setTimeLimitItem` and `setQuestItem` on
  it.** The pointer was uninitialised before the leak fix and null after
  it, so the failure became a deterministic null call. The gender and the
  fallback branch now run only for an item that exists; a player whose
  roll produced nothing gets nothing, as the inventory branch already
  allowed.
  > **Status:** fixed (refactor/game-context-10, stack top)

## A lair-item trade dereferences its mine info and item before checking them (2026-09-22)

- **`ActionTradeLairItem` dereferences `pItemMineInfo->getItem()` unchecked
  and calls `setItemGender` on its item before the null check that
  follows it,** so a treasure whose roll produced nothing reaches
  `setItemGender` with null, the shape the lottery handler had.
  The mine info is now checked before its item is taken and the gender is
  set only for an item that exists, so a band with no mine row and a race
  branch that produced nothing leave the trade item-less and reach the
  no-item handling the other trade types already take, rather than
  dereferencing null. The mine-backed types name no lair master, so an
  item-less one of those ends in the trade's monster lookup rather than
  its `tradeLairItemBUG.txt` refusal; both are data faults and neither
  faults the server now.
  > **Status:** fixed (fix/recorded-defects-2)

## Two more event item actions crash on an Ousters (2026-09-22)

- **`ActionGiveAccountEventItem` and `ActionTradeGiftBox` choose their Lua
  item selector in a Slayer branch and a Vampire branch, so an Ousters
  leaves the pointer null and dereferences it,** the defect
  `ActionGiveEventItem` had before it refused the race instead.
  Neither action has an Ousters selector to reach for: both read only a
  `SlayerFilename` and a `VampireFilename`, and the slayer and vampire
  classes remain the only selectors in the tree. So both now refuse the
  way `ActionGiveEventItem` does -- the character is logged to the
  action's own error file (`AccountEventItemError.txt`,
  `XMasEventError.txt`) and the NPC dialogue closes. The same two-branch
  selector choice is also in `ActionGiveTestServerReward`, untouched.
  > **Status:** fixed (fix/recorded-defects-2)

## The movement and attack speed-hack check is gone (2026-09-22)

- **`GamePlayer::verifySpeed` keeps only the `CG_VERIFY_TIME` heartbeat
  check; of its two callers, `processCommand` discarded the result and the
  heartbeat handler acted on it.** Its per-race move and
  attack timing check, 298 lines, sat inside a comment block since before the tree was imported and went with
  the commented-out code; what remains for `CG_MOVE` is an empty `if` on
  `m_MoveSpeedVerify` and a round-trip time computed into a local nothing
  reads, and `m_AttackSpeedVerify` is written by nothing that decides on
  it.
  The block was abandoned before the tree was imported, not paused: at the
  import commit `verifySpeed` was reached from one place, the `CGVerifyTime`
  handler, so its `CG_MOVE` and `CG_ATTACK` branches could not run whatever
  the comment markers said (the `processCommand` call that fed it every
  packet came later, with the recovered sources, and went with the heartbeat
  entry of 2026-09-24); it opens by redeclaring the `SpeedCheck` the live
  body above it already declares, so it does not compile where it sits; its
  race chain ends in an empty `else`, leaving Ousters unchecked; and its
  vampire arm assigns the same constant in all three speed tiers although
  its comments give the fast tier a different number. Reinstating it would mean inventing
  per-race timings and an enforcement action the original never had, so the
  residue was removed instead: the empty `if`, the unread round-trip local,
  `m_MoveSpeedVerify`, `m_AttackSpeedVerify`, the untouched
  `m_SkillSpeedVerify[SKILL_MAX]` array and the `tv_sub` helper that only the
  unread local used. What survives is the heartbeat rule, now a pure
  `de::verifyHeartbeat` (`src/server/gameserver/SpeedHackDecision.cpp`) with
  the session's state beside it and `tests/speed_hack_decision_test.cpp`
  holding its arithmetic; the duplicate call the same residue left behind is
  the 2026-09-24 entry at the top of this file.
  > **Status:** not a defect (the check never ran and could not compile; its
  > residue was removed in fix/shrine-and-speed)

## The log client never opens its socket (2026-09-22)

- **Every `LogClient` member and every global `log(...)` had its whole body
  commented out,** so `openLogClient()` allocates a client that never
  connects, `LogClient::m_LogLevel` is read by nothing, every `log(...)`
  call in the tree is a no-op, and each `main()` still reads
  `LogServerIP`, `LogServerPort` and `LogLevel` from its config for it.
  The module is gone: its 66 call sites, the message locals that only fed
  them, the class, its header, the `LogData` record it framed its messages
  in, the two ServerCore entries that compiled them, the three
  configuration keys no other reader wanted, and the gameserver's startup
  banner line for it. `LogData::read` took a `short` length straight into a
  2,001-byte stack buffer, so anything reinstated must not start from it;
  of the thirty-seven event kinds the header declared, nineteen were ever
  called. What the servers still write
  is the file logs (`filelog` and the `FILELOG_*` macros) and the play
  records the repositories keep; reinstating a log-server link would be a
  new feature, not a repair.
  > **Status:** fixed (refactor/dead-logclient)

## Duplicate Self is not gated on a master lair (2026-09-22)

- **`checkTimingDuplicateSelf` in `MonsterAI.cpp` lacks the master-lair
  guard its siblings `checkMasterSummonTiming` and `checkMasterNotReady`
  both apply;** the guard was there, switched off in a comment block that
  is now deleted. A master outside its lair can duplicate itself.
  The guard is back, spelled as its two siblings spell it: a master whose
  zone is not a master lair fails the condition, so the duplicate-self
  directive is offered only inside a lair.
  > **Status:** fixed (fix/recorded-defects-2)

## A random monster name is only ever its middle part (2026-09-22)

- **The non-event `MonsterNameManager::getRandomName` draws a first and a
  last name index it never uses, returns on the first pass of its retry
  loop, and spells its fallback as a comparison (`Name == "..."`) rather
  than an assignment,** so the fallback is unreachable and a named monster
  gets its middle name followed by a trailing space, which reaches the
  client. The design the deleted comment described was banded by level:
  a last name alone up to level 33, first and last to 66, all three above.
  The event overload has the same loop shape but draws a sensible event
  name.
  The name is now the middle part alone with no trailing space, the loop
  draws again while the row that came up is empty, and the fallback is an
  assignment the 300th empty draw reaches. The banded design is **not**
  restored and stays a design decision: the parts the database ships are up
  to 13, 11 and 18 bytes, so its top band composes up to 44 bytes while
  `GCAddMonster`, `GCAddMonsterFromBurrowing` and
  `GCAddMonsterFromTransformation` all throw `InvalidProtocolException`
  above a 32-byte name, and how to keep a three-part name inside that
  field is not something the deleted comment says. The first and last
  name tables stay loaded and unread, as they already were behind the
  indices that were always -1.
  > **Status:** fixed (fix/recorded-defects-2)

## The item factory's out-of-range guard is caught by its own handler (2026-09-22)

- **`ItemFactoryManager::createItem` checks the item class and throws
  `NoSuchElementException` inside a `try` whose `catch (Throwable&)` only
  prints it,** so control falls through to `m_Factories[IClass]->createItem`
  with the very index or null factory the guard rejected: an out-of-bounds
  read of the factory array for a class past `ITEM_CLASS_MAX`, a null call
  for an in-range class no factory registered. Not reachable from a client
  packet: the callers pass a literal class, a class read off an existing
  item, or one from server-side data, and the GM item command is gated by
  `isPossibleItem`, which asserts the range first. A corrupt database or
  XML row (`ItemMineInfo` reads its class from the database) crashes where
  the guard meant to log and skip. `getItemName` has the weaker shape of the
  same defect: it builds its diagnostic and discards it, then indexes.
  Both guards now leave their function: the swallowing `catch` is gone, so
  `createItem`'s `NoSuchElementException` propagates through `__END_CATCH`
  to the caller, and `getItemName`, which nothing in the tree calls, throws
  the diagnostic it had been building. Propagating rather than answering
  NULL is what the callers support: of the 183 `createItem` call sites
  only 38 check the pointer
  within five lines, so a NULL return would have moved the crash rather
  than removed it. The logging the guard already did is unchanged. Three
  in-range classes never get a factory (`ITEM_CLASS_CORPSE`,
  `ITEM_CLASS_GQUEST_ITEM`, `ITEM_CLASS_BLOOD_BIBLE_SIGN`), so server data
  naming one of them takes the null-factory branch. On a client packet the
  exception ends in a clean disconnect; on the worker threads that catch
  nothing per tick (`GDRLairManager::run`, the monster drop paths of
  `ZoneGroup::heartbeat`) it reaches `ManagedThread` and stops the server,
  where the old fall-through faulted on the same tick.
  > **Status:** fixed (fix/recorded-defects-1)

## A lair-item trade leaks every winning treasure but the last (2026-09-22)

- **`ActionTradeLairItem::execute` walks the monster type's treasure list
  and calls `createItem` for every entry whose `getRandomItem` succeeds,
  assigning each to the same `pItem1`,** so only the last survivor reaches
  `registerObject` and the earlier ones are heap objects nothing frees. One
  entry means no leak; several mean a leak on every trade, a repeatable NPC
  action. `CGLotterySelectHandler` has the identical loop shape.
  Both loops now `SAFE_DELETE` the pointer before overwriting it, so the
  roll the player is given is still the last winning one and the earlier
  ones are freed as they are replaced. The lottery handler's pointer also
  starts at NULL, which it needs to be deleted safely; it was left
  indeterminate. Still open there, and untouched: when no treasure in the
  list rolls an item, the body goes on to sex, store and log an item it
  does not have.
  > **Status:** fixed (fix/recorded-defects-1)

## Yellow Poison restores a fixed sight instead of the one it replaced (2026-09-22)

- **`EffectYellowPoisonToCreature::unaffect` sets the sight to the literal
  13 and only then removes the effect flag;** `m_OldSight`, which the affect
  and the loader both write, is read nowhere. `EffectLightness::unaffect`
  does it the right way round: remove the flag, then
  `setSight(getEffectedSight())`; `EffectFlare::unaffect` removes its flag
  first too, then restores the sight it saved, or a monster's per-type
  sight. The default and Lightness sights are
  13 too, so it diverges only under another sight effect: a creature blinded
  by Flare and then poisoned gets full vision back the moment the poison
  ticks out, and monsters, whose Flare restore reads the per-type sight,
  are restored to a value that need not be theirs.
  The flag now comes off first and the sight that follows is
  `getEffectedSight()`, so what is left is what the creature's remaining
  effects say: the Flare sight for one still flared, the default otherwise.
  Only a player creature can carry the effect, so the per-type restore
  Flare does for a monster is not needed here.
  `m_OldSight` stays as it is -- it is a persisted column
  (`EffectYellowPoisonToCreature.OldSight`, written by `create` and
  `save`), so the field and its writes are not dead, only its readers.
  > **Status:** fixed (fix/recorded-defects-1)

## The event item action dereferences a null selector for an Ousters (2026-09-22)

- **`ActionGiveEventItem::execute` picks the Lua item selector in an
  `isSlayer()` branch and an `isVampire()` branch, neither of which an
  Ousters enters,** so `pLuaSelectItem` stays null and is dereferenced a
  few lines down.
  There is no Ousters selector to reach for: the action reads a
  `SlayerFilename` and a `VampireFilename` and nothing else, and
  `LuaTradeEventSlayerItem` and `LuaTradeEventVampireItem` are the only
  two selector classes in the tree. So a null selector is now a refusal:
  the action logs the character to `GiveEventItemError.txt`, closes the
  NPC dialogue the way its other refusals do, and returns. The same
  two-branch selector choice is in `ActionGiveAccountEventItem` and
  `ActionTradeGiftBox`, which now refuse it too; `ActionGiveTestServerReward`
  still carries it.
  > **Status:** fixed (fix/recorded-defects-1)

## A skill off cooldown is sent a 4-billion-turn casting time (2026-09-22)

- **`getRemainTurn` returns `Turn_t`, which is `DWORD`.** The body subtracts
  the current time from the slot's run time, so a slot whose cooldown has
  already elapsed — the normal state of every skill a character is not
  mid-cast on — computes a negative remainder and converts it to a value
  near 2^32. All three races do the same thing with it:
  `Slayer::sendSlayerSkillInfo`, `Vampire::sendVampireSkillInfo` and
  `Ousters::sendOustersSkillInfo` put it in the sub-info's *casting time*
  field, and `SubSlayerSkillInfo`/`SubVampireSkillInfo`/`SubOustersSkillInfo`
  write it to the client as four bytes. The load path calls `setRunTime()`,
  which places the run time the stored delay ahead, and each race's
  `addSkill` stores a delay of 0 for a newly learned skill, so a skill
  learned and not yet cast is already past its run time by the time
  `ZoneSpawn.cpp` sends the skill list on the first zone entry after login;
  every later send repeats it — each zone entry, the four `CGSkillTo*`
  handlers after a vampire cast, `EventMorph.cpp`, `PCManager.cpp` and
  `skill/Restore.cpp`. The same body sits in `skill/SkillSlot.cpp` and
  `skill/RaceSkillSlot.cpp` alike, so this is one defect behind three
  callers, not three defects.
  Both bodies now clamp: a run time that is already past reports no turns
  left. The wire layout is unchanged and the client already handles the
  value — `GCSkillInfoHandler` passes the casting time to
  `SKILLINFO_NODE::SetAvailableTime`, whose zero means "usable now", the
  same thing the wrapped value reached through its negative `int`. The
  clamped rule is pinned by `race_skill_slot_tests` and by the new
  `skill_slot_tests`, which covers the slayer slot's copy of the body.
  > **Status:** fixed (fix/recorded-defects-1)

## ObjectManager created a volume info manager it never deleted (2026-09-17)

- **`g_pVolumeInfoManager` was `new`ed in `ObjectManager`'s constructor and
  named nowhere in its destructor.** Every other manager the constructor
  creates is freed there, so the manager leaked for the life of the process;
  it is now an `ObjectManager` member, deleted in the position that mirrors
  its creation, between the item factory and the item loader manager. (Its
  own destructor still frees none of the `VolumeInfo` entries `init()`
  allocates, which is a second leak, left as it is.) The same sweep dropped
  the duplicate `SAFE_DELETE` of `g_pOptionInfoManager` and of
  `g_pSkillDomainInfoManager`, each written twice in that destructor: the
  macro nulls what it frees, so the second call did nothing, but it read as
  a delete of a live pointer.
  > **Status:** fixed (refactor/game-context-4)

## A flag manager could wake up believing a flag war was already on (2026-09-17)

- **`FlagManager::m_bHasFlagWar` was never initialised.** The constructor
  set up the two wars and their schedules but left the flag holding whatever
  the allocation held. Read as true, `startFlagWar()` refused the first
  scheduled or manual start, and `endFlagWar()` then recorded a flag-war
  history row for a war that never happened, complete with the external
  history script run on an uninitialised end time and empty counts, before
  clearing the flag. The member now starts false, so a fresh manager starts
  its first war and records nothing until one has run.
  > **Status:** fixed (refactor/game-context-1)

## A GM command crashed the game server on a dynamic zone type it did not know (2026-09-16)

- **`opAddDynamicZone` dereferenced both halves of
  `getDynamicZoneGroup(type)->getAvailableDynamicZone()` unchecked.**
  `DynamicZoneManager::getDynamicZoneGroup` answers NULL for a type it holds
  no group for -- anything outside the set the dynamic zone info loads -- so
  `*command addDynamicZone` with a number that is not one of them called a
  member function through NULL and took the process down. The body now keeps
  the group pointer, answers the GM that there is no group of that type, and
  answers again should a group hand back no instance; the row already carries
  a `gcSystemMessage`, so both refusals reach the console. A type that has a
  group behaves as before.
  > **Status:** fixed (fix/dead-zone-bodies)

## A GM setting gold read the name of a creature it had just found NULL (2026-09-16)

- **`opSetGold` wrote the money-trace log outside the `pCreature != NULL`
  guard the rest of its body sits in.** The command reads the GM's own
  creature, does nothing with it when there is none -- a line typed before
  the character is in a zone, or by a player whose character failed to load
  -- and then, for an amount at or above the money-trace limit, logged
  `pCreature->getName()` regardless. The log now sits inside the guard, so a
  line with no creature behind it changes nothing and logs nothing instead of
  dereferencing NULL. With a creature, the same trace is written as before.
  > **Status:** fixed (fix/dead-zone-bodies)

## WarSystem::broadcastWarList sends through an unchecked player pointer (2026-09-16)

- **Read in the same sweep and left as it is.** The three
  `pGamePlayer->sendPacket(...)` calls in `WarSystem::broadcastWarList` never
  ask whether there is a player, but its one caller, `opShowWarList`, is
  registered `Relay::PlayerOnly`, and `SubcommandTable::dispatch` passes over
  such a row when the line has no player behind it -- which is the only way a
  `*command` body is reached without one. No other code calls the method. A
  guard was therefore not added; the pin that keeps this closed is the
  `showWarList` row in `tests/gm_console_command_test.cpp`, and a future
  caller that can pass NULL has to guard or the method has to grow one.
  > **Status:** not a defect (fix/dead-zone-bodies)

## Slayer's inventory packets lost the failing frame from the stack trace (2026-09-16)

- **`Slayer::getExtraInfo()` and `Slayer::getInventoryInfo()` opened a
  `__BEGIN_DEBUG` block but not the `__BEGIN_TRY`/`__END_CATCH` pair the
  Vampire and Ousters copies of the same code carried.** Both walk the
  character's items building one info object per item, so a `Throwable`
  raised inside them left the function straight for the caller's handler:
  the exception was the same and reached the same place, but `__END_CATCH`
  had not run, so the recorded stack named the caller and never the builder
  that actually failed. The three copies are now one body on
  `PlayerCreature`, the Vampire and Ousters one, so the Slayer path adds its
  frame and rethrows like its siblings. Control flow is unchanged.
  > **Status:** fixed (refactor/race-shared-2)

## A vampire or ousters character carried indeterminate gold until its first load (2026-09-16)

- **Only `Slayer::Slayer()` set `m_Gold = 0`; the Vampire and Ousters
  constructors left the member uninitialised.** Every path that reads gold
  before `load()` fills it in, such as `getGold()` in an info packet built
  for a character that failed to load, read whatever the allocation held.
  The member now lives on `PlayerCreature`, defined once for the three races,
  and carries a member initialiser of zero, so a fresh character of any race
  starts with the gold a Slayer always started with.
  > **Status:** fixed (refactor/race-shared-1)

## A relayed `*command` could crash the receiving game server (2026-09-16)

- **`GGCommandHandler` ran `*command` sub-command bodies with no player,
  and 40 of the 61 read one the moment they start.** `*world` relays
  whatever a GOD typed after it to every other game server, so
  `*world *command heal` reached each receiving server's `opHeal`, which
  opens with `pGamePlayer->getCreature()` on the NULL the relay hands it.
  Every sub-command row now declares whether its body is correct with no
  player behind the line, and `SubcommandTable::dispatch` passes over a
  `PlayerOnly` row when there is none, the way it passes over a row whose
  gate the caller is below. The behaviour change is that a `PlayerOnly`
  sub-command arriving through the relay now does nothing instead of
  dereferencing NULL; no body changed. `GGCommandHandler` hands the
  relayed message to a table of its own
  (`src/server/gameserver/gm/RelayCommandRegistration.cpp`) listing the
  twelve commands a relay may run, all of which are correct with no
  player; `tests/gm_console_command_test.cpp` and
  `tests/gm_command_router_test.cpp` pin both.
  > **Status:** fixed (refactor/gg-relay-table)

## Guild union changes never reached the other game servers (2026-09-16)

- **`GGCommandHandler` compared a 17-character slice of the relayed
  message against the 15-character name `modifyunioninfo`.**
  `GuildUnionManager::sendModifyUnionInfo` sends `*modifyunioninfo <guild
  id>` to every other game server so that each reloads the union the guild
  belongs to. The receiving slice always carried the space and the first
  digit of the id as well, so it never matched, the relay was dropped
  without a log line, and the other servers kept the old union membership
  until the next `*refreshguildunion` (whose name is 17 characters and did
  match). The slice is now the name's length and `opmodifyunioninfo` runs
  on the relay as intended.
  > **Status:** fixed (refactor/zone-split-1)

## CGWhisper wrote names its own read() refuses (2026-09-16)

- **`CGWhisper::write()` bounded the name at 128 while `read()` bounds
  it at 10.** The packet is client-to-game, so `read()` is the contract
  and `CGWhisperFactory::kMaxSize` budgets the name at 10 as well: a name
  of 11 to 128 bytes was written happily and then refused by every reader
  of the field, the server's own included. `write()` now refuses the same
  lengths `read()` does, which is a behaviour change on the write side --
  the call that used to emit an unreadable packet now throws
  `InvalidProtocolException`. No valid packet's bytes move:
  `tests/wire-layout.txt` and `tests/golden/CGWhisper.code0.hex` are
  unchanged, and the server never writes this packet. The client repo's
  copy (`Client/Packet/Cpackets/CGWhisper.cpp`) carries the same 128/10
  split, recorded there in a comment; it needs the identical change.
  `tests/packet_roundtrip_test.cpp` pins both ends of the bound.
  > **Status:** fixed (fix/exception-text)

## Three GM commands answered any player (2026-09-16)

- **`*find`, `*OpenPayMap` and `*ClosePayMap` ran for anyone who typed
  them.** Each branch of the say handler's command ladder tested two
  names with `nameA || nameB && isGOD()`, and `&&` binds tighter than
  `||`, so the GOD gate guarded only the second, mojibake, name: any
  player could look up which server another account was on, and could
  open or close the pay zone's portals. The command table registers all
  three as GOD-only, and `tests/gm_command_router_test.cpp` pins the
  gate.
  > **Status:** fixed (refactor/gm-command-router)

## Session and take-out write/read disagreements (2026-09-16)

The twelve findings task 1.2 stated as flip-tests in
`tests/packet_session_test.cpp`, the initialisation split the same file
pinned, and the four its header recorded rather than tested. These
packets are client-facing, so `write()` and `getPacketSize()` are the
contract: every fix is a refusal, a cap, a size-accounting correction, a
read-side correction or an initialisation. **No valid packet's bytes
move.** One `tests/wire-layout.txt` line moves — `GCReconnect` 42 → 41 —
a read-buffer budget, not a field on the wire. One golden is re-recorded,
`GCExecuteElement.code0.hex` (`…aa…` → `…03…`), because its fixture
carried a condition byte outside the four the packet now bounds; that is
a fixture change, not a protocol change.

- **`GCReconnect::getPacketSize()` counted a pc-type byte `write()`
  never emitted.** The declared body was one byte longer than the body
  sent, and `writePacket()` puts the declared length on the wire before
  calling `write()`, so the client's next packet header started one byte
  early. The client's own copy
  (`Client/Packet/Gpackets/GCReconnect.cpp`) reads a name, a server
  address and a key and no pc type, so `write()`/`read()` were right and
  the size was wrong: the `szPCType` term is gone from `getPacketSize()`
  and from the factory max, with `m_PCType` and its accessors, which
  nothing wrote, read or initialised — `getPCType()` returned whatever
  the storage held. No `src/server` source used them. The golden is
  unchanged.
  > **Status:** fixed (wire/session-disagreements)

- **`CGRequestIP` and `GCRequestedIP` carried a name past what their
  factory maxima budget.** Neither setter, `write()` nor `read()` bounded
  it, while both maxima budget ten bytes, so an eleven-byte name declared
  and emitted a body past the buffer the receiver sizes from that max —
  and `CGRequestIP::read()` took the full 255 its count byte carries.
  Both setters cut at ten, and both fields go through
  `de::wire::readString`/`writeString` at that width: `{0, 10}` for the
  request, which admits a nameless one, `{1, 10}` for the answer, which
  keeps refusing it on both sides. A ten-byte name's bytes are unchanged.
  > **Status:** fixed (wire/session-disagreements)

- **`CGRequestIP::read` left the name the packet already held
  untouched** when the count byte was zero, so a packet read into twice
  kept the first name while `write()` had emitted none. The helper clears
  the field on a zero length.
  > **Status:** fixed (wire/session-disagreements)

- **`GCGoodsList` narrowed both of its counts to a BYTE before testing
  them.** The record count was narrowed and then compared against
  `MAX_GOODS_LIST`, so 256 records wrapped the count to zero while
  `write()` still emitted every one of them — a listing the client reads
  as empty followed by bytes it takes for the next packet's header. The
  per-record option count was narrowed with no test at all, while
  `GoodsInfo::getPacketMaxSize` budgets 255 options. `addGoodsInfo`
  refuses a record past `MAX_GOODS_LIST` and one whose option list runs
  past 255, and `write()` tests both sizes before narrowing them.
  > **Status:** fixed (wire/session-disagreements)

- **`GCGoodsList::read` appended to the listing the packet already
  held** instead of replacing it, so a packet read into twice declared
  and wrote both listings, and the records of the first leaked. Ownership
  is settled from the sender: `quest/ActionTakeOutGoods.cpp` allocates
  each record fresh and keeps none, so the records belong to the packet —
  `read()` frees what it holds before filling the listing again, the
  destructor frees the rest, and `popGoodsInfo()` hands one to the caller
  to free.
  > **Status:** fixed (wire/session-disagreements)

- **`GCGoodsList` followed pointers it never checked.** `addGoodsInfo`
  accepted `NULL`, which `getPacketSize()` then dereferenced while
  `write()` bounded it through an `Assert()`; and `popGoodsInfo` took
  `front()` off a list it never tested for emptiness, which is undefined
  behaviour on an empty listing and the packet exposes no count to bound
  a drain with. The adder refuses a null record and the pop refuses an
  empty listing, both through `InvalidProtocolException`.
  > **Status:** fixed (wire/session-disagreements)

- **`CGRequestInfo::read`, `CGLotterySelect::read` and
  `CGTypeStringList::read` stored a code byte they compared against
  nothing**, each with a named range in its own header —
  `REQUEST_INFO_MAX`, `TYPE_MAX`, and the three `StringType` values. All
  three test the raw byte before storing it, the shape
  `GCChangeWeather::read` already uses.
  > **Status:** fixed (wire/session-disagreements)

- **`CGCrashReport::write` emitted a body `read()` refused.** `write()`
  admitted an empty OS, call stack and message and emitted each as a bare
  zero-length word, while `read()` refused a zero length on all three, so
  a crash report with any of the three empty disconnected the client that
  sent it. The client's own writer
  (`Client/Packet/Cpackets/CGCrashReport.cpp`) emits the same shape and
  its reader bounds only the upper end, so `read()` admits an empty field
  on all three. Its two fixed-width fields were bounded through
  `Assert()`, which appends to `assertion_failed.log` in the working
  directory before it throws; they are `InvalidProtocolException` now.
  > **Status:** fixed (wire/session-disagreements)

- **Twelve of the eighteen packets left a member the default constructor
  never set**, so a packet sent without every setter called put
  indeterminate bytes on the wire. Every member of all eighteen has an
  initialiser, and the poisoned-storage pin in
  `tests/packet_session_test.cpp` covers the whole set in one list.
  > **Status:** fixed (wire/session-disagreements)

- **`GCUpdateInfo::toString` indexed `Weather2String[m_Weather]`
  unguarded.** The table holds three entries and the enum names a
  `WEATHER_MAX` past them, so a stored value the table does not name read
  past its end. It prints as its number, the way `GCChangeWeather`'s own
  `toString()` does.
  > **Status:** fixed (wire/session-disagreements)

- **`GCExecuteElement` bounded its condition byte on read alone.** The
  four conditions a quest element fires under are `GQuestInfo`'s
  `HAPPEN`, `COMPLETE`, `FAIL` and `REWARD`; the setter and `write()` took
  any byte. Both refuse one past the four now. No reachable send moves:
  the packet's one sender passes the `ElementType` `makeVector` stamped on
  the element.
  > **Status:** fixed (wire/session-disagreements)

- **`CGPortCheck` is registered on the game server's client-facing
  dispatch table although `DatagramPacket::read(SocketInputStream&)`
  refuses a TCP stream by design.** A client that sends the id over TCP
  is already refused there. The registration is not the defect and cannot
  be dropped: `PacketFactoryManager::init()` builds one factory table per
  server process, and the game server's UDP path resolves that same table
  — `Datagram::read(DatagramPacket*&)` (in `src/Core/DatagramFactoryRead.cpp`
  since the 2.4 kernel split) asks `g_pPacketFactoryManager` for
  the packet and `Datagram::isDatagram` names `PACKET_CG_PORT_CHECK` — so
  the entry is what lets the packet arrive at all.
  > **Status:** recorded, left as it is (wire/session-disagreements)

## Creature-state write/read disagreements (2026-09-13)

The five findings task 1.2 stated as flip-tests in
`tests/packet_creature_test.cpp`, the initialisation split the same file
pinned, and the seven its header recorded rather than tested. These
packets are client-facing, so `write()` and `getPacketSize()` are the
contract: every fix is a refusal, a widening to what the field already
carries, a size-accounting correction, a read-side correction or an
initialisation. **No golden changes.** One `tests/wire-layout.txt` line
moves — `GCAddInjuriousCreature` 11 → 21 — a read-buffer budget, not a
field on the wire.

- **`PCSlayerInfo3`, `PCVampireInfo3` and `PCOustersInfo3` had
  hand-written copy constructors that left the union id out**, while
  their equally hand-written assignment operators copied it. The union
  id never reached a client: `Slayer::getSlayerInfo3()`,
  `Vampire::getVampireInfo3()` and `Ousters::getOustersInfo3()` each
  refresh the character's cached record — coordinates, HP, alignment,
  guild id, and the union id they look up through
  `GuildUnionManager::getGuildUnion` — and then `return m_SlayerInfo;`.
  Returning a *member* by value is a copy the compiler cannot elide, so
  the copy constructor ran on every call and dropped the id it had just
  written. Ten packets hold one of those records by value and are built
  from those getters: `GCAddSlayer`, `GCAddVampire` and `GCAddOusters`
  (every zone scan, every character that walks into view, via
  `PacketUtil.cpp`'s `makeGCAdd*`), the three corpse packets,
  `GCAddVampireFromBurrowing`, `GCAddVampireFromTransformation` and the
  two morph packets. What the client got in those four bytes was
  whatever the return slot held: zero for a vampire, whose record
  declared `uint m_UnionID = 0`, and indeterminate bytes for a slayer or
  an ousters, whose records declared no member initialisers at all. All
  three copy constructors and assignment operators are `= default` — the
  records are plain data — and every member of all three has an
  initialiser, so a copy of one carries no indeterminate byte either.
  > **Status:** fixed (wire/creature-disagreements)

- **`GCMorph1::getPacketSize()` did not count the pc-type byte
  `write()` puts in front of the record**, so the declared body was one
  byte shorter than the body sent. `writePacket()` puts the declared
  length on the wire before calling `write()`, so the extra byte was not
  a wrong length but a stream the client could not resynchronise: the
  next packet's header started one byte late. The factory max already
  budgeted the byte. It counts `szBYTE` now.
  > **Status:** fixed (wire/creature-disagreements)

- **`GCMorph1` followed null record pointers.** Its constructor sets all
  four to `NULL`, `getPacketSize()` dereferenced all four and `write()`
  guarded only the PC one, so a sender that set the PC record and forgot
  the inventory crashed the game server instead of being refused. Both
  refuse a missing record through `InvalidProtocolException`, and
  `write()`'s pc-type bound is that exception in place of the `Assert()`
  that appended to `assertion_failed.log` in the working directory before
  throwing. Ownership is settled from the senders: all three
  (`EventMorph.cpp` and `skill/Restore.cpp` twice) pass records the
  `Slayer`/`Vampire` accessors allocate fresh and nobody keeps, so the
  four belong to the packet — each setter frees what it replaces,
  `read()` frees the four it holds before filling four more, and the
  destructor frees what is left. A second `read()` leaked all four
  before.
  > **Status:** fixed (wire/creature-disagreements)

- **`GCAddInjuriousCreature` bounded at ten the character name it
  carries.** `write()` and `read()` both stopped at ten while the name
  comes from a creature and runs to `maxNameLength` (20), so a player
  with an eleven-byte name could not be announced as the one who struck
  first: `write()` raised instead of truncating, and the target never
  learned who to strike back at. The setter cuts at `maxNameLength`,
  `write()` and `read()` carry the field at that width, and the factory
  max budgets a whole name (`szBYTE + maxNameLength`, 11 → 21). A
  ten-byte name's bytes are unchanged.
  > **Status:** fixed (wire/creature-disagreements)

- **`GCChangeWeather::read` cast the wire byte straight to `Weather`.**
  The enum declares three weathers and a `WEATHER_MAX`, so a byte past
  its range was an out-of-range enum load, which the Debug toolchain's
  UBSan traps on; and `Weather2String`, which `toString()` indexes with
  the stored value, holds three entries, so a stored `WEATHER_MAX` read
  past its end. `read()` tests the raw byte against `WEATHER_MAX` before
  assigning, and `toString()` prints a value the table does not name as
  its number.
  > **Status:** fixed (wire/creature-disagreements)

- **`GCKnocksTargetBackOK1::read` and `GCKnocksTargetBackOK5::read`
  loaded the success flag straight into a `bool` member**, so a wire byte
  other than 0 or 1 became an invalid bool — a value on which every later
  test of it is undefined. Both read a BYTE, refuse anything but 0 or 1
  and assign, the shape `SlayerSkillInfo` and `OustersSkillInfo` already
  use.
  > **Status:** fixed (wire/creature-disagreements)

- **`GCExecuteElement::read` took a condition byte it compared against
  nothing**, where the four conditions a quest element fires under are
  `GQuestInfo::ElementType`'s `HAPPEN`, `COMPLETE`, `FAIL` and `REWARD`.
  `read()` refuses a byte past them. The sending side is left as it is:
  the packet's one sender, `GQuestExecuteElement::checkCondition`, passes
  the `ElementType` `GQuestInfo::makeVector` stamped on the element, so
  no reachable send is out of range and no emitted byte moves.
  > **Status:** fixed (wire/creature-disagreements)

- **`GCMorphVampire2`'s accessor for the vampire record it holds was
  named `getSlayerInfo`.** It is `getVampireInfo`, matching
  `setVampireInfo` beside it and `GCAddVampire`. No `src/server` source
  called it.
  > **Status:** fixed (wire/creature-disagreements)

- **`CGBloodDrain::read` and `::write` copied the object id as raw
  bytes** (`iStream.read((char*)&m_ObjectID, szObjectID)`) rather than
  through the typed stream calls every other packet in the set uses,
  which is what `de::WireScalar` exists to constrain. `szObjectID` is
  `sizeof(ObjectID_t)`, so the bytes are identical.
  > **Status:** fixed (wire/creature-disagreements)

- **Thirty-one of the forty-two left at least one member the default
  constructor never set**, so a packet sent without every setter called
  put indeterminate bytes on the wire — a client reading a coordinate, a
  skill type or a direction that was never assigned. All forty-two
  initialise every member their `write()` emits.
  > **Status:** fixed (wire/creature-disagreements)

## Error paths that could not report anything (2026-09-13)

- **`XMLUtil::filelog`'s own overflow branch passed a null format to
  itself.** `filelog(NULL, "filelog buffer overflow!")` reaches
  `vsnprintf(buf, n, NULL, valist)` on the one path that exists to report
  that formatting already failed, which is undefined behaviour; the message
  is the format now. Both `filelog` overloads also threw their overflow
  `Error` before `va_end(valist)`, leaving the variadic state unclosed on
  the throwing path; `va_end` runs right after `vsnprintf` in both.
  > **Status:** fixed (seam/item-literal-throws)

- **`EventManager::addEvent` had its debug macros in the wrong order**
  (`__BEGIN_TRY __END_DEBUG` ... `__BEGIN_DEBUG __END_CATCH`), which
  expands to an empty `try` with two handlers, then the whole body outside
  any `try`, then a second empty one: the duplicate-event-class `Error` it
  raises reached the caller with nothing printed and no frame added to its
  stack. Correcting the order to `__BEGIN_TRY __BEGIN_DEBUG` ...
  `__END_DEBUG __END_CATCH` puts the body inside both, so the refusal is
  written to standard output and carries `addEvent` in its trace. It still
  propagates: both macros rethrow.
  > **Status:** fixed (seam/item-literal-throws)

## Chat, nickname, union and SMS write/read disagreements (2026-09-13)

The eleven findings task 1.2 stated as flip-tests in
`tests/packet_chat_test.cpp`, the initialisation split the same file
pinned, and the seven its header recorded rather than tested. These
packets are client-facing, so `write()` and `getPacketSize()` are the
contract: every fix is a refusal, a cap at the width the factory max
budgets, a size-accounting correction, a read-side correction or an
initialisation. **No golden changes.** Two `tests/wire-layout.txt` lines
move — `GCNicknameList` 13001 → 6631 and `CGSMSSend` 108 → 114 — both
read-buffer budgets, not fields on the wire.

- **`GCSystemMessage` carried a `Race_t` with a getter and a setter
  neither `read()` nor `write()` touches**, so a sender's `setRace` never
  left the process. The client's own reader
  (`Client/Packet/Gpackets/GCSystemMessage.cpp`) takes the message behind
  its length byte, the colour and the type byte and stops; its
  `GCSystemMessage.h` declares no race member at all. The two mentions in
  `src/server` — `CGRangerSayHandler.cpp`'s `setRace` and
  `ZonePlayerManager.cpp`'s `getRace` — both sit inside commented-out
  blocks, so nothing reachable used it. The member and both accessors are
  gone; the commented blocks are left as they are.
  > **Status:** fixed (wire/chat-disagreements)

- **`GCModifyNickname`'s record pointer started indeterminate and
  `getPacketSize()`, `write()` and `read()` all dereferenced it.** A
  sender that skipped `setNicknameInfo` followed an indeterminate value
  and a receiver wrote a whole record through one. All four senders hand
  over a record they keep — the character's `NicknameBook`'s in
  `CGModifyNicknameHandler`, `CGSelectNicknameHandler` and
  `PlayerCreature::whenQuestLevelUpgrade`, a function-local `static` or a
  stack `NicknameInfo` in the two admin commands, which live in
  `gm/ConsoleCommands.cpp` since the 4.1 router — so the packet
  tracks which record is its own the way `GCGQuestStatusModify` does: the
  pointer starts empty, `getPacketSize()` and `write()` refuse on it,
  `read()` allocates and owns the record it fills and frees it before
  allocating the next, and the destructor frees that and never a
  sender's.
  > **Status:** fixed (wire/chat-disagreements)

- **`GCRequestFailed::setCode` took a `WORD` into a `BYTE`, and the
  name's length byte was derived with no bound** while the factory max
  budgets ten bytes for it, so an eleven-byte name already outgrew the
  read buffer the receiver sizes from that max. The setter takes the
  `BYTE` the wire carries (its one caller passes `REQUEST_FAILED_IP`),
  the name is cut to `kMaxNameLength` in the setter and carried through
  `de::wire` in `write()` and `read()`, and the zero-length refusal both
  halves raised as `ProtocolException("")` — a log line naming no field —
  is now the helper's `InvalidProtocolException` naming `Name`. The max
  is unchanged, restated as that constant.
  > **Status:** fixed (wire/chat-disagreements)

- **`CGModifyNickname::setItemObjectID` took a `WORD`** while the member
  it writes and the wire both carry an `ObjectID_t`, so no caller could
  set an item id past 65535. It takes the full `ObjectID_t`; no
  `src/server` source calls it.
  > **Status:** fixed (wire/chat-disagreements)

- **`NicknameInfo::write` bounded its custom nickname at the 255 its
  length byte carries where `read()` stopped at `MAX_NICKNAME_SIZE` and
  refused an empty one**, so the record `write()` emits for a
  `NICK_CUSTOM` slot with no text could not be read back. All three sides
  agree at `{0, MAX_NICKNAME_SIZE}` now. The empty value is admitted
  rather than refused because senders produce it —
  `CGModifyNicknameHandler`'s add-a-nickname branch builds a
  `NICK_CUSTOM` record straight from the packet's nickname without
  checking it, and `CGModifyNickname::read` admits an empty one — and
  because the client's own `NicknameInfo::read` writes the field only
  when its length byte is non-zero. The three `Assert(false)` defaults
  that closed the switch in `getSize()`, `read()` and `write()`, which
  append to `assertion_failed.log` in the working directory before they
  throw, are `InvalidProtocolException`s; in `read()` that is also the
  range check on the type byte.
  > **Status:** fixed (wire/chat-disagreements)

- **Three counted lists derived a count byte they capped nowhere**, so
  the 256th entry wrapped it to zero while `write()` still emitted every
  one: `GCNicknameList`'s records, `GCSMSAddressList`'s entries and
  `CGSMSSend`'s receivers. Each is held to what its own maximum budgets —
  255, `MAX_ADDRESS_NUM` 30 and `MAX_RECEVIER_NUM` 5 — in `write()` and
  in `read()`. `GCNicknameListFactory`'s max budgeted `MAX_NICKNAME_NUM`
  500 records, twice what the count byte in front of them can ever
  describe, so `MAX_NICKNAME_NUM` is the 255 that byte carries and the
  max follows it: `GCNicknameList` 13001 → 6631.
  `SMSAddressBook::addAddressElement` compared its size with `>` before
  inserting, so the book could reach 31 entries and the listing packet
  refuse to write at all; it stops at the thirty the listing carries.
  > **Status:** fixed (wire/chat-disagreements)

- **`CGSMSSendFactory`'s maximum budgeted `MAX_RECEVIER_NUM` (5) bytes
  for the caller number where `read()` accepts `MAX_NUMBER_LENGTH`
  (11)** — the wrong constant in the arithmetic — so a packet built at
  the lengths `read()` admits outgrew the read buffer by six bytes. The
  max budgets the length it accepts, and a packet at every read cap is
  now exactly that max: `CGSMSSend` 108 → 114. The same packet derived
  all four of its length bytes from the strings with no bound, so a
  message past `MAX_MESSAGE_LENGTH` was emitted whole and one of 256
  wrapped its byte to zero; every string travels through `de::wire` now,
  at the widths the max budgets. Its four `Assert()` bounds on the read
  side, which wrote `assertion_failed.log` before throwing, are
  `InvalidProtocolException`s.
  > **Status:** fixed (wire/chat-disagreements)

- **`GCSystemMessage::read`, `GCKickMessage::read` and
  `GCKickMessage::setType` cast a byte straight to an enum narrower than
  a byte.** `SystemMessageType` declares eight values and
  `KickMessageType` two, so a byte above their range is an out-of-range
  enum load, which the Debug toolchain's UBSan traps on. Each tests the
  raw `BYTE` against the enum's `_MAX` before assigning.
  > **Status:** fixed (wire/chat-disagreements)

- **`~GCNicknameList` and `~GCSMSAddressList` freed no record, and both
  `read()`s cleared the vector without freeing what it held**, so every
  record a `read()` allocated leaked. The two differ in who owns what.
  `GCNicknameList`'s one sender, `NicknameBook::getNicknameBookListPacket`,
  pushes the book's own records, so the listing starts unowned and the
  packet frees only what `read()` allocated. Every `GCSMSAddressList`
  entry is the packet's: its one sender,
  `SMSAddressBook::getGCSMSAddressList`, builds a fresh `AddressUnit` per
  address through `SMSAddressElement::getAddressUnit` and keeps only the
  element, so the destructor frees the whole listing and `read()` frees
  before replacing it. `CGSMSAddressListHandler` also never deleted the
  packet it sent — the nickname list's handler does — so the records
  leaked with it; it deletes it now.
  > **Status:** fixed (wire/chat-disagreements)

- **Fifteen of the thirty-one left at least one member the default
  constructor never sets**, so a packet sent without every setter called
  put indeterminate bytes on the wire: `GCSay`'s object id and colour,
  `GCWhisper`'s and `GCGlobalChat`'s colour and race, `CGGlobalChat`'s
  colour, `GCKickMessage`'s seconds, `CGSelectNickname`'s nickname id,
  `CGModifyNickname`'s item id, the six union packets' guild id,
  `CGQuitUnion`'s quit method, `CGAppointSubmaster`'s guild id and
  `CGDeleteSMSAddress`'s element id. All 31 initialise every member their
  `write()` emits now, pinned over poisoned storage.
  > **Status:** fixed (wire/chat-disagreements)

- **`GCFriendChatting` capped its message at 128 on read and 512 on
  write, and admitted on write the empty name and message `read()`
  refused; `CGAddSMSAddress` admitted all three of its empty fields on
  write and refused them on read.** Both are read-side widenings, because
  the write side is what the senders and the client already agree on: the
  client's `GCFriendChatting::read` refuses only past 32 and 512 and
  writes each field only when its length is non-zero, most of
  `GCFriendChattingHandler`'s sends leave both strings empty, and the
  client's `CGAddSMSAddress::write` admits an empty value in each of the
  three. The pins in `tests/packet_roundtrip_test.cpp` follow.
  > **Status:** fixed (wire/chat-disagreements)

## A bare string-literal throw walked past every handler written for it (2026-09-10)

- **149 sites answered a refusal with `throw "text"`, a `const char*`
  nothing in the tree catches.** `__END_CATCH` rethrows a `Throwable` and
  the 344 `__END_CATCH_NO_RETHROW` sites swallow one; `__END_DEBUG_EX`,
  which every CG handler ends with, rethrows an `Error` and logs a plain
  `Exception`; a skill's own `catch (Throwable& t)` runs its
  `executeSkillFailException` branch. A `const char*` matches none of
  them, so it walked past the handler the surrounding code had written for
  exactly this failure and landed in whatever `catch (...)` stood highest —
  `main()`'s "unknown exception...", `GamePlayer::processCommand`'s
  disconnect, `ManagedThread`'s `ServerShutdown::fail()` — with the message
  dropped on the way. Out of a destructor there is no backstop at all:
  `~GamePlayer` is implicitly `noexcept`, so its literal was
  `std::terminate` for the whole game server whenever a departing player's
  party lookup missed. The 62 sites outside `src/server/gameserver/item`
  now throw `Error` with the same text, the 15 Korean ones translated.
  Each lands in the handler that was already there: a load failure reaches
  `main()`'s `catch (Throwable&)`, which writes it to `../log/instant.log`
  and refuses to start where the literal only printed "unknown
  exception..."; a skill's missing-ammunition refusal reaches the skill's
  own fail branch instead of disconnecting the player; a zone tick failure
  reaches `zoneGroupThreadError.log` before the same shutdown; and
  `~GamePlayer`'s reaches the destructor's own `__END_CATCH_NO_RETHROW`,
  which swallows it after the `cerr` line the site already writes — a
  swallow accepted deliberately, because terminating every logged-in
  session over one departing player's party bookkeeping is not a refusal
  worth keeping. The item classes' identical `Invalid item type or
  optionType` constructor refusal, 87 sites, one per class, went the same
  way. It is raised only by the `(itemType, optionType)` constructor the
  `ItemFactory` subclasses call, never by the argument-less one every
  `<Class>Loader::load` uses, so no database row is skipped at startup by
  the change. Eleven of the paths that reach a factory do land somewhere
  else now. Four improve: the item-making skills (`CreateBomb`,
  `CreateMine`, `CreateHolyWater`, `AbsorbSoul`) already catch `Throwable`
  and answer with their own skill-fail packet, so a refusal ends the skill
  instead of disconnecting the player. Seven turn a kill into a swallow:
  monster loot (`MonsterManager::processCreatures`) and NPC-triggered quest
  actions (`NPCManager::processCreatures`) logged nothing and took the whole
  server down through `ManagedThread`'s `catch (...)`, and now log one line
  and abandon the rest of that tick; the GM relic command, the NPC-dialogue
  rewards, the event-tree inventory move, the quick-slot motorcycle key and
  `ActionRedeemMotorcycle` disconnected the player and now swallow, four of
  them with their diagnostic commented out. All eleven are accepted: the
  `filelog("itembug.log", ...)` line beside every one of these throws
  reports the failure whatever catches it, and a malformed `ItemInfo` row is
  not worth a server. Ratchet R11 is 0, so a new literal anywhere in `src`
  fails it.
  > **Status:** fixed; the 62 sites outside `src/server/gameserver/item` in
  > seam/statement-leak, the 87 item constructors in
  > seam/item-literal-throws

## A statement that met anything but a SQL failure was never closed (2026-09-10)

- **`BEGIN_DB`/`END_DB` closed the `Statement` only in the clause that
  answers a failed statement.** The shape all 682 sites share is
  `Statement* pStmt = NULL; BEGIN_DB { pStmt = conn->createStatement();
  ...; SAFE_DELETE(pStmt); } END_DB(pStmt)`, and that `SAFE_DELETE` sits
  inside the try. Any other exception raised between the query and it — a
  `bad_alloc`, an `OutOfBoundException` or `NoSuchElementException` from a
  container walked while building the row, an `Error` from a helper called
  with the statement still open — carried the site's only reference away
  and leaked the `Statement` together with the `Result` it owns, which for
  a SELECT is the whole result set. `END_DB`, `END_DB_EX` and
  `MySQLSMSMessageRepository.cpp`'s file-local `END_DB_RETHROW` now carry a
  `catch (...)` that deletes the statement and rethrows unchanged, so every
  site is covered without touching one. Nothing is logged there: the
  failure is not the statement's, and the handler that catches it decides
  what to say. Double deletion cannot follow, because `SAFE_DELETE` clears
  the pointer: a site that finished early leaves NULL behind, and the four
  blocks in `MySQLExchangeRepository.cpp` that open a second statement each
  do it after a `SAFE_DELETE` of the first. `tests/database_error_test.cpp`
  pins the clause with no database, counting destructions through a
  `Statement` subclass declared at the site — the macro deletes the pointer
  with the type the site gives it — for a `Throwable`, for a
  `std::bad_alloc`, for the SQL clause, and for a site that already closed
  its statement.
  > **Status:** fixed (seam/statement-leak)

## Quest, war and zone-selection write/read disagreements (2026-09-10)

The fifteen findings task 1.2 stated as flip-tests in
`tests/packet_quest_war_test.cpp`, the initialisation split the same file
pinned, and the six its header recorded rather than tested. These packets
are client-facing, so `write()` and `getPacketSize()` are the contract:
every fix is a refusal, a cap at the width the factory max budgets, a
size-accounting correction, a read-side correction or an initialisation.
**No golden changes.** Two `tests/wire-layout.txt` lines move —
`GCWarList` 13344 → 14257 and `GCWarScheduleList` 2281 → 2401 — both
read-buffer budgets for packets no server reads, not fields on the wire.

- **`GCRegenZoneStatus::read` appended the eight bytes it took off the
  wire to the eight its constructor had already pushed into a
  `vector<BYTE>`**, so `getStatus()` kept returning the constructor's
  zeros and `write()` emitted them again: the packet could not round trip
  at all. `write()` always emits exactly eight, so the statuses are a
  fixed eight-slot array `read()` writes into, and a slot outside the
  eight is refused in the getter and the setter instead of indexing past
  the end. `RegenZoneManager` indexes those slots with the
  `RegenZonePosition.id` column, whose seed rows are 0..7.
  > **Status:** fixed (wire/quest-war-disagreements)

- **`GCMiniGameScores::getPacketSize` never advanced its iterator**, so
  it counted the first name's length once per entry and a table of names
  of different lengths declared a size `write()` did not send —
  `writePacket()` puts that size on the wire before `write()` runs, so
  the stream never resynchronises. It walks the table now. The same
  packet derived each name's length byte from the name with no bound
  while its max budgets twenty bytes for one, so a name of 256 wrapped
  the byte to zero while `write()` emitted the whole string: `addScore`
  cuts the name to twenty, `write()` and `read()` carry the field through
  `de::wire`, and `read()` refuses a table past the ten `write()` emits.
  The max is unchanged, restated as its two constants.
  > **Status:** fixed (wire/quest-war-disagreements)

- **`GCNoticeEvent::getCode` returned a `BYTE` of the `WORD` it holds and
  puts on the wire**, so a caller could not see a code past 255, and
  **`setParameter(WORD, WORD)` assigned `makeDWORD` of its two halves to
  the code** rather than the parameter, overwriting the notice with the
  low half. Both are fixed; no `src/server` source called either. The
  code was also never compared against `NOTICE_EVENT_MAX`, so a peer
  could announce a notice no branch of the client draws: `read()` refuses
  on the raw `WORD` before it reaches the member the three switches read.
  > **Status:** fixed (wire/quest-war-disagreements)

- **`MissionInfo::write` printed every mission it emitted to standard
  output**, on the wire path, once per player the packet was sent to. The
  print is gone.
  > **Status:** fixed (wire/quest-war-disagreements)

- **Five counted lists derived a count byte they capped nowhere**, so the
  256th entry wrapped it to zero while `write()` still emitted every one:
  `GCGQuestStatusInfo`'s records, `QuestStatusInfo`'s missions,
  `GCWarList`'s wars, `GCWarScheduleList`'s entries and the `ValueList`
  carrying a guild war's join guilds and a race war's castles. Each is
  held to what its own maximum budgets — `MAX_QUEST_NUM` 100,
  `MAX_MISSION_NUM` 100, 24 wars, `MAX_WAR_NUM` 20 and 255 values — in
  the adder where there is one, in `write()` and in `read()`.
  `GCSelectQuestID` and `GCMonsterKillQuestInfo` did refuse a list past
  255, but through `Assert()`, which appends to `assertion_failed.log` in
  the working directory before it throws; both throw
  `InvalidProtocolException` now, and `GCMonsterKillQuestInfo` refuses in
  its adder as well.
  > **Status:** fixed (wire/quest-war-disagreements)

- **`GCWarListFactory`'s maximum budgeted twelve race wars and twelve
  guild wars and nothing else** — not the count byte, not the war type
  byte in front of each record, and not the level war its own `read()`
  builds — so twelve full guild wars and twelve full race wars already
  outgrew the buffer the receiver sizes from it. `write()` can put any of
  the three shapes behind each war type byte, so the max budgets the
  count byte and twenty-four times the widest of them, a `GuildWarInfo`:
  `GCWarList` 13344 → 14257. Twenty-four is the record count the old
  number already stood for, and `addWarInfo`, `write()` and `read()`
  refuse past it.
  > **Status:** fixed (wire/quest-war-disagreements)

- **`GCWarScheduleList` derived each of the six guild names' length bytes
  from the name with no bound.** The names are held to sixteen through
  `de::wire`, the width its max budgets — and which the golden's own
  sixteen-byte reinforcing guild name shows is the real field width — so
  a name of 256 can no longer wrap the byte while `write()` emits the
  whole string. The max never budgeted the length byte in front of each
  of those six names, so a full schedule of full names outgrew it by six
  bytes an entry; it counts them now: `GCWarScheduleList` 2281 → 2401.
  `WarScheduleInfo` also initialises every member, so an entry
  `read()` builds for a war type that carries no challengers no longer
  holds indeterminate guild ids.
  > **Status:** fixed (wire/quest-war-disagreements)

- **Seven readers appended to what the packet already held instead of
  replacing it**, so a reused packet grew by one listing per read:
  `GCSelectQuestID`, `GCGQuestStatusInfo`, `GCWarList`,
  `GCWarScheduleList`, `GCMonsterKillQuestInfo`, `QuestStatusInfo`'s
  mission list and `ValueList`. All seven replace what they hold at the
  top of `read()`, freeing the records they drop where they own them.
  > **Status:** fixed (wire/quest-war-disagreements)

- **`GCGQuestStatusInfo`'s destructor freed no record — the loop that did
  was commented out — and `GCGQuestStatusModify::read` allocated one on
  every call and freed none**, while its destructor dropped that one too.
  Every sender of both hands over a `GQuestStatus` the character's
  `GQuestManager` owns and keeps (`GQuestManager::getStatusInfoPacket`
  pushes `m_QuestStatuses` entries into the listing; `accept`, `cancel`,
  the two mission-progress sites and `GQuestStatus::update` hand over the
  status itself), so both packets track which record is their own: the
  listing and the pointer start unowned, `read()` frees what it allocated
  before allocating the next, and the destructor frees that and never a
  sender's. `QuestStatusInfo` owns the missions it holds and frees them,
  which is where the records `read()` allocates were leaking their
  missions; `GQuestStatus` no longer frees them itself.
  `GCGQuestStatusModify`'s record pointer starts empty and
  `getPacketSize()` and `write()` refuse on it rather than following the
  indeterminate value a sender that skipped `setInfo` left.
  > **Status:** fixed (wire/quest-war-disagreements)

- **`GCWarList::read` cast the war type byte straight to the `WarType`
  enum before switching on it.** The enum declares three values, so its
  range is 0..3 and any byte above that is an out-of-range enum load,
  which the Debug toolchain's UBSan traps on. The raw `BYTE` is tested
  against the three shapes `read()` builds before it reaches the enum.
  **`GCFlagWarStatus::getFlagCount` and `setFlagCount` indexed a
  three-element array with a `Race_t` they never bounded**; both refuse a
  race outside the three slots `write()` emits.
  > **Status:** fixed (wire/quest-war-disagreements)

- **Eighteen of the twenty-seven left at least one member the default
  constructor never set**, so a packet sent without every setter called
  put indeterminate bytes on the wire. All twenty-seven initialise every
  member now, pinned by constructing each over storage poisoned with two
  different bytes. `MissionInfo`, `GuildWarInfo` and `LevelWarInfo`
  initialise theirs with them.
  > **Status:** fixed (wire/quest-war-disagreements)

- **`GCAddHelicopter`'s code getter was named `setCode`**, overloading
  the setter. It is `getCode`; no `src/server` source called the getter.
  > **Status:** fixed (wire/quest-war-disagreements)

## Every SQL failure threw a dangling pointer (2026-09-10)

- **`END_DB` and `END_DB_EX` built the `DBError.log` line in a local
  `std::string` and then threw `msg.c_str()`**, a pointer into storage
  that dies as the catch clause unwinds. All 735 statement sites in
  `src/` answer a failed statement that way, and the 34
  `catch (const char*)` handlers in `src/server` receive it. Nothing
  crashed only because almost every one of them logged a fixed string
  and never read the pointer; the MySQL integration tier said so in a
  comment beside each refusal it pinned, having no way to assert on the
  error MySQL gave. The macros now throw a `DatabaseError`
  (`src/server/database/DatabaseError.h`), a value holding the same
  message, and the handlers catch that type. The ones that report the
  failure carry the real text rather than pointing at the log: the
  startup managers' `Error`, the login handlers' `DisconnectException`,
  `PaySystem`'s `paySystem.txt` lines and `CLDeletePCHandler`'s console
  line. `DatabaseError` derives from nothing on purpose — `__END_CATCH`
  rethrows a `Throwable` and `__END_CATCH_NO_RETHROW` swallows one, so a
  base would have changed how far a SQL failure travels before it
  reaches the handler that decides what the caller is told.
  `MySQLSMSMessageRepository`'s file-local `END_DB_RETHROW` is
  untouched: its thread catches the `SQLQueryException` itself to
  reconnect. `tests/database_error_test.cpp` pins the macros with no
  database, and the MySQL tier pins a statement against a missing table;
  both copy the error out of the handler and read its message after the
  catch block is gone. Ratchet R10 keeps both halves at zero.
  > **Status:** fixed (seam/db-error-type)

## Character progression write/read disagreements (2026-09-10)

The nine findings task 1.2 stated as flip-tests in
`tests/packet_skill_test.cpp`, the initialisation split the same file
pinned, and three of the four its header recorded rather than tested.
These packets are client-facing, so `write()` and `getPacketSize()` are
the contract: every fix is a refusal, a cap at the width the factory max
already budgets, a size-accounting correction, a read-side correction or
an initialisation. **No golden changes.** One `tests/wire-layout.txt`
line moves — `GCSkillInfo` 4338 → 34706 — a read-buffer budget for a
packet no server reads, not a field on the wire.

- **`GCSkillInfo` derived its record count into a `BYTE` it capped
  nowhere, and `GCSkillInfoFactory`'s maximum was one bare
  `SlayerSkillInfo`.** The 256th record wrapped the count to zero while
  `write()` still emitted every record, and the max budgeted neither the
  pc type byte nor the record count byte nor a second record, so a single
  full record already outgrew the buffer the receiver sizes from it. A
  book carries one record per skill domain — `SKILL_DOMAIN_MAX`, 8, the
  bound `Slayer::sendSlayerSkillInfo` fills against and the one a vampire
  and an ousters use one slot of — and `addListElement`, `write()` and
  `read()` refuse past it. The max budgets the two header bytes and eight
  slayer records, the widest of the three races: `GCSkillInfo`
  4338 → 34706.
  > **Status:** fixed (wire/skill-disagreements)

- **`SlayerSkillInfo`, `VampireSkillInfo` and `OustersSkillInfo` kept
  their skill count in a `BYTE` the caller set by hand while
  `addListElement` left it alone.** `write()` emitted that count and then
  the whole list, so a record whose list was longer sent skills the
  reader never consumed and `getPacketSize()` counted them. The count is
  the list now, held to the 255, 120 and 120 each record's own
  `getMaxSize()` budgets in the adder, in `write()` and in `read()`.
  `setListNum` is gone from all three, with the `SkillCount` bookkeeping
  in `Slayer.cpp`, `Vampire.cpp` and `Ousters.cpp` that maintained it.
  > **Status:** fixed (wire/skill-disagreements)

- **`GCSkillInfo::read` appended to the book the packet already held**,
  as `GCRankBonusInfo`, `GCSweeperBonusInfo`, `GCHolyLandBonusInfo` and
  `GCBloodBibleList` did to their lists, so a reused packet declared one
  listing and held two. All five replace what they hold at the top of
  `read()`, with `GCSkillInfo` destroying the records it drops instead of
  leaking them.
  > **Status:** fixed (wire/skill-disagreements)

- **`GCSkillInfo::write` emitted any pc type while `read()` refused every
  one but `PC_SLAYER`, `PC_VAMPIRE` and `PC_OUSTERS`**, so a book
  announced with any other type went out and could not be read back — and
  the type was one of the members the constructor left alone. The setter,
  `write()` and `read()` refuse the same three-value set, testing the raw
  `BYTE` before it reaches an enum, which the Debug toolchain's UBSan
  traps on.
  > **Status:** fixed (wire/skill-disagreements)

- **`GCRankBonusInfo`, `GCSweeperBonusInfo` and `GCHolyLandBonusInfo`
  derived their count byte from a list they bounded against nothing.**
  The 256th entry wrapped the count to zero while `write()` still emitted
  every entry, and the body outgrew maxima budgeting 100, 12 and 12
  entries. Each is held to its own max's number in the adder, in
  `write()` and in `read()`. `GCBloodBibleList`, whose list is reached
  through `getList()`, only `Assert`ed the same bound and now refuses past
  the 12 its max budgets in `write()` and in `read()`. All four maxima
  were already right, so no wire-layout line moves for them.
  > **Status:** fixed (wire/skill-disagreements)

- **`SweeperBonusInfo` and `BloodBibleBonusInfo` carried a type the
  managers set on every record and the wire never took.** Both `read()`
  and `write()` handle the race byte alone; the type and the option list
  are commented out on both sides. The client decides which side is right
  and it reads no type: its own
  `Client/Packet/Gpackets/SweeperBonusInfo.cpp` and
  `BloodBibleBonusInfo.cpp` read the race byte only, and
  `GCSweeperBonusInfoHandler` and `GCHolyLandBonusInfoHandler` use
  `getRace()` and never `getType()`. So the wire is right and the type was
  dead API: `m_Type` / `getType` / `setType` are gone from both records,
  with the two `SweeperBonusManager::makeSweeperBonusInfo` /
  `makeVoidSweeperBonusInfo` calls that set it. Both records' `getSize()`
  and `getMaxSize()` already counted the race byte alone, so neither
  factory max moves. The option list the same two calls still fill is
  dead the same way and is left alone.
  > **Status:** fixed (wire/skill-disagreements)

- **`GCBloodBibleSignInfo` left its record pointer uninitialised and both
  `getPacketSize()` and `write()` dereferenced it**, while `read()`
  allocated a record on every call and freed none and the destructor
  dropped the pointer. Every sender hands the packet the character's own
  `BloodBibleSignInfo` (`PlayerCreature::getBloodBibleSign`) and keeps it,
  so the packet tracks which record is its own: the pointer starts empty
  and `getPacketSize()` and `write()` refuse on it, `read()` frees the
  record it allocated before allocating the next, and the destructor frees
  that one and never a sender's.
  > **Status:** fixed (wire/skill-disagreements)

- **`BloodBibleSignInfo::read` appended to the sign list it already held
  and bounded the count against nothing**, where `write()` emits at most
  the six slots the record has and `getMaxSize()` budgets six. It replaces
  the list and refuses a count past `BLOOD_BIBLE_SIGN_SLOT_NUM`.
  > **Status:** fixed (wire/skill-disagreements)

- **Eighteen of the twenty-nine left at least one member the default
  constructor never set**, so a packet sent without every setter called
  put whatever the allocation held on the wire. All twenty-nine
  initialise every member now, pinned by constructing each over storage
  poisoned with two different bytes.
  > **Status:** fixed (wire/skill-disagreements)

- **Each race record's learn flag and a slayer skill's enable flag were
  read straight into a `bool`**, so a peer sending any byte but 0 or 1
  left a `bool` holding a value no `bool` may hold. Each travels as a
  `BYTE` that `read()` holds to 0 or 1 before it reaches the member. A
  sender writes a real `bool`, so no byte on the wire changes.
  > **Status:** fixed (wire/skill-disagreements)

- **Four `throw "..."` raw string literals in the packet layer could be
  caught by nothing.** `__END_CATCH` catches `Throwable`, so an
  out-of-range index in `GCShopList::getShopItem`,
  `GCShopListMysterious::getShopItem`, `GCShopVersion::getVersion` or
  `GCShopVersion::setVersion` unwound past every handler on the path.
  All four throw `InvalidProtocolException`, pinned in
  `tests/packet_store_test.cpp`. The two remaining raw-literal throws in
  `src/Core` (`SXml.cpp` and `Utility.cpp`, both in `filelog()`) are not
  in the packet layer and are left alone.
  > **Status:** fixed (wire/skill-disagreements)

- **`GCUsePowerPointResult::read` and `GCRequestPowerPointResult::read`
  took their code bytes without comparing them against the last
  enumerator of the `RESULT_CODE` and `ITEM_CODE` lists their own headers
  declare**, so a peer could announce a result the client's handlers only
  met in their `default:` branch, a generic error popup. Both reads now
  refuse a code past the last enumerator
  (`kLastResultCode`, `kLastItemCode`) with `InvalidProtocolException`,
  pinned by refusal tests in `tests/packet_skill_test.cpp`. The fixtures
  behind the two goldens carried out-of-range codes -- `0x9D` / `0x9E` and
  `0xA1`, chosen to follow the file's `>= 128` byte rule -- and were
  re-recorded with each list's last enumerator, which pins the boundary:
  `GCUsePowerPointResult.code0.hex` and
  `GCRequestPowerPointResult.code0.hex` change in their code bytes only.
  That is fixture content, not layout: no size or inventory line moves,
  every sender already emits enumerators, and the client repo holds no
  copy of these goldens. The client is the receiver of both packets, and
  its own copies of the two reads (`Client/Packet/Gpackets/`) carry the
  identical check now (client branch `fix/server-round-48-reads`), where
  such a frame is a read failure and a disconnect rather than the popup.
  > **Status:** fixed (fix/exchange-residue)

## Hard-coded BBS credentials in the `*notice` operator command (2026-09-10)

- **`CGSayHandler::opnotice` opened a MySQL connection to a
  third-party host whose address, database, user and password were
  literals in the source**, and inserted the GM's chat text into that
  server's `quick1001` table **unquoted and unescaped**: the text went
  straight between the VALUES parentheses, so whatever a GM typed was
  SQL. The credentials are not moved into a repository. Nothing else
  happened in the command — it answered the player nothing and touched
  no game state — so the command is deleted: the dispatch branch that
  reached it and the whole body. Its declaration stays in
  `src/Core/CGSay.h`, the packet header the client repo mirrors.
  > **Status:** fixed (seam/inline-sql-residue)

- **`MySQLSMSMessageRepository` turned every SQL failure into a
  `const char*`, so the SMS relay could never reconnect.**
  `SMSServiceThread::run()` catches `SQLQueryException` to call
  `reopen()` on the repository, but the repository's statements sat
  inside `END_DB`, which logs to `DBError.log` and then throws
  `msg.c_str()` of a local string. Neither that branch nor the
  thread's `catch (Throwable&)` matches a `const char*`, so a dropped
  relay connection left the loop instead of being reopened. The
  repository now catches the `SQLQueryException` itself, writes the
  same `DBError.log` line and rethrows it; `END_DB` and its other call
  sites are untouched. Dormant: `GameServer::start()` never starts the
  thread.
  > **Status:** fixed (seam/inline-sql-residue)

## Store, shop and stash write/read disagreements (2026-09-10)

The nine findings task 1.2 stated as flip-tests in
`tests/packet_store_test.cpp`, the initialisation split the same file
pinned, and two entry counts the inventory round left near-missed. These
packets are client-facing, so `write()` and `getPacketSize()` are the
contract: every fix is a refusal, a cap at the width the factory max
already budgets, a size-accounting correction, a read-side correction or
an initialisation. **No golden changes.** Six `tests/wire-layout.txt`
lines move — `GCPetStashList` 1021 → 1020, `GCShopBought` 284 → 59,
`GCShopBuyFail` 4 → 9, `GCShopBuyOK` 287 → 62, `GCShopList` 5515 → 1015
and `GCStashList` 21006 → 7506 — all read-buffer budgets, not fields on
the wire.

- **`GCShopBuyFail::getPacketSize()` counted the NPC id alone while
  `write()` also emitted the fail code and the four-byte amount, and the
  factory max was the same four bytes.** Every refusal declared a body
  five bytes shorter than it sent, so `writePacket()` put a length on the
  wire that did not describe the body behind it and the stream never
  resynchronised; the receiver's read buffer, sized from the max, was
  overrun by the same five bytes. The size and the max count all three
  fields: `GCShopBuyFail` 4 → 9.
  > **Status:** fixed (wire/store-disagreements)

- **`CGStoreSign` derived the sign's length byte by hand and capped
  nothing.** A sign past the 80 bytes its factory max budgets was emitted
  whole, and at 256 bytes the length byte wrapped to zero while the text
  still followed it. The field goes through `de::wire::readString` /
  `writeString` at `MAX_SIGN_SIZE`, and the setter refuses a longer one,
  so the length always describes the bytes behind it. A sign of 0..80
  bytes travels unchanged.
  > **Status:** fixed (wire/store-disagreements)

- **`CGStoreSign::write()` emitted an empty sign as a bare zero length
  byte that `read()` could not take back.** The hand-written sequence
  handed the zero to `SocketInputStream::read(string&, uint)`, which
  refuses a zero length, so a stall named with an empty string was
  written and never read. The helper's bounds admit an empty value
  (`{0, MAX_SIGN_SIZE}`) and `write()` is byte-identical.
  > **Status:** fixed (wire/store-disagreements)

- **`StoreInfo::setSign` capped nothing and `StoreInfo::write` bounded
  the sign at 255 while `StoreInfo::getMaxSize` budgets 80.** A full
  stall whose sign was longer made `GCMyStoreInfo` and `GCOtherStoreInfo`
  outgrow their factory maxima by the overage. The setter cuts at
  `MAX_SIGN_SIZE`, the way `StoreOutlook::setSign` already did — the two
  carriers of the same player-typed field behave alike, and
  `tests/packet_zone_scan_test.cpp` pins the cut for both — and `read()`
  and `write()` bound the field there too, so a peer cannot smuggle a
  longer one past the record's budget.
  > **Status:** fixed (wire/store-disagreements)

- **`GCShopBought`, `GCShopBuyOK`, `GCShopList` and `GCStashList` bounded
  their option lists against nothing.** The count travels in a `BYTE`, so
  the 256th option wrapped it to zero while `write()` still emitted every
  option, and the four maxima budgeted 255 options per item where an item
  carries at most `MAX_ITEM_OPTION_NUM` (30) — a code sheet's stone grid.
  The lists are held to that bound in the adders, the setters, `write()`
  and `read()`, and the maxima budget it: `GCShopBought` 284 → 59,
  `GCShopBuyOK` 287 → 62, `GCShopList` 5515 → 1015 and `GCStashList`
  21006 → 7506, the last two twenty and sixty slots deep.
  > **Status:** fixed (wire/store-disagreements)

- **The same four read their option list into whatever the packet already
  held.** A reused packet grew by one listing per read, so it declared
  one listing and held two. `GCShopBought::read` and `GCShopBuyOK::read`
  clear the list first; `GCShopList::read` and `GCStashList::read` reset
  every slot, which is what a full listing replaces, and `GCStashList`
  destroys the sub-item records it drops instead of leaking them.
  > **Status:** fixed (wire/store-disagreements)

- **`GCPetStashList` never put its code byte on the wire although three
  senders set it and its factory max budgeted a byte for it.** The client
  decides which side is right and it reads no code:
  `Client/Packet/Gpackets/GCPetStashList.cpp` reads, per slot, an
  availability byte, the `PetInfo` and the keep days, and
  `GCPetStashListHandler` never calls `getCode()`. So the wire is right
  and the code was dead API: `m_Code` / `getCode` / `setCode` are gone,
  with the `setCode` calls in `ActionPetDeposit`, `ActionPetWithdraw` and
  `CGSayHandler`'s `PetStash` command, and the max no longer budgets a
  byte no body carries: `GCPetStashList` 1021 → 1020.
  > **Status:** fixed (wire/store-disagreements)

- **`GCStashList` kept each slot's sub-item count in an array of its
  own while the records came from a separate list.** A sub-item added
  through `getSubItems()` left the count behind, so `write()` emitted the
  stale count and then the whole list and `getPacketSize()` counted the
  stale one. The count is the list, refused past the eight belt pockets
  the max budgets in `write()` and in `read()`; the count array and the
  bookkeeping in `GCStashListFill.cpp` are gone, and a slot entry holding
  no record is refused rather than skipped, which would leave the body one
  record short of the count.
  > **Status:** fixed (wire/store-disagreements)

- **`GCMyStoreInfo` and `GCOtherStoreInfo` left their `StoreInfo`
  pointer uninitialised, and both `getPacketSize()` and `write()`
  dereferenced it.** A sender that skipped `setStoreInfo` followed an
  indeterminate value. The pointer starts empty and `getPacketSize()`,
  `write()` and `read()` throw `InvalidProtocolException` on it.
  > **Status:** fixed (wire/store-disagreements)

- **Twenty-five of the thirty-six left at least one member the default
  constructor never set**, so a packet sent without every setter called
  put whatever the allocation held on the wire. All thirty-six initialise
  every member now, pinned by constructing each over storage poisoned
  with two different bytes.
  > **Status:** fixed (wire/store-disagreements)

- **`GCTimeLimitItemInfo` and `CGTypeStringList` derived an entry count
  from a list they bounded against nothing.** Both counts travel in a
  `BYTE`; `GCTimeLimitItemInfo` only `Assert`ed the bound and
  `CGTypeStringList` did not check at all, so a listing past what the
  factory max budgets outgrew the receiver's read buffer. Each is held to
  the number its own max already budgets — 100 time-limit entries, 20
  strings of 50 bytes — in the adder, in `write()` and in `read()`. Both
  maxima were already right, so no wire-layout line moves.
  `CGTypeStringList::popString` called `front()` on a list that may be
  empty and throws now, and the packet initialises its type and its
  parameter.
  > **Status:** fixed (wire/store-disagreements)

## Inventory and item handling write/read disagreements (2026-09-10)

The fourteen findings task 1.2 stated as flip-tests in
`tests/packet_inventory_test.cpp`, plus the one its PR recorded rather
than tested. These packets are client-facing, so `write()` and
`getPacketSize()` are the contract: every fix is a refusal, a cap at the
width the factory max already budgets, a size-accounting correction, a
read-side correction or an initialisation. One golden pair moves
deliberately — `GCMakeItemOK` carried a count byte twice and the client
reads it once — and no other golden changes. The three
`tests/wire-layout.txt` movements are read-buffer budgets, not fields on
the wire.

- **`GCAddItemToInventory::write()` emitted the option count twice while
  `read()` consumed it once, and it wrote an item count
  `getPacketSize()` never budgeted for.** `GCMakeItemOK`, the only
  packet that puts the record on the wire, declared a body two bytes
  shorter than it sent, so the receiver took the second count as the
  first option, read every field behind the option list a byte early and
  ran the stat record off the end of the body. The client decides which
  side is right and it reads one count: its own
  `Client/Packet/Gpackets/GCAddItemToInventory.cpp` reads the object id,
  the two inventory coordinates, the class, the type, **one** option
  count, the options, the durability and the item count, and
  `GCMakeItemOKHandler` uses `getItemNum()`. So the duplicated write is
  the bug: `write()` emits one count and `getPacketSize()` counts the
  item count. `tests/golden/GCMakeItemOK.code0.hex` and
  `GCMakeItemOK.nooptions.code0.hex` are re-recorded one byte shorter —
  a deliberate wire fix, and the only golden change in the set.
  > **Status:** fixed (wire/inventory-disagreements)

- **The record's option list was bounded against nothing.** The count
  travels in a `BYTE`, so the 256th option wrapped it to zero while
  `write()` still emitted every option. The list is held to
  `MAX_ITEM_OPTION_NUM` (30) in the adder, in the setter, in `write()`
  and in `read()`. That is the widest option list an item carries: a
  code sheet keeps its stone grid there and holds exactly 30
  (`CodeSheet::CodeSheet` pads to 30, `CGAddItemToCodeSheetHandler`
  refuses a sheet with fewer), and every other item class holds at most
  three.
  > **Status:** fixed (wire/inventory-disagreements)

- **`GCAddItemToItemVerify::getPacketSize()` had no case for its
  THREE_ENCHANT_OK branch.** That result declared a bare code byte while
  `write()` gave it the code and two parameters, so the packet put a
  length on the wire eight bytes short of the body behind it. The size
  counts both parameters now.
  > **Status:** fixed (wire/inventory-disagreements)

- **`GCAddItemToItemVerify`'s second parameter was the one member the
  constructor left alone**, and the THREE_ENCHANT_OK branch put it on
  the wire, so a sender that skipped `setParameter2()` sent whatever the
  allocation held. It starts at zero.
  > **Status:** fixed (wire/inventory-disagreements)

- **`GCChangeInventoryItemNum` kept its material count in a `BYTE` it
  incremented per entry and bounded nowhere.** The 256th material
  wrapped the count to zero while `write()` still emitted every pair;
  `setChangedItemListNum()` let the count and the lists disagree, and
  the declared size then described neither; and
  `popFrontChangedItemListElement()` took an entry off the list without
  taking it off the count. The count is the list now, refused past
  `kMaxCount` — 255, what the count byte carries — in the adder and in
  `write()`; the setter is gone; popping an empty list is an
  `InvalidProtocolException` rather than a `front()` on nothing; and
  `read()` clears before it parses. The bound is the count byte's
  because the record has no live fill site to take one from:
  `CGMakeItemHandler::execute` is commented out in its entirety, no
  source calls `addChangedItemListElement`, and there is no recipe table
  in `initdb/` saying how many materials a craft consumes.
  > **Status:** fixed (wire/inventory-disagreements)

- **A full material list was five times what the two crafting factory
  maxima budgeted for it.** Both budgeted a flat 255 bytes for a record
  that reaches 1 + 255 * (`szObjectID` + `szItemNum`) = 1276, and
  `GCMakeItemOK` budgeted another flat 255 for an item record it can now
  measure. Both maxima are built from the two records' own
  `getPacketMaxSize()`: `GCMakeItemFail` 2297 -> 3318 and `GCMakeItemOK`
  2552 -> 3363, server-side read-buffer budgets and not fields on the
  wire.
  > **Status:** fixed (wire/inventory-disagreements)

- **`GCCreateItem` derived its option count from the list and bounded
  nothing**, so the 256th option wrapped the count and the body outgrew
  the 255 options its factory max budgeted. It is held to the same
  `MAX_ITEM_OPTION_NUM` in the adder, the setter, `write()` and
  `read()`, and its max budgets that: `GCCreateItem` 277 -> 52.
  > **Status:** fixed (wire/inventory-disagreements)

- **`GCGQuestInventory` counted its item list in a `BYTE` it never
  bounded, and `read()` appended to the list the packet already held.**
  A guild-quest inventory past 255 items wrapped the count, one past the
  100 the factory max budgets outgrew the receiver's read buffer, and a
  packet read into twice declared one listing and held two. The list is
  held to `MAX_GQUEST_INVENTORY_ITEM_NUM` in a new `addItem()`, in
  `write()` and in `read()`, and `read()` clears before it parses. The
  server's own adds go through `GQuestInventory::addOne`.
  > **Status:** fixed (wire/inventory-disagreements)

- **`GCTimeLimitItemInfo::getTimeLimit()` answered `0xffff` for an item
  it did not hold**, a value a real remaining time can equal, so a
  caller could not tell an absent item from one with 65535 seconds left.
  It returns `std::optional<DWORD>` now, with `hasTimeLimit()` beside
  it. The wire is untouched; the packet has no caller of the accessor in
  `src/server`.
  > **Status:** fixed (wire/inventory-disagreements)

- **Thirty of the thirty-seven packets left at least one member the
  default constructor never set**, so a packet sent without every setter
  called put whatever the allocation held on the wire. All thirty-seven
  initialise every member now, pinned by constructing each over storage
  poisoned with two different bytes and requiring the same body.
  > **Status:** fixed (wire/inventory-disagreements)

- **Three string fields still read a length into a local and handed it
  straight to `read(string&, uint)`** — `CGTypeStringList`'s list
  strings, `GCShowWaitGuildInfo`'s founding-member names and
  `GCSMSAddressList`'s three `AddressUnit` fields — a shape R9's grep did
  not match because it only looked for reads into a member. All go
  through `de::wire::readString`/`writeString` at the bounds their
  factory maxima budget, and R9's grep accepts any identifier and still
  measures 0.
  > **Status:** fixed (wire/inventory-disagreements)

- **Four `pop*()` methods called `front()` on a list that may be
  empty** — `GCNPCAskDynamic::popContent`,
  `GCRemoveEffect::popFrontListElement` and
  `ModifyInfo::popShortData`/`popLongData` — and so did the eleven
  `popCListElement()` copies in the tile, bomb and mine packets. All
  throw `InvalidProtocolException` on an empty list.
  > **Status:** fixed (wire/inventory-disagreements)

- **`Script::addContent` threw a raw string literal past
  `SCRIPT_MAX_CONTENTS`**, which no `__END_CATCH` can catch, so a script
  with a sixteenth content terminated the process instead of being
  reported. It throws `Error`.
  > **Status:** fixed (wire/inventory-disagreements)

## Movement, effect-lifecycle and NPC dialogue write/read disagreements (2026-09-10)

The ten findings task 1.2 stated as flip-tests in
`tests/packet_movement_test.cpp`, plus the three its PR recorded rather
than tested, and the blast-list bounds the combat round left open in the
bomb and mine packets. These packets are client-facing, so `write()` and
`getPacketSize()` are the contract: every fix is a refusal, a cap at the
width the length byte or the factory max already budgets, a
size-accounting correction or a read-side correction to what `write()`
emits. No golden changed; the seven maxima below are the set's only
`tests/wire-layout.txt` movements, and the five bomb and mine goldens are
new files.

- **`GCNPCAskDynamic::getPacketSize()` did not count the choice-count
  byte `write()` emits.** `writePacket()` puts that size on the wire
  before calling `write()`, so every question declared a body one byte
  shorter than the one that followed it and the stream never
  resynchronised. The size counts the byte now, and so does the factory
  maximum, which had left it out too.
  > **Status:** fixed (wire/movement-disagreements)

- **`GCNPCAskDynamic` counted its choices in a `BYTE` it incremented per
  entry and bounded the list against nothing.** The 256th choice wrapped
  the count to zero while `write()` still emitted every string. The count
  is the list now, bounded by `kMaxCount` — 15, refused in
  `addContent()`, in `write()` and in `read()`. The bound is the fill
  site's own: `ActionAskDynamic` adds one choice per `Script` content and
  `Script` holds at most `SCRIPT_MAX_CONTENTS` (15) of them, so the
  refusal is unreachable. The factory max budgeted ten, which the shipped
  data already passes — of 1155 `Script` rows two carry 11 and 12
  contents — so it budgets 15 now: `GCNPCAskDynamic` 11294 -> 16425, a
  read-buffer budget rather than a field on the wire.
  > **Status:** fixed (wire/movement-disagreements)

- **`GCNPCAskDynamic::write()` emitted a zero-length choice that `read()`
  counted and dropped.** The packet read back declared one more choice
  than it held, so forwarding it wrote one fewer field than its own count
  promised. `read()` keeps the empty choice, which is the side that
  matches `write()`: the field is a well-formed zero-length string, the
  size counts its length word, and `ScriptManager` trims each
  `**`-separated chunk into the script whatever it holds.
  > **Status:** fixed (wire/movement-disagreements)

- **`GCNPCSayDynamic` derived the length byte in front of its message
  from an unbounded string.** `ActionSayDynamic` sets the message to a
  script subject, and 208 of the 1155 shipped subjects are longer than
  255 bytes, so the length byte wrapped (846 becomes 78) and the client
  read a truncated line with the rest of the text left in its buffer. The
  message goes through `de::wire::readString`/`writeString` at the 255
  the byte can describe, on both sides. The seven live sends are
  `CGNPCTalkHandler`'s string-pool alerts, whose longest entry is 120
  bytes, so no live line is refused.
  > **Status:** fixed (wire/movement-disagreements)

- **`ScriptParameter::getSize()` measured its name and value through a
  `BYTE`.** `GCNPCAskVariable` therefore under-reported by 256 for every
  parameter longer than a byte can count, while `write()` emitted both
  strings whole. Both fields go through `de::wire::readString`/
  `writeString` at 255 and the size is `stringWireSize()` of each, so the
  declared size and the emitted bytes cannot drift again. The parameter
  count, a `BYTE` with the same wrap, is the map now, refused past the
  255 the factory max budgets in `addScriptParameter()`, in `write()` and
  by `read()`'s own count byte; a refused record is destroyed rather than
  leaked, as the duplicate-name refusal beside it now is too.
  > **Status:** fixed (wire/movement-disagreements)

- **`GCRemoveEffect` counted its effect list the same way, and
  `popFrontListElement()` left the count alone.** The count is the list
  now, refused past `kMaxCount` (255, what the count byte carries) in
  `addEffectList()`, in `write()` and in `read()`, `setListNum` is gone,
  and popping an id shortens what the packet declares. The factory max
  was a flat 255 bytes, which budgets 125 ids for a list the count byte
  lets reach 255; it is `szObjectID + szBYTE + 255 * szEffectID` now:
  `GCRemoveEffect` 255 -> 515.
  > **Status:** fixed (wire/movement-disagreements)

- **`GCRemoveEffect::read()` appended to the list the packet already
  held.** A packet read into twice declared one sweep's worth of ids and
  wrote two. It clears before it parses, the shape `ModifyInfo::read()`
  and the tile reads already have.
  > **Status:** fixed (wire/movement-disagreements)

- **`GCAddMonsterFromBurrowing` and `GCAddMonsterFromTransformation` did
  not hold the monster name to the 32 bytes their factory max budgets.**
  `GCAddMonster` carries the same record and refuses a longer name; these
  two let a long name outgrow the receiver's read buffer and a name past
  255 wrap the length byte in front of it. Both go through
  `de::wire::readString`/`writeString` at `kMaxNameSize`, the same 32.
  No maximum moves — it already budgeted 32.
  > **Status:** fixed (wire/movement-disagreements)

- **A direction outside the eight travelled unchecked in both
  directions.** `Dir_t` is a `BYTE` typedef, so this is a range gap
  rather than undefined behaviour, and `dir2String()` already prints an
  unknown value as its number. The two client requests refuse it:
  `CGUnburrow::read()` and `CGMove::read()` throw on a direction `>=
  DIR_MAX`. The server's own broadcasts keep it: `MonsterAI::moveNormal`
  computes `DIR_NONE` for a monster already standing on its destination
  and hands it to `Zone::moveCreature`, and `dirMoveMask` has a (0,0)
  entry at that index, so `GCMove`, `GCUnburrowOK` and `GCUntransformOK`
  carrying `DIR_NONE` is part of the contract and is pinned as one.
  > **Status:** fixed (wire/movement-disagreements)

- **Twenty-two of the twenty-six packets left at least one member
  uninitialised in the default constructor.** A packet sent without every
  setter called put whatever the allocation held on the wire. All of them
  initialise every member now — including `PCVampireInfo3`, the record
  the two vampire transition packets embed — pinned by constructing each
  over storage poisoned with two different bytes and requiring the same
  body.
  > **Status:** fixed (wire/movement-disagreements)

- **`~GCNPCInfo` cleared its record list without freeing what `read()`
  allocated into it.** The records a filler hands the packet belong to
  the `Zone` and the ones `read()` parses belong to nobody, so every
  packet read leaked one record per NPC in the zone. The packet tracks
  which it holds and destroys only its own; `read()` clears first, so a
  packet read into twice does not leak the first roster either.
  > **Status:** fixed (wire/movement-disagreements)

- **The four transition packets' `read()` allocated a fresh `EffectInfo`
  over the one the packet already held.** Each destructor frees the
  record, so reading into a packet that had one leaked it. All four free
  the held record before allocating.
  > **Status:** fixed (wire/movement-disagreements)

- **`GCThrowBombOK1`/`2`/`3` and `GCMineExplosionOK1`/`2` carried the
  tile packets' creature list with the tile packets' bugs.** The count
  wrapped at 256, `popCListElement()` left it alone, `setCListNum` let a
  caller declare a count the list did not hold, and every factory max
  budgeted one `ObjectID` for a list the count byte lets reach 255 — so a
  blast that caught more than one creature already wrote a body larger
  than the receiver's read buffer. All five take the `GCSkillToTileOK*`
  fix verbatim, and their maxima budget a full list: `GCThrowBombOK1`
  2055 -> 3071, `GCThrowBombOK2` 2058 -> 3074, `GCThrowBombOK3` 271 ->
  1287, `GCMineExplosionOK1` 2054 -> 3070, `GCMineExplosionOK2` 267 ->
  1283. None was pinned by a golden before; all five have one now.
  > **Status:** fixed (wire/movement-disagreements)

## Combat feedback write/read disagreements (2026-09-09)

The six findings task 1.2 stated as flip-tests in
`tests/packet_combat_test.cpp`, plus the two its review recorded rather
than tested, and three list bounds the social round left open. These
packets are client-facing, so `write()` and `getPacketSize()` are the
contract: every fix is a refusal, a cap at a width the factory max
budgets, a size-accounting correction or a read-side correction to what
`write()` emits. No golden changed; the five tile maxima below are the
set's only `tests/wire-layout.txt` movements.

- **`ModifyInfo` counted each of its two lists in a `BYTE` it incremented
  per entry.** Nothing capped either list, so the 256th entry wrapped the
  count to zero while `write()` still emitted every entry: the receiver
  stopped after the count byte and read the rest of the record as the next
  packet's header, and `getPacketSize()` counted the wrapped count too, so
  `writePacket()` put a length on the wire the body did not match either.
  Both counts are the list now (`ModifyInfo::kMaxCount`, the 255 the count
  byte carries and the maximum budgets), `addShortData`/`addLongData`
  refuse the entry past it and so does `write()`, and `popShortData` /
  `popLongData` no longer underflow a count of their own. Of the 485 fill
  sites in `src/server`, none adds in a loop: each adds one entry per
  changed field under its own `if`, so a record cannot legitimately hold
  more than the 73 `ModifyType` values and no refusal is reachable.
  > **Status:** fixed (wire/combat-disagreements)

- **The five tile packets that carry a creature list counted it the same
  way, and `popCListElement()` left the count alone.** `GCSkillToTileOK1`,
  `OK2`, `OK4`, `OK5` and `OK6` wrapped their `m_CListNum` at 256 exactly
  as `ModifyInfo` did, and a packet read, popped and forwarded declared
  more ids than it held. Each derives the count from the list now,
  `setCListNum` is gone, and `addCListElement` and `write()` refuse the id
  past `kMaxCount`. The bound is 255: the count travels in a byte, so
  nothing wider can be expressed, and no narrower bound is defensible —
  the widest mask a tile skill uses is `IceWave`'s 193 tiles, and a tile
  holds one creature per move mode, so a sweep can in principle reach 579.
  A crowded sweep therefore refuses the packet where it used to wrap the
  count; that is loud where the old behaviour desynchronised the stream.
  > **Status:** fixed (wire/combat-disagreements)

- **No tile packet's factory max budgeted its creature list.**
  `GCSkillToTileOK1`, `OK2` and `OK6` budgeted one `ObjectID` and `OK4`
  and `OK5` one plus a spare 255 bytes, for a list the count byte lets
  reach 255, so a sweep that caught more than one creature wrote a body
  larger than the read buffer the receiver sizes from that maximum. Each
  max multiplies `szObjectID` by its packet's `kMaxCount`, which moves five
  lines in `tests/wire-layout.txt`: `GCSkillToTileOK1` 2060 -> 3076,
  `GCSkillToTileOK2` 2061 -> 3077, `GCSkillToTileOK4` 270 -> 1286,
  `GCSkillToTileOK5` 274 -> 1290 and `GCSkillToTileOK6` 2059 -> 3075. This
  is the buffer the server budgets for a body it receives; the client sizes
  its own from its own copy of the max, and no byte any packet emits
  changes.
  > **Status:** fixed (wire/combat-disagreements)

- **`ModifyInfo::read()` and the tile `read()`s appended to the list the
  packet already held.** A reader is reused, so a packet read into twice
  declared one record's worth of entries and wrote two. All six clear
  before they parse, the shape `GMServerInfo::read` already has.
  > **Status:** fixed (wire/combat-disagreements)

- **The `ModifyType` tag travelled unchecked while `toString()` indexed
  `ModifyType2String` with it.** A tag past the last enumerator round
  tripped intact and the debug string then read past the end of a
  74-element table. `read()` checks the raw byte before it keeps the
  entry, the adders refuse the same value, and `modifyType2String()`
  prints an unknown tag as its number, the way `dir2String()` does.
  > **Status:** fixed (wire/combat-disagreements)

- **`GCSkillToTileOK3::getObjectID()` and
  `GCSkillToInventoryOK2::getObjectID()` were declared `CEffectID_t` over
  an `ObjectID_t` member.** Every caller saw the low half of the id the
  packet puts on the wire, so two creatures whose ids differ only above
  bit 16 were one creature to it. Both return `ObjectID_t`; no server
  source calls either accessor.
  > **Status:** fixed (wire/combat-disagreements)

- **Thirty-one of the thirty-three packets left a member uninitialised in
  the default constructor.** Only `GCStatusCurrentHP` and
  `GCModifyInformation` initialised everything they write, so a packet
  sent without every setter called put whatever the allocation held on the
  wire. All of them initialise every member now, pinned by constructing
  each over storage poisoned with two different bytes and requiring the
  same body.
  > **Status:** fixed (wire/combat-disagreements)

- **`GCAttackArmsOK1` and `GCAttackArmsOK5` read the hit flag into a
  `bool`.** Any byte other than 0 or 1 left an object no load may touch.
  Both read a `BYTE` and narrow it, so a non-zero byte is a hit. `write()`
  is untouched and the wire does not move.
  > **Status:** fixed (wire/combat-disagreements)

- **Three guild lists were capped nowhere.** `GCActiveGuildList` and
  `GCWaitGuildList` emitted every guild they held against a factory max
  that budgets 5000 (the count is a `WORD`, so nothing wrapped, but a
  larger table outgrew the read buffer), and `GCShowWaitGuildInfo`'s
  founding-member list did the same against its 5. All three refuse in
  `addGuildInfo`/`addMember`, in `write()` and in `read()`, the way
  `GCGuildMemberList` does, and destroy a refused owned record. Neither
  refusal is reachable: `GuildManager::makeActiveGuildList` /
  `makeWaitGuildList` walk the guild table, which would need 5001 guilds
  of one race in one state, and a guild leaves `GUILD_STATE_WAIT` for
  `GUILD_STATE_ACTIVE` the moment it has five members
  (`GSAddGuildMemberHandler`), so `CGSelectGuildHandler` can never hand
  `GCShowWaitGuildInfo` a sixth name.
  > **Status:** fixed (wire/combat-disagreements)

- **The last raw direction and sex indexers.** `Dir2String[m_Dir]` in
  `CGMove`, `CGUnburrow`, `GCMove`, `GCMoveOK`, `GCUnburrowOK`,
  `GCUntransformOK`, `PCSlayerInfo3`, `PCOustersInfo3`, `MonsterCorpse`,
  `ActionSetPosition`, `EffectBloodySnake` and `VisionInfo`, and
  `Sex2String[m_Sex]` in `PCSlayerInfo3`, `PCVampireInfo3` and
  `PCOustersInfo3`, indexed an eight- and a two-element table with a value
  that arrives off the wire. All go through `dir2String()` and a new
  `sex2String()` beside it, which print the number instead. Debug strings
  only.
  > **Status:** fixed (wire/combat-disagreements)

## Social protocol write/read disagreements (2026-09-09)

The thirteen findings task 1.2 stated as flip-tests in
`tests/packet_party_test.cpp`, `tests/packet_guild_test.cpp` and
`tests/packet_trade_test.cpp`, plus the two readers its review reported
could not be round-tripped. These packets are client-facing, so `write()`
and `getPacketSize()` are the contract: every fix is a refusal, a cap at
a width the factory max already budgets, a size-accounting correction or
a read-side correction to what `write()` emits. No golden changed; the
guild roster and union offer maxima below are the set's only
`tests/wire-layout.txt` movements.

- **`GuildInfo::getSize()` omitted the expiry date.** `write()` emits a
  length byte and then the date; `getSize()` counted neither, so
  `GCActiveGuildList` declared a body a byte plus the date shorter than
  the one it sends, for every guild in the table, and `writePacket()`
  put that short length on the wire ahead of the longer body.
  `getMaxSize()` already budgets `szBYTE + 11` for the date, so only the
  size was wrong and no maximum moves.
  > **Status:** fixed (wire/social-disagreements)

- **Four client-facing lists arrived reversed.**
  `GCActiveGuildList::read()`, `GCGuildMemberList::read()`,
  `GCWaitGuildList::read()` and `GCShowWaitGuildInfo::read()` pushed each
  record they parsed to the front of a list their `write()` emits front
  to back. All four push to the back now. The two guild tables are drawn
  as a list the player picks from, the roster is drawn the same way, and
  `GCShowWaitGuildInfo`'s founding members are a name list — order is
  what the player sees. Read side only; the wire does not move.
  > **Status:** fixed (wire/social-disagreements)

- **The guild-table count was narrowed to a `BYTE` by its accessor.**
  `GCActiveGuildList::getListNum()` and `GCWaitGuildList::getListNum()`
  returned a `BYTE` of a list whose count goes on the wire as a `WORD`
  and whose factory max budgets 5000 guilds, so a caller asking how many
  guilds the packet holds was told zero at 256. Both return the `WORD`.
  > **Status:** fixed (wire/social-disagreements)

- **Four lists were capped nowhere.** `GCGuildMemberList`,
  `GCUnionOfferList`, `GCPartyJoined` and `GCTradeAddItem`'s option and
  sub-item lists each emitted every element they held against a factory
  max that budgets a fixed number, so the body outgrew the read buffer
  the receiver sizes from that max. Each refuses the entry past the
  budget in `addListElement`, in `write()` and in `read()`, and destroys
  a refused owned record, the way `LCWorldList` does with
  `WorldInfo::kMaxCount`. Two of the maxima were wrong as well:
  `GuildMemberInfo::getMaxSize()` was a whole 220-member table that
  counted one `ServerID` for all of them and left out the roster's own
  count byte, and `GCUnionOfferList`'s max was exactly twenty offers with
  no room for the count byte in front of them. The record maximum is one
  record now and each packet multiplies by its own `kMaxCount`, which
  moves two lines in `tests/wire-layout.txt`: `GCGuildMemberList`
  5064 → 5502 and `GCUnionOfferList` 1180 → 1181. Both are read buffers
  the server budgets for a body it receives, not fields on the wire. Of
  the fill sites, two can reach a refusal: `Guild::makeMemberInfo()`
  walks the guild's member map and nothing limits a guild to 220 members,
  and `GuildUnionOfferManager::makeOfferList()` reads its offers from the
  database, so a union with more than twenty outstanding offers throws
  out of the list request. The party roster cannot —
  `PARTY_MAX_SIZE` is the same six — and the belt and armsband pocket
  counts in the item data stop at the eight sub-items budgeted.
  > **Status:** fixed (wire/social-disagreements)

- **`GCTradeAddItem`'s sub-item count was set independently of the
  records written.** `setListNum()` wrote a member that
  `addListElement()` left alone; `write()` put that member on the wire
  and then emitted every record the list held, and `getPacketSize()`
  sized the records from the member. A packet built without a matching
  `setListNum()` declared one length, announced a second count and
  emitted a third number of records, and the receiver read the rest of
  the body as the next packet's header. The count is the list now and
  `setListNum` is gone, the shape `InventoryInfo`, `GearInfo`,
  `ExtraInfo` and `RideMotorcycleInfo` already have. Both callers — the
  belt and the armsband branches of
  `CGTradeAddItemHandler::makeGCTradeAddItemPacket()` — set exactly the
  number of records they had just added, so no count on the wire changes.
  > **Status:** fixed (wire/social-disagreements)

- **The protocol's strings were bounded on neither side.** The guild and
  member introductions in `CGJoinGuild`, `CGModifyGuildIntro`,
  `CGModifyGuildMemberIntro`, `CGRegistGuild`, `GCShowGuildInfo`,
  `GCShowGuildMemberInfo` and `GCShowWaitGuildInfo` each derived a `BYTE`
  length from the string and emitted the string whole; the guards that
  looked like a cap compared a `BYTE` against 255, which no `BYTE` can
  exceed. They are cut to `GUILD_INTRO_MAX_LENGTH` in the setter and
  refused past it in `write()` — the same width and the same shape as
  the inter-server introductions, since every one of these budgets 255
  or 256 for it. The party protocol's names and chat messages did the
  same against budgets of 10, 20 and 128: the names are refused past
  their width, and when empty where the packet needs one, while the
  messages are cut to 128 in the setter and refused when empty, the way
  `GCGuildChat` already refuses its own. `GCGuildChat`'s sending guild
  name is refused empty or past `GUILD_NAME_MAX_LENGTH`, next to the
  sender and the message it already bounded. A valid value is untouched,
  so the wire does not move.
  > **Status:** fixed (wire/social-disagreements)

- **Two result-code accessors halved a `WORD`.**
  `GCGuildResponse::getCode()` and `GCNPCResponse::getCode()` returned a
  `BYTE` of a member and a wire field that are both `WORD`, so a code
  past 255 reached the wire whole and the accessor whole halved.
  `GCNPCResponse` already declares 139 dialogue codes. Both return the
  `WORD`; no server source calls either accessor, so nothing else moves.
  > **Status:** fixed (wire/social-disagreements)

- **Two readers tested an uninitialised length byte.**
  `GCModifyGuildMemberInfo::read()` and `GCOtherGuildName::read()`
  declared a `BYTE` for the guild-name length, tested it against 30 and
  against 0, and only then read the length the stream carries. So the
  read consumed a length that was never checked when the indeterminate
  value happened to be non-zero, and read no length byte at all when it
  happened to be zero, leaving that byte in the stream and taking the
  rank from it. Both read the length first and check it after, so both
  packets round trip.
  > **Status:** fixed (wire/social-disagreements)

- **Five more packets read a leading flag byte `write()` does not
  emit.** `GCCreatureDied`, `GCDropItemToZone`, `GCMove`, `GCNPCSay` and
  `GCSkillFailed2` each carry a commented-out `oStream.write((BYTE)48)`
  and a `read()` that consumes the byte it would have produced — the
  shape `GCAddEffect` and `GCAddMonsterFromBurrowing` had. The reader
  took the first byte of the object id as a flag and shifted every field
  after it. All five start at the first field `write()` emits, and the
  commented-out writes are gone. `GCDropItemToZone` round trips through
  `tests/packet_encrypter_test.cpp` at every encrypt code now instead of
  being pinned write-side only; its goldens are unchanged.
  > **Status:** fixed (wire/social-disagreements)

- **`toString()` indexed `Dir2String[8]` unchecked.** `GCAddMonster`,
  `GCAddMonsterCorpse`, `GCAddMonsterFromBurrowing`,
  `GCAddMonsterFromTransformation` and `PCVampireInfo3` print a
  direction that arrives off the wire, so a value outside the eight read
  past the end of the array. All five go through `dir2String()`, which
  prints the number instead. Debug strings only; nothing on the wire
  moves.
  > **Status:** fixed (wire/social-disagreements)

## Inter-server link write/read disagreements (2026-09-09)

The findings task 1.2 stated as flip-tests in
`tests/packet_interserver_test.cpp`, plus the one its review reported
that no test stated usefully, and the leftovers earlier rounds reported
in the packets they did not cover. Each is now pinned as the behaviour
it produces. No golden changed; the `GuildInfo2` maximum below is the
set's only `tests/wire-layout.txt` movement.

- **Two chat packets guarded the message length with the sender
  length.** `GGGuildChat::read()` and `GGServerChat::read()` both test
  `szSender > 128` where they mean the message. The sender is capped at
  10 two lines above, so the guard can never fire and a declared message
  length of up to 255 was accepted — while `write()` refuses anything
  past 128 and the receiver sizes its buffer from a factory max that
  budgets 128. Both guards name the message now.
  > **Status:** fixed (wire/interserver-disagreements)

- **The guild introduction was bounded nowhere.** `GSAddGuild`,
  `GSModifyGuildIntro`, `SGAddGuildOK` and `SGModifyGuildIntroOK`, and
  the `GuildInfo2` record nested in `SGGuildInfo`, each derived a `BYTE`
  length from the string and then emitted the string whole. Past 255
  characters the length byte wrapped and the reader stopped mid-text;
  below that the body simply outgrew the read buffer the receiver sizes
  from the factory max. The guards that looked like caps
  (`szGuildIntro > 255` in the two `ModifyGuildIntro` packets,
  `szIntro > 256` in `GuildInfo2`) compared a `BYTE` against a value no
  `BYTE` can hold. `GUILD_INTRO_MAX_LENGTH` (255, the width the length
  byte can express and the smallest of the five budgets) is now cut to
  in every setter and refused past in every `write()`; the read side
  needs no guard, because the one-byte length is the bound. The
  introduction is player-typed and arrives inside the same one-byte
  frame in `CGRegistGuild` and `CGModifyGuildIntro`, so the game server
  never has to refuse a value the client already limited, and the shared
  server's `Guild.Intro` column — a MySQL `text` — is cut on the way
  into the record.
  > **Status:** fixed (wire/interserver-disagreements)

- **`SGGuildInfo` and `GuildInfo2` read their lists back reversed.**
  Both `read()`s push each record they parse to the front of a list
  their `write()` emits front to back, so the guild table and every
  guild's member list arrived in the opposite order. Both push to the
  back now. Nothing depended on the reversal: `SGGuildInfoHandler`
  inserts each guild into `GuildManager`'s id-keyed map and each member
  into `Guild`'s name-keyed map, and the shared server fills the packet
  by walking its own hash map, whose order is arbitrary to begin with.
  > **Status:** fixed (wire/interserver-disagreements)

- **`SGGuildInfo`'s entry count was capped on neither side.**
  `SGGuildInfoFactory::kMaxSize` is 500 guilds' worth of `GuildInfo2`
  and that number is the receiver's read buffer, but neither `write()`
  nor `read()` held the `WORD` count to it, so a table of 501 full-sized
  guilds was emitted and parsed rather than refused. `addGuildInfo()`
  now refuses the entry past `GuildInfo2::kMaxCount` and destroys it,
  the way `LCWorldList::addListElement` does with `WorldInfo::kMaxCount`;
  `write()` refuses a list that reached that length another way, and
  `read()` refuses a declared count past it before allocating a record.
  The shared server fills the packet in
  `GuildManager::makeSGGuildInfo()`, under its own mutex, from
  `GSRequestGuildInfoHandler`: a guild table past 500 makes
  `addGuildInfo()` throw, the scoped critical section releases the
  mutex, and `GameServerManager::processCommands()` catches the
  `ProtocolException` and drops that one game server's connection. The
  shared server stays up; the game server reconnects and asks again.
  > **Status:** fixed (wire/interserver-disagreements)

- **`GuildInfo2::getMaxSize()` counted the member-count word twice.** It
  added `szWORD` once for the count `write()` emits and once more at the
  end, so every guild in `SGGuildInfo`'s budget was two bytes too large
  and the packet's factory max was 1000 bytes above what 500 full guilds
  can occupy. It over-budgeted, so nothing truncated, but the number was
  not the record's size. One line moves in `tests/wire-layout.txt`:
  `SGGuildInfo` 2916502 → 2915502. This is the read buffer the game
  server budgets for a body it receives, not a field on the wire.
  > **Status:** fixed (wire/interserver-disagreements)

- **`GMServerInfo::read()` appended to the zone table it already
  held.** It read the count into the member that `getPacketSize()` and
  `write()` use and then pushed the new rows onto the existing list, so
  a packet read into twice reported fewer zones than it carried. The
  body carries the whole table, so `read()` clears the list first. The
  receive path builds a fresh packet each time, so nothing reached that
  state.
  > **Status:** fixed (wire/interserver-disagreements)

- **Four transition packets dereferenced a missing effect record.**
  `GCAddMonsterFromBurrowing`, `GCAddMonsterFromTransformation`,
  `GCAddVampireFromBurrowing` and `GCAddVampireFromTransformation` read
  `m_pEffectInfo` unconditionally in `getPacketSize()`, `write()` and
  `toString()`, so a default-constructed instance — the one every
  factory creates for a reader — crashed before it could read anything.
  All four fall back to an empty list, as `GCAddSlayer`, `GCAddVampire`,
  `GCAddOusters` and `GCAddMonster` already do.
  > **Status:** fixed (wire/interserver-disagreements)

- **`GCAddMonsterFromBurrowing::read()` consumed a byte `write()` never
  emits.** The same shape `GCAddEffect` had: the
  `oStream.write((BYTE)48)` that would produce the leading flag is
  commented out and `getPacketSize()` never counted it, so the reader
  took the first byte of the object id as a flag and shifted every field
  after it. `read()` starts at the object id now and the commented-out
  write is gone. No byte moves: `write()` never emitted the flag. Five
  more packets carry the same commented-out write and the same leading
  read — `GCCreatureDied`, `GCDropItemToZone`, `GCMove`, `GCNPCSay` and
  `GCSkillFailed2` — and are left alone here; `GCDropItemToZone` is
  already an open finding under task 1.2.
  > **Status:** fixed (wire/interserver-disagreements)

- **Four more records swallowed the exceptions their bodies raised.**
  `RideMotorcycleSlotInfo`, `SubOustersSkillInfo`, `SubSlayerSkillInfo`
  and `SubVampireSkillInfo` each wrapped the whole of `read()` and
  `write()` in `try { … } catch (Throwable& t) { cout … }`, the shape
  `PCSlayerInfo`, `PCSlayerInfo2`, `PCSlayerInfo3` and `SubItemInfo`
  had. A stream that stops short leaves the record half-parsed and the
  caller reading its next field from the wrong offset, so the failure
  has to reach it. The exceptions now leave all eight functions, and no
  `read()` or `write()` in `src/Core` catches `Throwable` any more.
  > **Status:** fixed (wire/interserver-disagreements)

## Zone population scan write/read disagreements (2026-09-09)

The findings task 1.2 stated as flip-tests in
`tests/packet_zone_scan_test.cpp`, plus the ones its review reported
that no test could state while they were open. Each is now pinned as
the behaviour it produces. No valid packet's bytes moved and no golden
changed; the alignment field below is the set's only
`tests/wire-layout.txt` movement.

- **`PCSlayerInfo3::read`/`write` swallowed the exceptions they
  raised.** Both wrapped their whole body in
  `try { … } catch (Throwable& t) { cout … }`, the third copy of the
  shape `PCSlayerInfo` and `PCSlayerInfo2` had. `write()` refuses an
  empty name and a name past 20; the object id was already on the wire
  when the throw happened, so the PC record stopped after four bytes
  while `GCAddSlayer::getPacketSize()` — which `writePacket()` puts on
  the wire ahead of the body — still counted the whole record, and the
  client misframed every packet after it. `PCVampireInfo3` and
  `PCOustersInfo3` never had the wrapper. The exceptions now leave both
  functions.
  > **Status:** fixed (wire/zone-scan-disagreements)

- **`GCAddEffect::read()` consumed a byte `write()` never emits.** The
  `oStream.write((BYTE)48)` that produced it is commented out and
  `getPacketSize()` never counted it, so the reader took the first byte
  of the object id as a flag and shifted every field after it. `read()`
  now starts at the object id, and the commented-out write is gone.
  > **Status:** fixed (wire/zone-scan-disagreements)

- **Two record maxima omitted the alignment field their size counts.**
  `PCVampireInfo3::getMaxSize()` and `PCOustersInfo3::getMaxSize()` left
  out the `szAlignment` that `getSize()` puts on the wire. `Alignment_t`
  is an int, so every carrier of either record budgeted four bytes too
  few, and a Vampire or Ousters name of 17 characters or more already
  declared a body larger than the read buffer the receiver sizes from
  the factory max. Seven factory maxima grow by four bytes in
  `tests/wire-layout.txt` (`GCAddOusters` 1228 → 1232,
  `GCAddOustersCorpse` 54 → 58, `GCAddVampire` 1230 → 1234,
  `GCAddVampireCorpse` 55 → 59, `GCAddVampireFromBurrowing`
  1075 → 1079, `GCAddVampireFromTransformation` 1075 → 1079,
  `GCMorphVampire2` 54 → 58). This is the read buffer the server
  budgets; the client sizes its own from its own copy of the max, and no
  byte any packet emits changes.
  > **Status:** fixed (wire/zone-scan-disagreements)

- **`PCVampireInfo3` narrowed its coat type to a byte in silence.** The
  member is an `ItemType_t`, a WORD, while `write()` cast it to a BYTE
  and `getSize()` budgeted one byte for it, so a value above 255 lost
  its high half and dressed the character in a different coat. The wire
  byte is the client's contract and stays a byte; the domain fits it
  (`VampireCoatInfo.ItemType` is a `tinyint unsigned`, and the gameserver
  only ever assigns `pItem->getItemType()` of a worn `VampireCoat`), so
  `write()` refuses a wider value instead of truncating it.
  > **Status:** fixed (wire/zone-scan-disagreements)

- **Five strings were bounded on neither side.** `GCAddMonster` and
  `GCAddMonsterCorpse` derived a BYTE length from the monster name and
  then wrote the whole string, against factory maxima that budget 32 and
  128 characters; `StoreOutlook`'s shop sign, `PetInfo`'s pet nickname
  and `GCAddVampirePortal`'s owner name did the same against
  `MAX_SIGN_SIZE`, 22 and 20; `NPCInfo::setName` took any length against
  the 30 its own max budgets. Past 255 the length byte wrapped and the
  reader stopped mid-string; below that the body simply outgrew the read
  buffer the receiver sizes from the max. All five are refused past the
  cap in `write()` and `read()`. The two a player types — the shop sign
  and the pet nickname — are also cut to the cap in their setter, the
  way the character nickname is; the monster and NPC names come from
  game data and the portal owner is a character name, so those three are
  refused rather than quietly shortened. A valid value is untouched.
  > **Status:** fixed (wire/zone-scan-disagreements)

- **`GCNPCInfo` wrote a list its count byte could not describe.**
  `write()` puts the record count in a BYTE while `getPacketSize()`
  counted the whole list and the factory max budgets 255 records — the
  shape `GCUpdateInfo::addNPCInfo` already refuses. `addNPCInfo` now
  refuses the record past the count byte; the zone owns the records
  either way, so a refused one is simply not listed.
  > **Status:** fixed (wire/zone-scan-disagreements)

- **The corpse packets left the treasure count indeterminate.**
  `GCAddSlayerCorpse`, `GCAddVampireCorpse` and `GCAddOustersCorpse`
  zeroed it in the default constructor and not in the info-taking one,
  so a corpse built from a PC record put whatever the allocation held on
  the wire. All three initialise it.
  > **Status:** fixed (wire/zone-scan-disagreements)

- **`GCAddOusters` freed records the creature owns.** Its destructor
  deleted the effect, the pet and the nickname records, while
  `GCAddSlayer` and `GCAddVampire` deleted only the effect record.
  `makeGCAddOusters` installs `pOusters->getPetInfo()` and
  `pOusters->getNickname()`, which are the creature's own members, so
  every Ousters that entered someone's view left the creature holding
  two dangling pointers. Only the effect record is packet-owned — both
  `read()` and `PacketUtil` hand over a freshly allocated one — and all
  three destructors now free that and nothing else.
  > **Status:** fixed (wire/zone-scan-disagreements)

- **A missing effect record crashed the size and the write.**
  `GCAddSlayer`, `GCAddVampire`, `GCAddOusters` and `GCAddMonster`
  dereferenced `m_pEffectInfo` unconditionally in `getPacketSize()`,
  `write()` and `toString()`, so a default-constructed instance — the
  one every factory creates for a reader — crashed before it could read
  anything. All four fall back to an empty list, the way `GCUpdateInfo`
  falls back to an empty nickname and blood bible sign.
  > **Status:** fixed (wire/zone-scan-disagreements)

- **`GCAddSlayer::write()` and `GCAddVampire::write()` shared a mutable
  static.** The null-pet branch wrote a function-local
  `static PetInfo NullPetInfo` from a `const` member function, so every
  zone thread emitting a pet-less character touched the same object.
  Each `write()` now builds its own, as `GCAddOusters::write()` already
  did. The bytes are the empty pet record either way.
  > **Status:** fixed (wire/zone-scan-disagreements)

## Gameserver handshake write/read disagreements (2026-09-08)

The findings task 1.2 stated as flip-tests in
`tests/packet_gameserver_handshake_test.cpp`, plus the ones its review
reported that no test could state while they were open. Each is now
pinned as the behaviour it produces. No valid packet's bytes moved and
no golden changed; the effect-list max below is the set's only
`tests/wire-layout.txt` movement.

- **`PCSlayerInfo2` and `SubItemInfo` swallowed the exceptions their
  `read`/`write` raised.** Both wrapped their whole body in
  `try { ... } catch (Throwable& t) { cout ... }`, the shape
  `PCSlayerInfo` had on the login side. `PCSlayerInfo2::write()` raises
  on an empty PC name and on a guild name past 30; the object id was
  already on the wire when the throw happened, so the record stopped
  after four bytes while `GCUpdateInfo::getPacketSize()` — which
  `writePacket()` puts on the wire ahead of the body — still counted the
  whole record, and every packet after it was misframed.
  `PCVampireInfo2` and `PCOustersInfo2` never had the wrapper. The
  exceptions now leave both functions in both records.
  > **Status:** fixed (wire/update-info-disagreements)

- **`setGuildName` did not truncate.** All three `PCInfo2` records
  budget 30 in `getMaxSize()` and refuse more in `write()`, but the
  setter took any length, so a guild renamed past the cap made every
  `GCUpdateInfo` carrying one of its members unsendable. The setter now
  truncates to 30, the way the name setters in the same family do.
  > **Status:** fixed (wire/update-info-disagreements)

- **`NPCInfo::getSize()` counted fields `write()` omits.** `write()`
  emits the id and the two coordinates only behind a non-empty name;
  `getSize()` always added them, so one nameless NPC in a zone made
  `GCUpdateInfo` declare six bytes it never sent. `getSize()` now stops
  at the length byte for a nameless record, which is what `read()`
  expects.
  > **Status:** fixed (wire/update-info-disagreements)

- **`EffectInfo::getMaxSize()` was a flat 255.** `getSize()` is
  `szBYTE + 4 * ListNum` and `ListNum` is a BYTE, so a character
  carrying the maximum 255 effects needs 1021 bytes. The shortfall was
  budgeted into the factory max of every packet that embeds an effect
  list, which is what the receiver sizes its read buffer from. The max
  is now the full list, so nine factory maxima grow by 766 bytes in
  `tests/wire-layout.txt` (`GCAddMonster`,
  `GCAddMonsterFromBurrowing`, `GCAddMonsterFromTransformation`,
  `GCAddOusters`, `GCAddSlayer`, `GCAddVampire`,
  `GCAddVampireFromBurrowing`, `GCAddVampireFromTransformation`,
  `GCUpdateInfo`). Bytes on the wire are unchanged; this is the read
  buffer the server budgets, and the client sizes its own from its own
  copy of the max.
  > **Status:** fixed (wire/update-info-disagreements)

- **A settable `m_ListNum` that `addListElement` did not maintain.**
  `InventoryInfo`, `GearInfo`, `ExtraInfo` and `RideMotorcycleInfo` put
  `m_ListNum` on the wire and then wrote every element they held, so a
  count that disagreed with the list left the reader parsing fewer
  records than the body carried — and `getSize()`, which counts the
  list rather than the field, still matched the byte count, so the
  packet size looked correct all the way to the receiver. All four now
  maintain the count in `addListElement()` like `PCItemInfo` and
  `EffectInfo`, and `setListNum` is gone; every caller set it to the
  number it had just added, so no count on the wire changes.
  > **Status:** fixed (wire/update-info-disagreements)

- **`GCUpdateInfo` left most of its members indeterminate.** The
  constructor initialised ten of them. Among the rest was
  `m_pBloodBibleSign`, which `read()` wrote through without allocating —
  a write through an uninitialised pointer on every received packet.
  Every member is initialised now, `read()` allocates the sign record
  the way it allocates the other sub-records, and `write()` /
  `getPacketSize()` fall back to an empty record when none is installed,
  the way they already did for the nickname.
  > **Status:** fixed (wire/update-info-disagreements)

- **`NicknameInfo` had no constructor.** The NULL-nickname branch of
  `GCUpdateInfo::write()` builds one on the stack and emits it, so two
  indeterminate bytes of nickname id went on the wire for every player
  without a nickname record. The fields are zeroed now, and
  `setNickname` truncates to the 22 the max size budgets.
  > **Status:** fixed (wire/update-info-disagreements)

- **`GCPetInfo::read()` never restored the summon flag.** `write()`
  copies the packet's flag into the `PetInfo` it emits, so the wire
  carries it; `read()` left the packet's own copy at whatever the
  allocation held. The constructor zeroes it and `read()` takes it back
  from the `PetInfo` it read.
  > **Status:** fixed (wire/update-info-disagreements)

- **`CGSetVampireHotKey::toString()` read one past its array.** The
  eight hot keys live at indices 0..7; the debug string printed indices
  1..8, so it never showed the first key and read one element past the
  end. It now walks the eight it owns.
  > **Status:** fixed (wire/update-info-disagreements)

- **Three lists were unbounded against the widths their max sizes
  budget.** `GCUpdateInfo` writes its NPC record count as a BYTE while
  `getPacketSize()` counted the whole list, and its factory max budgets
  255 records; `BloodBibleSignInfo` budgets six signs and capped
  neither `write()` nor `getSize()`, so a seventh both wrapped the
  count byte and outgrew the max; the nickname was bounded on neither
  side. `addNPCInfo` now refuses the record past the count byte,
  `BloodBibleSignInfo` writes and counts at most the budgeted slots,
  and the nickname is capped in the setter and refused on read.
  > **Status:** fixed (wire/update-info-disagreements)

## Login-phase framing disagreements the CL/LC goldens found (2026-09-08)

The three write/read disagreements task 1.2 stated as flip-tests in
`tests/packet_login_test.cpp`. Each is now pinned as the refusal it
produces; no valid packet's bytes moved and no golden changed.

- **`PCSlayerInfo::read`/`write` swallowed the exceptions they raised.**
  Both wrapped their whole body in `try { … } catch (Throwable& t) { cout … }`,
  so the empty-name refusal that `PCVampireInfo` and `PCOustersInfo` let
  escape was printed and discarded here. `write()` carried on and emitted
  only the trailing advancement level while `getPacketSize()` still
  counted the full record — and `writePacket()` had already put that
  count on the wire, so the client read a body dozens of bytes short and
  every packet after it in the stream was misframed. The exception now
  leaves both functions.
  > **Status:** fixed (wire/login-packet-disagreements)

- **`LCRegisterPlayerOK`'s group name was bounded on neither side.** The
  setter took any length, `write()` narrowed it to a BYTE prefix with no
  check and `read()` accepted whatever length byte arrived, so an empty
  name wrote a zero prefix and a 256-byte name wrapped its prefix to
  zero — both undeliverable, because the stream's own string read rejects
  a zero length. The setter now truncates to `maxNameLength`, matching
  the width the factory max already budgeted, and both `read()` and
  `write()` refuse an empty or over-long name.
  > **Status:** fixed (wire/login-packet-disagreements)

- **`LCWorldList` / `LCServerList` accepted more entries than the
  factory max budgets.** Both budget 37 records of a 20-character name,
  and nothing capped the list at fill time, so the 38th world or server
  group in the login server's configuration made `getPacketSize()` exceed
  the max the client sizes its read buffer from — a truncated packet, not
  a caught error. `addListElement` now refuses the entry past the budget,
  and `WorldInfo` / `ServerGroupInfo` truncate their name to the width
  the budget allows, so the declared size can no longer outgrow the max.
  > **Status:** fixed (wire/login-packet-disagreements)

## Critical sections leaked their lock on non-Throwable exits (2026-09-05)

Found while making `__ENTER_CRITICAL_SECTION`/`__LEAVE_CRITICAL_SECTION` RAII.
The pair expanded to a bare `mutex.lock()` and a
`catch (Throwable&) { mutex.unlock(); throw; }` tail, so the lock survived any
exit the tail did not cover. Three defect shapes, all in the 463 sections under
`src/`:

- **`return` inside a section with no hand-written unlock.** Three live sites:
  `PCFinder::addNPC` (duplicate NPC name) and `PCFinder::deleteNPC` (unknown
  name) return holding `PCFinder::m_Mutex`, the lock every cross-thread SG/LG/GG
  handler takes through `getCreature_LOCKED`; `MPlayerManager::processResult`
  returns holding it from the `default:` branch of its error-code switch, on the
  mofus thread. `Mutex::lock()` detects a same-thread relock and throws `Error`,
  so the first symptom is a `SELF DEAD LOCK` line in `MutexError.log`; another
  thread blocks instead. The other 107 early returns unlocked by hand.
- **Non-`Throwable` exceptions.** `std::bad_alloc`, `std::out_of_range` and the
  `const char*` that `END_DB` rethrows do not match `catch (Throwable&)`, so any
  of them crossing a section boundary left the mutex held. The `const char*`
  case was already recorded in a comment in `SGDeleteGuildOKHandler.cpp`, which
  said the general fix belonged in `__LEAVE_CRITICAL_SECTION`. (`END_DB` throwing
  `msg.c_str()` from a local `string` — a dangling pointer — is a separate
  defect and is still open.)
- **Double unlock.** `Guild::addMember` and its sharedserver twin do
  `m_Mutex.unlock(); throw DuplicatedException();` inside a section; the
  `Throwable` tail then unlocked the same non-recursive pthread mutex a second
  time, which is undefined behaviour.

The section is now a block guarded by a scoped `CriticalSection` object, so the
lock is released on every exit — end of block, `return`, `goto`/`continue` out
of it, and any thrown type. The 120 hand-written unlocks inside sections were
removed (114) or routed through the guard (6, where work deliberately ran
unlocked before the return). `tests/critical_section_tests` pins the exits and
`tests/tools/critical_section_audit.pl` fails the class on sight.
> **Status:** fixed (cpp20/raii-critical-sections)

## loginserver and sharedserver could not be shut down (2026-09-05)

Found while extending PR #89's cooperative lifecycle to the other two server
processes. Neither `main()` installed a signal handler, so SIGTERM took the
default disposition and killed the process outright — no worker join, no
socket drain, no chance for the loop to finish its turn. There was no
graceful path either: `LoginServer::stop()` and `SharedServer::stop()` both
began with `throw UnsupportedError()` (the sub-manager stops after it were
dead code), `ClientManager::stop()`/`HeartbeatManager::stop()` threw the
same, and the `GameServerManager` workers derived from the legacy pthread
`Thread`, whose `stop()` also throws — their `while (true)` loops had no exit
at all. The loginserver's UDP listener compounded it: the socket was left
blocking, so the worker sat inside `recvfrom` for as long as the game servers
stayed quiet. The container supervisor's unbounded final `wait` matched the
old behaviour (login/shared died instantly), so it too had to be bounded once
they started draining. Every worker now uses `ManagedThread`, both mains
install the SIGTERM/SIGINT handler and the named 30-second failed-exit
watchdog, `stop()` is idempotent and joins before dependencies are released,
and `docker/start.sh` gives login/shared 8 seconds before SIGKILL.
> **Status:** fixed (cpp20/login-shared-managed-threads)

## sharedserver keep-alive query runs every tick, not hourly (2026-09-05)

Found while migrating `sharedserver/GameServerManager::run()`. Its
connection keep-alive reschedules with `dummyQueryTime.tv_sec = (60 + rand()
% 30) * 60;` — a plain assignment where the gameserver's `LoginServerManager`,
`SharedServerManager`, `SMSServiceThread` and the loginserver's
`ClientManager` all use `+=`. The absolute deadline therefore lands about an
hour after the epoch, is always in the past, and `executeDummyQuery()` fires
on every pass of a loop that now turns roughly every millisecond. Left alone
here deliberately: the lifecycle migration changed no thread's actual work.
The deadline now advances from itself there as it does at the other eight
keep-alive sites -- the gameserver's `ClientManager`, `GDRLairManager`,
`LoginServerManager`, `MPlayerManager`, `SharedServerManager`,
`SMSServiceThread` and `ZoneGroupThread`, and the loginserver's
`ClientManager` -- and all nine compute it through one function,
`de::nextKeepAliveDeadline` (`src/server/KeepAlive.h`), an hour plus up to
29 minutes past the previous deadline. `tests/keep_alive_test.cpp` pins the
bounds and that the deadline moves from itself, not from the epoch.
> **Status:** fixed (fix/manager-residue)

## Self-initialised pointer in sharedserver processOutputs() (2026-09-05)

`GameServerManager::processOutputs()` writes `GameServerPlayer*
pGameServerPlayer = pGameServerPlayer;` inside the socket-error branch,
shadowing the live outer pointer with one initialised from itself, and then
`delete`s it. Clang reports it (`-Wuninitialized`) on every build. The same
file never initialises `m_pGameServerPlayers[nMaxGameServers]`, so its
unwritten slots are indeterminate rather than NULL — the `m_MinFD`/`m_MaxFD`
window is what keeps the loops off them today.
> **Status:** fixed (refactor/shared-twin-classes) — the shadowing
> declaration is gone, so the branch deletes the player the outer pointer
> names, the shape the ConnectException branch below it already had, and
> `m_pGameServerPlayers` is default-initialised, so a slot no connection
> filled reads as NULL instead of as an indeterminate pointer. The table
> is still indexed by raw descriptor and sized 100, and a descriptor above
> that overruns it, which this entry does not close.

## Cooperative lifecycle and process shutdown gaps (2026-09-05)

Adversarial review of PR #89 found unsynchronized start/stop publication,
pool startup leaving its mutex locked after thread-construction failures,
uncaught worker exceptions, and a container SIGTERM path that bypassed joins.
The gameserver auxiliary workers could also outlive zone/database dependencies.
`ManagedThread` now serializes lifecycle operations, retains worker failures,
and requests process shutdown. The pool uses RAII and rolls back failed starts.
All gameserver workers are stopped/joined before dependencies may be released;
SIGTERM/SIGINT drain the main loop, and the supervisor keeps peer services alive
until gameserver finishes. Idle UDP reception is nonblocking. MySQL options
bound individual network operations; a 30-second watchdog gives stuck work a
failed process exit, without destroying live worker state. Normal main exit
reclaims the legacy singleton graph through the OS after joins instead of
invoking its unaudited destructor ordering. This adds no world-save guarantee.
Lifecycle, signal, supervisor and silent-MySQL-peer regression tests accompany
the fixes. CMake checks actual C++20 library support; CI runs the pinned toolchain.
> **Status:** fixed (codex/cpp20-jthread-lifecycle)

## Zone workers could outlive their zone state during teardown (2026-09-04)

Found while migrating `ZoneGroupThread` to `std::jthread`: the gameserver
destructor deleted `g_pObjectManager` before `g_pThreadManager`, even though
the zone workers continuously dereference zone state owned below the object
manager. The zone pool's `stop()` was also an `UnsupportedError`, so teardown
could either trip its exit-state assertion or let a worker observe freed state.
The pool now requests cancellation for every zone, joins every worker, and is
destroyed before the client, object, and database managers.
> **Status:** fixed (codex/cpp20-jthread-lifecycle)

## SGModifyGuildMemberOK was never handled: `#ifdef __GAME_SERER__` (2026-08-31)

Found while migrating the SG direction onto the dispatch table (task
2.3): `SGModifyGuildMemberOK::execute()` wrapped its handler call in
`#ifdef __GAME_SERER__` — a misspelling of `__GAME_SERVER__` that no
build defines — so the shared server's acknowledgement of a guild-member
rank change was silently dropped by every game server, leaving the
in-memory `Guild` stale until reload. The other nine SG packets spell
the guard correctly. The dispatch table now registers the handler like
its siblings, which both fixes the bug and makes the class of bug
impossible: registration is plain code at the composition root, not a
per-file macro spelling.
> **Status:** fixed (restructuring/dispatch-cg)

## Types.h include order broke the container build (2026-08-31)

The Phase 2 scaffolding PR (#14) clang-formatted `Types.h` *after* its
verification build had already synced sources into the container volume:
the formatter sorted `#include "Utility.h"` above the `types/` block, but
`Utility.h` uses `BYTE`/`WORD`/`sz*` from those headers and its own
`#include "Types.h"` is an empty no-op mid-expansion (the guard is
already set) — so every TU failed on a fresh build while the stale
volume kept passing. Fixed by pinning `Utility.h` below the `types/`
block behind `// clang-format off`. Lesson recorded: **re-run the build
after formatting**, not before.
> **Status:** fixed (restructuring/dispatch-cg)

## Wire max-size reconcile (both repos, 2026-08-31)

The cross-repo inventory diff (`tests/tools/wire_inventory_diff.sh`)
exits 0 as of this set. "Server" = this repo, "client" = the `client`
repo's hand-copied packet classes. None of these changed live wire
*bytes*; they changed the size bookkeeping (`getPacketSize()` /
`getPacketMaxSize()`) that frames and validates them — except where
noted.

- **`CGUseItemFromInventory` / `CGSkillToInventory` phantom field
  (server).** `getPacketSize()`/`getPacketMaxSize()` counted
  `m_InventoryItemObjectID`, which `read()`/`write()` skip (commented
  out). Dormant only because the server never writes CG packets. Found
  by the 1.2 encrypter pins; stated as a fact by the
  `..._WITH_SIZE_DRIFT` tests until fixed.
  > **Status:** fixed (restructuring/wire-maxsize-reconcile)

- **`LCPCList::getPacketSize()` under-reported by SLOT_MAX (server).**
  `write()` emits one PC-type char per slot before the info bodies; the
  size never counted them, so every character-list packet's size header
  was 3 bytes short. Worked only because TCP usually delivers the whole
  packet at once, so the client's length check passed anyway; a
  fragmented delivery could throw `InsufficientDataException` mid-parse.
  Separately, the trailing `m_Agree` byte was written only under
  `__NETMARBLE_SERVER__`, which nothing defines, but was counted
  unconditionally in `getPacketMaxSize()`; the byte and the macro are
  both gone now (R14), so the size no longer carries it.
  > **Status:** fixed (restructuring/wire-maxsize-reconcile)

- **`GCUseOK` client cap dropped large use results (client).** The
  client's factory hardcoded `getPacketMaxSize() = 255` while the body
  is one `ModifyInfo` (max 2042). A use result with more than ~36
  modify entries was rejected by `Player.cpp`'s size guard (bug report +
  disconnect). Now returns `ModifyInfo::getPacketMaxSize()`.
  > **Status:** fixed (restructuring/wire-maxsize-reconcile)

- **`PCSlayerInfo2` dead statements after `return` (client).** Both
  `getSize()` and `getMaxSize()` ended `+ szLevel; + szExp; + szBonus;`
  — the last two are discarded expression statements, so the sums missed
  6 bytes that `read()`/`write()` do transfer (`m_AdvancementGoalExp`,
  `m_AttrBonus`). Undercounted the max of every packet embedding it
  (`GCUpdateInfo`, `GCMorph1`, …).
  > **Status:** fixed (restructuring/wire-maxsize-reconcile)

- **`InventoryInfo::getMaxSize()` phantom Width/Height (client).**
  Counted two `szCoordInven` for fields commented out of
  `read()`/`write()`.
  > **Status:** fixed (restructuring/wire-maxsize-reconcile)

- **`GCAddMonsterCorpse` client max missed the `hasHead` byte.**
  `read()`/`write()` transfer `m_bhasHead`; the client's max summed one
  `szBYTE` too few, rejecting only a maximal (128-char-name) packet.
  > **Status:** fixed (restructuring/wire-maxsize-reconcile)

- **`GCExecuteElement` server max 3 for a 7-byte body.** The factory
  summed `szBYTE + szWORD`, omitting the leading `DWORD` quest id.
  Harmless live (server only sends it) but wrong as contract.
  > **Status:** fixed (restructuring/wire-maxsize-reconcile)

- **`GCNPCResponse` server max counted `szBYTE` for a `WORD` code.**
  > **Status:** fixed (restructuring/wire-maxsize-reconcile)

- **`CLLogin` server max was the dead netmarble sso formula.**
  `szint + 2048 + …` = 2090, while `read()` accepts at most
  1+30+1+30+6+1 = 69 and throws on anything longer. Client's copy also
  drifted (counted a 20-byte password cap; its own `write()` clamp stays
  20, the server accepts up to 30). Both now state 69.
  > **Status:** fixed (restructuring/wire-maxsize-reconcile)

- **`CGSMSSend` message cap disagreed (80 server / 40 client).** The
  client's `write()` asserts `size < 40`, so the server's extra 40 bytes
  of acceptance were unreachable. Aligned both at 40.
  > **Status:** fixed (restructuring/wire-maxsize-reconcile)

- **`GLIncomingConnectionError` max wrong in both repos.** The body is
  two length-prefixed strings each capped at 127 by `read()`/`write()`
  (max 256); the server counted one string (129), the client counted
  `szBYTE + 80` (81). Datagram-only (game→login), so never framed on the
  TCP wire.
  > **Status:** fixed (restructuring/wire-maxsize-reconcile)

- **`LCServerList` / `LCWorldList` server max missed the list-count
  byte** that `write()` emits between the id and the infos (their
  dynamic `getPacketSize()` counted it correctly).
  > **Status:** fixed (restructuring/wire-maxsize-reconcile)

- **`CGBloodDrain` stale client factory max.** Flagged in 1.4 as a
  layout mismatch, but the client's X/Y/Dir reads/writes were already
  commented out to match the server — only the factory max (7) still
  described the old layout. Now 4 (`szObjectID`) in both repos. The
  client's only send site remains commented out (`MPlayer.cpp:3457`).
  > **Status:** fixed (restructuring/wire-maxsize-reconcile)

- **`getDBString` wrote up to two bytes past its `char[100]` with
  client-controlled input.** The legacy SQL escaper (formerly in
  `CGModifyNicknameHandler.cpp`, now single-sourced in
  `MySQLNicknameRepository.cpp`) appended the escape byte and the NUL
  terminator past the buffer whenever an escaped character landed on the
  boundary — and `CGModifyNickname` feeds it a client-supplied string of
  up to 255 bytes with only an empty-string check in front. Rewritten as
  a bounded `std::string` accumulator: byte-identical output for every
  input the old code survived, clean truncation at the same ~100-byte
  horizon for the rest (the column is `varchar(22)` regardless). Found
  by the 3.2 adversarial review.
  > **Status:** fixed (restructuring/repository-pilot)
