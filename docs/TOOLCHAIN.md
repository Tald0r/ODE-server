# Toolchain notes

Replacing the XML dependency made a reproducible Zig/Clang compiler toolchain
viable. That toolchain is now used by both container builders.

## 1. xerces-c replaced by vendored tinyxml2

### What xerces was doing

Four live call sites, all `LoadFromFile`, all at gameserver startup:

| Call site | File | Size |
|---|---|---|
| `GQuestInfoManager::load` (`src/server/gameserver/GQuestInfo.cpp`) | `data/SimpleGQuest.xml` | 17 KB |
| `GQuestInfoManager::load` (same function) | `data/EventGQuest.xml` | 24 KB |
| `GQuestCheckPoint::load` (`src/server/gameserver/GQuestCheckPoint.cpp`) | `data/EventCheckPoint.xml` | 4.4 KB |
| `GQuestCheckPoint::load` (same function) | `data/TravelWay.xml` | 3.8 KB |

48 KB, read once. `data/EventGQuestB.xml` is never loaded. Every `SaveToFile`
call in the tree is commented out, and `LoadFromMem` had no callers, so the
write path and the in-memory parse path were both dead.

`SXml.cpp` enabled `fgXercesSchema`, `fgSAX2CoreValidation` and
`fgXercesDynamic`, but no data file references a schema, DTD or namespace —
`fgXercesDynamic` validates only when a grammar is found, so validation never
ran. The project linked a W3C compliance engine that had nothing to check.

### Two bugs this surfaced

**Element text was silently discarded.** `XMLTreeGenerator::characters()`
declared its length parameter `unsigned int`; Xerces-C 3.x declares that
virtual as `XMLSize_t` (i.e. `size_t`). The signatures never matched, so the
override never bound and `DefaultHandler`'s no-op ran instead. Every `<Title>`,
`<Script>`, `<CompleteMessage>` and `<FailMessage>` body in every file was
dropped. The mismatch dates to Xerces-C 3.0 (2008) changing that signature;
`SXml.cpp` was written against Xerces 2.x in 2003.

Nothing reads `XMLTree::GetText()` today, which is why this went unnoticed.
The replacement captures text, so `GetText()` now returns content where it
previously returned `""`. No caller is affected. `-Woverloaded-virtual`, or
C++11 `override`, would have caught this at compile time.

**Non-ASCII text was locale-dependent.** Values went through
`XMLString::transcode()` to the process's *local code page*. Under the
container's POSIX locale that double-encoded every non-ASCII byte
(`0xBA` → `U+00BA` → `0xC2 0xBA`), so the parsed text differed depending on
the `LANG` of whoever started the server.

tinyxml2 does no transcoding: bytes reach `XMLTree` exactly as they sit in the
file. That is deterministic. Re-encoding is a separate decision and belongs
where the text is used, not in the parser.

### Encoding state of the data files

The files declare `encoding="iso-8859-1"` but actually hold a **mix** of
EUC-KR and UTF-8 — evidence of a half-finished encoding migration. Of 19
distinct non-ASCII attribute values, 13 are EUC-KR and 6 are UTF-8. Byte
passthrough preserves that distinction so a proper cleanup can be done as its
own reviewable change; the old path double-encoded both uniformly and
destroyed it.

Converting `data/*.xml` to UTF-8 is **not** done here. It is a data change,
and it would need the client's expectations checked alongside it.

The database dumps went the other way: `initdb/*.sql` create every table in
`utf8mb4`, the servers pin their MySQL session to `utf8mb4`
(`src/server/database/Connection.cpp`), and the EUC-KR/GBK text the dumps
carried as latin1 mojibake was re-encoded to real UTF-8 with
`tools/reencode_legacy_dump.pl`. Under the old latin1 session the database
was a byte store — whatever the client sent came back unchanged — so the
wire and the database now agree on UTF-8.

### Evidence

`tests/xml_parse_test.cpp` pins the parsed shape of all four files against
`tests/golden/xml-parse.txt`, with every non-ASCII byte spelled `\xNN`.
Re-record deliberately with `UPDATE_GOLDENS=1 ./bin/wire_tests`.

Comparing the old and new backends over those files:

```
line counts                : 2664 vs 2664   (identical structure)
identical lines            : 2565
differ, all explained by    :   99   xerces = UTF-8 re-encoding of the raw
                                     byte read as latin-1 (double-encoding)
differ, unexplained         :    0
```

Every node, attribute name, attribute count and child count matches exactly.
The only differences are the two intended ones above.

`tests/tools/xml_dump.cpp` is the standalone dumper used to produce that
comparison; it is not built by default.

## 2. Building with `zig cc`

`Dockerfile` and `Dockerfile.dev` pin Zig 0.16.0, whose C++ driver reports
Clang 21.1.0. CMake always reaches it through `cmake/zig-toolchain.cmake`, so
the distro's compiler version no longer controls the language features used by
container builds.

For the fast development-volume build:

```bash
docker build -f Dockerfile.dev -t darkeden-dev .
make dev-test
make dev-build
```

The production image uses the same compiler and requires C++20:

```bash
docker build -t darkeden:local .
```

Both image builds accept `--build-arg ZIG_VERSION=...` for an intentional
toolchain update. The current tree requires C++20; the former C++17 rollback
lane was retired when cooperative `std::jthread` ownership entered the zone
tick.

For a manually installed Zig toolchain, the equivalent direct CMake commands
are:

```bash
cmake -B build-zig -DCMAKE_TOOLCHAIN_FILE=cmake/zig-toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_STANDARD=20
cmake --build build-zig -j
```

Zig is located as `$ZIG`, else `zig` on `$PATH`, else `python3 -m ziglang`
(the PyPI `ziglang` package ships the full toolchain).

`tools/devbuild.sh` gives every Zig version, target and build type
its own CMake tree, output root, ccache directory and Zig cache directory. Run
`tools/devbuild.sh output-dir` with the same environment to print that lane's
artifact root. For manual builds, use one build tree and output root per
toolchain configuration, for example
`-DDARKEDEN_OUTPUT_ROOT="$PWD/output-zig-cxx20"`.

### Why it works here

The source is unusually portable for its age: no `#pragma GCC`, no inline
asm, no `__builtin_*`, no `typeof`. The compiler extension it leaned on was
`__PRETTY_FUNCTION__`, which clang supports and which
`std::source_location` has since replaced everywhere (ratchet R8 holds the
non-comment uses at 0 — see "Diagnostics without location macros"). One
deliberate extension is left, `Statement::executeQuery`'s
`__attribute__((format(printf, 2, 3)))` in `src/server/database/Statement.h`,
which is what makes `-Wformat` check every repository statement's
conversions; clang supports it too.

Dropping xerces was the prerequisite. `zig c++` links LLVM's libc++, not GNU
libstdc++, and `SXml.cpp` caught `const XMLException&` across the xerces
boundary — cross-runtime `type_info` matching is exactly where libc++abi and
libsupc++ mixing goes wrong, and it fails by silently falling through to
`catch (...)` rather than by crashing. With xerces gone, every remaining
external dependency (libmysqlclient, lua, zlib) exposes a C API and carries no
C++ ABI, and so does the vendored argon2 (`third_party/argon2`, compiled as C
by the same Zig driver). **Re-introducing a C++-API dependency reopens this.**

### What it buys

- One compiler binary, byte-identical on every machine and CI runner, rather
  than whatever `g++` the distribution shipped.
- Zig can select a target and libc version at configure time for code whose
  complete dependency set supports that target:
  ```bash
  cmake -B build-zig-glibc217 \
        -DCMAKE_TOOLCHAIN_FILE=cmake/zig-toolchain.cmake \
        -DDARKEDEN_ZIG_TARGET=x86_64-linux-gnu.2.17 \
        -DDARKEDEN_OUTPUT_ROOT="$PWD/output-zig-glibc217"
  ```
  The target is a CMake cache value, so it is visible in the generated Ninja
  commands and changing it requires a separate build tree.
- Cross-target compilation support without constructing a libc sysroot by
  hand.

The current container installs MySQL, Lua and zlib development libraries from
its Ubuntu 20.04 package repository. Those host libraries are not a Zig cross
sysroot, so a complete server binary built by these Dockerfiles is supported
only for the container's native target. A non-native target or older glibc
floor requires target-compatible builds of all three dependencies; do not
infer CentOS 7 compatibility from `DARKEDEN_ZIG_TARGET` alone.

### What it found immediately

Zig's Debug build enables UBSan by default. The first run of `wire_tests`
under it aborted on pre-existing undefined behaviour in the wire layer,
unrelated to any change here:

```
panic: store of misaligned address 0x2dcbaf76 for type 'unsigned int',
       which requires 4 byte alignment
  src/Core/SocketOutputStream.h:174   *((T*)(m_Buffer + m_Tail)) = buf;
  src/Core/CGWhisper.cpp:62           oStream.write(m_Color);
```

`SocketOutputStream::write<T>` and `SocketInputStream::read<T>` cast a `char*`
ring buffer at an arbitrary byte offset to `T*` and dereference it. That is UB
at any misaligned offset; x86 tolerates it in hardware, which is why GCC builds
have run for two decades without complaint. It is a real portability and
optimisation hazard, not a false positive.

Fixed at all five sites (three stores, two loads) by using `memcpy` with the
same `sizeof(T)` length — the idiom the wrap-around branch of that very
function already used. For a compile-time-constant size this lowers to the
same single instruction, so there is no cost in the hot path. The wire golden
tests pass unchanged under both toolchains, which is what establishes the
output is byte-identical.

The C++17/C++20 migration pass found a second checked-conversion case:
`tileDistance` intentionally narrows a distance to the protocol's byte width.
The code now spells that modulo operation explicitly, preserving the existing
300 → 44 behavior without asking Zig's checked Debug mode to perform an
out-of-range cast.

This is the argument for sanitizers in one example: the bug was reachable from
an ordinary `CGWhisper` round-trip, sat in the packet write path used by every
outbound packet, and no amount of reading found it in twenty years.

### What it is not

`zig cc` is a *compiler*, not a build system or a package manager. Zig's own
package management (`build.zig.zon`) is reachable only through `zig build`,
which would mean replacing CMake wholesale. With three external C
dependencies that trade does not pay for itself, so this setup keeps CMake and
uses Zig only as the compiler driver.

## 3. Where C++20 pays off in DarkEden

Changing the compiler flag does not by itself make the server safer or faster.
The value is that new/refactored code can use a stronger vocabulary at the
places where this codebase currently relies on conventions, raw buffer pairs,
macros and polling loops.

The project now requires C++20. The first production use was cooperative zone
worker shutdown; new facilities are adopted at focused boundaries instead of
through tree-wide style conversions. The `std::variant`-backed
`Outcome<Events, Rejection>` followed the same rule, one decision at a time,
and now has production callers in all three processes: every loginserver
`CL*` decision (create, select, login, delete, reconnect, register, world and
server selection), the gameserver's Exchange service, guild NPC, party invite,
trade prepare and trade table protocols, and the sharedserver's guild
mutations. Each answers the events to perform — the rows to write, the packet
to send and its recipient, the record to open or close — or the typed reason
the request was refused, and the handler performs the writes in the old
order. The full list with file paths is task 3.1 in `docs/RESTRUCTURING.md`.

| Priority | C++20 facility | Project seam | Main benefit |
|---|---|---|---|
| P0 | `std::jthread`, `std::stop_token`, atomic wait/notify | `ManagedThread` (every worker in all three servers) | Cooperative shutdown and owned joins instead of unsupported `stop()`, `while (true)` and sleep polling — done |
| P0 | `std::span`, concepts, `std::endian`, `std::bit_cast` | `SocketInputStream`, `SocketOutputStream`, packet codecs | Bound buffer lengths to their data and reject unsafe wire types at compile time — done for the streams, the frame and the string fields; the field-by-field codecs stay by decision |
| P1 | `constexpr`/`consteval` metadata and concepts | `PacketMeta.h`, every packet factory, `PacketFactoryManager` | Detect duplicate IDs, invalid sizes and incomplete registrations during compilation — done |
| P1 | `std::source_location` | `Assert.h`, `Exception.h`, DB/error macros | Preserve call-site diagnostics without compiler-specific macros or repeated file/line plumbing — done |
| P1 | `std::latch`, `std::barrier`, `std::counting_semaphore` | thread startup phases and bounded work queues | Replace timing assumptions with explicit readiness and back-pressure — the queue half is done as the zone-group mailbox; the readiness half is retired, no start path waits on a sleep |
| P2 | ranges, views, `contains`, `erase_if` | manager/registry traversal | Reduce hand-written iterator and double-lookup mistakes once ownership and lock boundaries are explicit — a standing convention, applied where the guild tables were made lock-safe |

### Structured thread lifetime and cancellation

`ManagedThread` is now the only subclass of the legacy `Thread`, so every
worker in all three server processes uses it: gameserver zone, login-link,
shared-link, GDR, SMS and mofus workers, the loginserver's
`GameServerManager` (UDP game-server link) and the sharedserver's
`GameServerManager` (TCP game-server link).
`ManagedThread` owns a `CooperativeThread` backed by `std::jthread`. What is
left of the legacy base is `start()`/`stop()`/`join()`/`run()`, the status
enum and `Thread::self()` (the key for per-thread database connections); the
pthread-only `detach()` and the unused static `join()` overloads are gone.

`std::jthread` owns the join operation and propagates a `std::stop_token` to
the worker. A stop-aware condition-variable wait can wake immediately during
shutdown instead of waiting for the next polling interval. Where only a state
change is needed, `std::atomic::wait`/`notify_*` can avoid a mutex and repeated
wakeups. Cancellation is cooperative: it cannot interrupt arbitrary C++ code,
a held mutex, or a synchronous library call.

`ZoneGroupThread` retains its one-millisecond wait and existing group lock.
Start/stop operations are serialized, stop-before-start permanently cancels
the worker, and an in-progress join does not prevent another caller requesting
stop. Worker exceptions are retained for reporting after join and request a
failed process shutdown. Pool startup rolls back already-started workers on
any exception; shutdown requests all stops before joining. Derived destructors
must join before their members disappear; a base destructor alone is too late.

SIGTERM/SIGINT only store a lock-free atomic request, using operations permitted
in [C++ signal handlers](https://eel.is/c++draft/support.signal). All three
processes now install that handler and share the same teardown: the main loop
(gameserver `ClientManager`, loginserver `ClientManager`, sharedserver
`HeartbeatManager`) returns, every worker is requested to stop and joined,
then main flushes its status message and calls `_Exit` with a failure code if
any step failed. `LoginServer::stop()`/`SharedServer::stop()` are idempotent
and no longer throw `UnsupportedError`. The OS reclaims the
legacy singleton graph: its destructor dependency order is not fully audited,
so normal process shutdown deliberately does not invoke that graph. Explicit
`GameServer` destruction also joins workers before releasing dependencies.
Joining workers does **not** provide a new world-save/transaction guarantee.

MySQL connections default to a five-second connect timeout and 300-second
read/write timeouts, allowing longer normal-service queries. Operators can set
`DARKEDEN_DB_CONNECT_TIMEOUT_SECONDS` and `DARKEDEN_DB_IO_TIMEOUT_SECONDS` to
positive integer seconds (Compose forwards them). These are per-operation
limits with [client retry semantics](https://dev.mysql.com/doc/c-api/8.0/en/mysql-options.html),
not an overall query or shutdown time guarantee. Stop is checked between zone
connection setup operations. Both ends of the login link use a nonblocking UDP
socket so idle traffic cannot prevent shutdown. A separate watchdog starts
before server initialization in each process and allows 30 seconds after a
signal/error/shutdown request; if I/O or gameplay code stays blocked, it exits
the whole process with failure without freeing memory underneath live workers,
naming the process that gave up. The container supervisor keeps
login/shared alive while gameserver drains, enforces a 35-second backstop, then
gives login/shared 8 seconds of their own before SIGKILL — 43 seconds in total,
inside Compose's 45-second stop grace period.

Regression tests exercise concurrent lifecycle calls, worker errors, pool
rollback and stop/join ordering, dependency lifetime, signals, a stuck worker,
stop-aware waits waking without sitting out their polling interval, a silent
MySQL peer, and the real `docker/start.sh` with stand-in processes — both when
they all drain and when one login/shared stand-in ignores SIGTERM.
The pinned-Zig CI job runs these tests and builds all production targets and
the production image only on master pushes/merges. PRs and feature-branch
commits use local verification to conserve Actions minutes. CMake probes the
required library facilities, so an
unsupported toolchain fails at configuration instead of deep in compilation.

### Type-safe packet buffers

Done for the stream layer; the packet codecs above it are unchanged.

`SocketInputStream::read<T>` and `SocketOutputStream::write<T>` copy
`sizeof(T)` bytes of an object's representation out of / into the socket ring
buffer, so whatever compiles *defines* the protocol. Before this change a
pointer, a `std::string`, a padded class or a platform-sized integer (`long`
and `size_t` are 8 bytes on the Linux server and 4 on the Win32 client) all
compiled silently. `src/Core/WireTypes.h` now states the rule once as a
`de::WireScalar` concept, and both templates are constrained by it.

The accept list came from evidence rather than taste: every `T` that
instantiates the two templates across de-kernel and all three servers was
enumerated first, with a throwaway deprecated-probe build. It is `bool`,
`char`, `signed char`, `unsigned char`, `short`, `unsigned short`, `int`,
`unsigned int` and `std::uint64_t`. That covers every `Types.h` field alias,
since `BYTE`/`WORD`/`DWORD` are `unsigned char`/`unsigned short`/`unsigned
int`. Pointers, arrays, class types, floating point, enumerations and
signed 64-bit in every spelling are rejected — trivially copyable
aggregates included, because "can be memcpy'd" is not the question the
wire asks.

One caveat is unavoidable, and the header spells it out. The Exchange packets
(`CGExchangeBuy`, `GCExchangeBuy`, `GCExchangeList`) write their listing id as
a `uint64_t`, so that has to stay admitted — and on the LP64 server
`std::uint64_t` **is** `unsigned long`, so `unsigned long` and `size_t` are
admitted with it and a stray `write(size_t)` still compiles here while it
would be four bytes on the client. Signed 64-bit is not admitted, and that is
what keeps plain `long` out; its only instantiation sites were the dead
`readEncrypt(long&)` / `writeEncrypt(long)` overloads, now removed. The test
asserts this split relative to `std::uint64_t` rather than to a spelling, so
it states the intent on a platform where the two differ.

Constraining alone would have been a hazard rather than a fix: with
`write<T>` constrained away, `write(someCharPointer)` would have found
`write(const string&)` through a user-defined conversion and put a
*different* byte sequence on the wire. Each template therefore has an
unconstrained sibling whose only statement is a `static_assert`, so a
rejected type is a named compile error instead.

The buffer paths gained `read` / `peek(std::span<std::byte>)` and
`write(std::span<const std::byte>)`. The `read` and `write` spans carry the
implementation, and are defined **inline in the headers** so the scalar
templates keep handing their `memcpy` a compile-time-constant `sizeof(T)`:
at `-O2` a four-byte field is still the single load/store the section above
describes, not a call with a runtime length. (`peek` stays out of line --
nothing calls it with a constant size.) The
`char*` / `const char*` plus `uint` signatures are kept for the existing call
sites and forward to them, so the hundreds of callers and the exceptions they
rely on (`InvalidProtocolException` for a zero-length request,
`InsufficientDataException` for a short buffer) are unchanged. The scalar
templates forward as well, which removes the hand-copied second version of
the ring-buffer walk — the copy the misaligned-access fix above had to patch
separately.

Inline is a request, not a guarantee, and the throw expressions were enough
to lose it: with `throw InvalidProtocolException(...)` and
`throw InsufficientDataException(...)` written in `read(std::span)`, Zig
0.16's Clang at `-O2` kept it out of line in every packet read measured
(`CGAttack::read` called it sixteen times, `GCUpdateInfo::read` 21, each
copy a `memcpy` with a runtime length). Both refusals are now out-of-line
`[[noreturn]]` calls (`failRead`, `failUnderflow`), and the definition
inlines: on the unwrapped path a scalar field is a `length()` call, a
compare and a single load. The frame bound
`readPacket` puts on a packet's `read()` adds nothing to that path, since
it is the same `len > length()` compare against a tail pulled in to the
body's end. No test checks this; to check it again, compile a packet read
to assembly and count the call instructions to the span overload. There
should be none:

```sh
cmake/zig-c++ -std=c++20 -O2 -UNDEBUG -S -D__LINUX__ -D__GAME_SERVER__ \
    -D__COMBAT__ -Isrc/Core -o CGAttack.s src/Core/CGAttack.cpp
grep -cE '^[[:space:]]*(bl|callq?)[[:space:]]+_ZN17SocketInputStream4readE(NSt3__1|St)4span' CGAttack.s
```

`-UNDEBUG` is needed because zig's `-O2` defines `NDEBUG`, which
`Exception.h` and `Assert.h` refuse with `#error`. The pattern counts only
call instructions (`bl` on aarch64, `call` on x86-64): a plain count of the
symbol also hits its definition and the debug strings. It covers both spellings of
the symbol, `...4readENSt3__14span...` under libc++ (zig) and
`...4readESt4span...` under libstdc++ (the `darkeden-fuzz` image's
Clang 18). Measured with zig on aarch64, before and after the frame
bound: `CGAttack::read` 16 calls to 0, `CGMove` 12 to 0, `GCUpdateInfo`
21 to 0, `CLLogin` and `CGSay` 1 to 0; `CGAttack` under Clang 18 and
libstdc++ also went from 16 to 0. The loss is target-dependent: for
x86-64, zig already inlined the span overload into `CGAttack::read`
before the change, so check on aarch64, where it was lost.

`std::endian` states the deployed byte order once, as a `static_assert` in
`WireTypes.h`: the wire format is the little-endian in-memory representation,
no packet swaps bytes, and none is introduced here. `std::bit_cast` is
deliberately **not** used. The aliasing casts it would replace are already
gone (that is what the five `memcpy` sites above are), and the remaining
pointer conversion is to `std::byte*`, which may legally alias any object,
whereas `bit_cast` would add a copy through a temporary array in a hot path.

`tests/wire_types_test.cpp` pins both lists with `static_assert`s, round-trips
every admitted scalar and a span buffer through real loopback sockets, and
drives a 16-byte ring buffer so the wrap-around branch and its split copy
actually run. The wire-layout inventory and every golden are unchanged, which
is what establishes that no packet moved.

The length in a packet header is measured rather than declared.
`writePacket` reserves the size field when it writes the header, writes the
body, and then fills the field in with the number of bytes the body produced;
`Packet::writeHeaderNBody`, which frames a packet into the buffers that are
copied to many sockets, does the same. A reservation is tracked as a distance
from the buffer head rather than as a pointer or a buffer index, so it
survives both the ring buffer's wrap point — a field may straddle it, and is
patched byte by byte — and a `resize()` triggered by the body itself, which
moves the buffered bytes to the front of a larger allocation but keeps their
order. A `getPacketSize()` that has drifted from `write()` therefore cannot
put a wrong length on the wire; the disagreement is reported to
`packetsizeerror.txt`, the file an undersized-buffer `resize()` already names,
because the declared value still sizes the send buffers.
`tests/wire_types_test.cpp` pins a packet that lies short and one that lies
long, both contiguously and with the size field split across the wrap point,
and that an honest packet's frame is byte for byte what it was.

The server-to-server UDP path follows the same rule. `Datagram::write` still
sizes the datagram buffer from `getPacketSize()` — that value is a capacity
hint now, not the frame length — writes the id and a placeholder size field,
writes the body, then fills the size field in with the bytes the body produced
and sets the datagram's own length to match. A body that outgrows the buffer
grows it, rather than the write aborting half way through a partly filled
datagram. Drift is reported to `packetsizeerror.txt`, the same file the
stream names; every datagram packet's declaration agrees with its `write()`,
and `tests/datagram_frame_test.cpp` pins that for the three whose layouts are
least obvious (`GMServerInfo`'s per-zone table and the two
`IncomingConnectionError` packets' pair of length-prefixed strings).
The receive side is unchanged
and still requires a datagram to hold exactly one packet of the length its
header claims — the sender now satisfies that for a lying packet too.
`tests/datagram_frame_test.cpp` pins the short and long lies, the byte image
of an honest packet's datagram, the growth path, and a real UDP loopback trip
through the factory-backed receive side.

Every byte is encrypted exactly once on its way out. `SocketOutputStream`
encrypts what it has buffered before it sends it, and the client sockets are
non-blocking, so a peer that has stopped reading makes a send take part of the
buffer and report zero for the rest. The stream therefore tracks how much of
the buffer, counted from the head, is already encrypted; a flush encrypts only
from there to the tail, and the bytes a short send left behind go out as they
stand. That distinction matters because the key advances one step per byte: a
second pass over the same bytes would run from a key that has moved on,
producing bytes the receiver's single pass cannot undo and desynchronising its
key for everything sent afterwards. Counting from the head rather than from a
buffer position is what lets the mark survive `resize()`, which moves the
buffered bytes to the front of a new allocation but keeps their order, and the
wrap point, where a flush sends in two pieces and may be cut short in either
of them. (The transform itself is currently disabled — `EncryptData` returns
before its first statement, on both the sending and the receiving side — so
today this is a latent fault rather than an observed one, and the bytes on the
wire are unchanged either way.) `tests/wire_types_test.cpp` scripts a socket
that accepts an exact number of bytes and then refuses, cuts a flush in a
contiguous region and in each half of a wrapped one, grows the buffer between
a cut and the flush that finishes it, and checks each time that the receiving
stream decrypts back to what was written.

The codecs above the stream read and write field by field, and that is the
decision rather than a gap: the client repository keeps a hand-written copy of
every codec, so a declarative layout on the server side would describe each
field twice without moving a byte (see "Features that are not first moves").
What the rounds closed instead is what made the hand-written shape unsafe.
The wire carries the measured body length (`writePacket`, `Datagram::write`),
so `getPacketSize()` only budgets buffers, and every declared size is pinned
against the bytes `write()` emits by the golden suites. String fields carry a
length prefix through `de::wire::readString`/`writeString`
(`src/Core/WireString.h`), with `readString16`/`writeString16` for the few
whose length travels in a WORD, which state a field's minimum and maximum
once and refuse outside them at the same points the hand-written sequences
did; new or touched string fields use it, and the shrink-only ratchet R9,
now at zero, counts the legacy `read(m_X, szX)` sequences that remain.

### Compile-time packet metadata

Every packet factory now states its static facts as constant expressions:
`static constexpr` `kPacketID`, `kName` and `kMaxSize` members, with the three
getters delegating to them and `createPacket` the only virtual that still does
work (`src/Core/PacketMeta.h` documents the contract).
`kMaxSize` is brace-initialised, so a body-size expression that no longer fits
`PacketSize_t` is a narrowing error where it is written; the `Info::getMaxSize`
helpers the expressions call are `constexpr` too, and the one that lived in a
`.cpp` (`QuestStatusInfo`) moved inline. Runtime creation is unchanged:
`PacketFactoryManager` still owns one heap factory per id and
`createPacket()` still allocates.

`de::PacketFactoryType` is the concept for that contract. It instantiates
`std::integral_constant` on the members rather than merely naming them, so a
factory that still computed its id in a virtual body fails the concept, not a
`static_assert` somewhere inside a table. `de::packet::FactoryList<F...>` folds
a pack of factories into a `constexpr` `Meta` table and runs `consteval`
`validateRegistry` over it while the class is instantiated; a duplicate,
out-of-range or unnamed id is a compile error whose "in instantiation of
`RegistryCheck<RegistryError::DuplicateId, 231>`" note carries the rule and the
id. `PacketFactoryManager::init()` is four such lists (client link, guild link,
login-only, game-only) concatenated per server with `de::packet::Concat`, which
validates the joined table, and `forEach` instantiates them in pack order. The
per-server membership is the old `addFactory(new ...)` set (registration order
changed, which a slot table does not observe), pinned in
`tests/ratchet/factory_registrations.txt`: `ratchets.sh` regenerates the file
from the type lists with `tests/tools/factory_registrations.pl` and fails on
any add or drop — the inventory check alone only proves registered ⊆ inventory,
and the first cut of this branch dropped `CLRegisterPlayerFactory` from the
loginserver without any test noticing. Note the test build defines no server
macro, so the production lists are instantiated (and their tables validated)
only by the server builds; a factory listed twice across lists is caught
there, not by `make dev-test`.
`DE_REGISTER_PACKET_HANDLER` binds handlers by `Cls##Factory::kPacketID`
instead of constructing a throwaway packet to ask for its id.

`tests/packet_meta_test.cpp` compiles the whole kernel (the generated
`ALL_PACKET_FACTORIES_TYPES` section of `AllPacketFactories.inc`) into one
`FactoryList`, so two kernel factories claiming one id is now a build failure,
pins each rejection rule on hand-built tables, and checks at run time that every
factory's constants agree with its virtuals and with the packet it creates —
that last agreement is what keeps the dispatcher pointed at the same handlers.

The link a packet rides is a fourth static fact, and it needs no per-factory
edit: `de::packet::directionOf` is a `consteval` parse of the two-letter prefix
every packet name already carries (`CG`/`GC`/`CL`/`LC`/`GL`/`LG`/`GS`/`SG`/`GG`,
plus `GM` for the one server-info datagram a gameserver sends the loginserver),
and `metaOf` records the result on `Meta`. A name whose prefix names no link is
`Direction::Unknown`, which `validateRegistry` rejects; `FactoryList` also runs a
per-factory `KnownDirection` check, so that failure's "in instantiation of
`KnownDirection<XFactory>`" note names the factory, which is what a misspelled
name needs, rather than the packet id, which is not the thing that is wrong.

The direction is enforced where a packet is claimed, not where it is listed.
Each composition root states the links its server accepts as a `constexpr
de::packet::DirectionSet kReceivedDirections`, and `DE_REGISTER_PACKET_HANDLER` /
`_NOPLAYER` / `_FN` `static_assert` that the packet being registered rides one of
them: a handler bound to a packet the server never receives is a compile error,
and a root that states no set does not compile at all. The registration lists
keep their membership, because direction is not what decides whether a factory
is looked up. Both servers do register packets they only ever send (242 `GC`
factories on the gameserver, 17 `LC` on the loginserver), but the prefix cannot
tell those from the ones that are read: the gameserver's client read path
creates `GCFriendChatting` and the two personal-store `GC` packets through
`PacketFactoryManager::createPacket`, and the loginserver creates
`CGConnectSetKey` the same way. So the accepted sets are per server and wider
than the naive reading — gameserver `CG GC GG LG SG`, loginserver `CG CL GL GM`,
sharedserver `GS` — and each is written down beside the registrations it
governs.

A link can also be narrowed to some of its packets: `narrowed(link, filter)`
returns a copy whose `admits(meta)` answers true for that link only when the
filter does, and each macro `static_assert`s `admits()` as its second check.
The gameserver narrows `GC` with `sentOnGameClientLink`
(`src/Core/GameClientLink.h`): a GC packet reaches it only on the client link,
whose `GPS_NORMAL` set admits only the GC-named packets listed there, so a GC
handler outside that list would never run and does not compile.

`tests/packet_meta_test.cpp` pins the parse of every prefix, that no kernel
factory is left unclassified, that `DirectionSet` answers only for the links it
was given and a narrowed link only for its filter, and that an unclassifiable
entry is rejected by `validateRegistry` naming its id. As with the registration
lists, the composition roots' sets are compiled only under a server macro, so
their `static_assert`s fire in the production builds, not in `make dev-test`.

This complements rather than replaces the golden tests: compile-time checks
prove internal consistency, while goldens prove compatibility with the client
and the encrypted wire format. The original item's other half, a fixed or
minimum size per packet, is retired: the frame carries the measured body
length, a reader that runs out of bytes refuses with
`InsufficientDataException` (pinned by the short-stream tests), and every
declared size is pinned against the bytes `write()` emits, so a minimum the
factories would state by hand adds nothing the wire does not already enforce.

### Diagnostics without location macros

Done for the call-site diagnostics. `Assert.h` declares `assertionFailed()` /
`protocolAssertionFailed()`, ordinary functions whose last parameter defaults
to `std::source_location::current()`; `Assert(expr)` and `ProtocolAssert(expr)`
remain macros only because the diagnostic needs the unevaluated `#expr` text
and must evaluate the expression exactly once. `Throwable::addStack()` has the
same defaulted parameter, so `__END_CATCH` / `__END_CATCH_NO_RETHROW` no longer
forward `__PRETTY_FUNCTION__`, and `END_DB` / `END_DB_EX` in
`src/server/database/DB.h` take the enclosing function the same way. Under
Clang `source_location::function_name()` is the text `__PRETTY_FUNCTION__`
produced, so `assertion_failed.log`, `protocol_assertion_failed.log`,
`DBError.log` and `getStackTrace()` are byte-identical to before; the historical
quirks (no separator between the function name and the expression, function
names only in the stack trace) are preserved deliberately. Because
`std::source_location` is portable, the dead `__WIN32__` / `__WIN_CONSOLE__` /
`__MFC__` branches of `Assert`, `__BEGIN_DEBUG` and `__END_DEBUG` are gone.
`Assert1.h` - a near-duplicate included by ~110 files - is now a one-line
forwarder to `Assert.h`, and its never-compiled twin `Assert1.cpp` (a copy of
the old implementation, in no build list) is deleted.
`tests/diagnostics_test.cpp` pins the message layout, the reported
file/line/function and the stack-trace format.

No `__FILE__`, `__LINE__` or `__PRETTY_FUNCTION__` use is left in `src/` code.
Ratchet R8 holds the count of non-comment lines naming the last of those at 0;
the comments that explain the `source_location` equivalence still spell it, and
the rule is line-based, so they do not count.

`UnsupportedError` is what the unimplemented virtual stubs and the
platform-specific stubs in `FileAPI.cpp` / `SocketAPI.cpp` throw, and its
default constructor takes the enclosing function from a defaulted
`std::source_location`: `throw UnsupportedError()` reports the stub's own name,
with no argument at the throw site, so all 73 of them are spelled the same
way. The `const string&` constructor still serves the sites that pass their own
text. The same defaulted parameter carries the direct `addStack()` call in
`GamePlayer.cpp` and the two `Party.cpp` debug prints. Every one of those
messages is the text the macro yields at the same point, so logs and stack
traces are unchanged.

### Explicit coordination and bounded work

Use `std::latch` for one-shot worker readiness, `std::barrier` only where a
repeated phase boundary genuinely exists, and `std::counting_semaphore` for a
bounded producer/consumer queue. These primitives are preferable to adding
another sleep-and-check loop, but they should follow an ownership audit: a new
primitive cannot make shared gameplay state safe if its owner and lock scope
are unclear.

The cross-thread mailbox is the first of these queues. `de::Mailbox<Item>`
(`src/server/Mailbox.h`) is a mutex-guarded vector: `post()` from any thread;
`drain(visit[, onFailure])` by the owner, which takes the batch out under the
mutex and visits it lock-free in posting order, isolating a throwing item
through the caller's failure handler; `drainIf(take, …)` for an owner that may
run only some kinds of item, which leaves the rest in place, in order, ahead
of later posts; and a lock-free `empty()` so the owner's hot loop pays nothing
when nothing is queued. `GamePlayer` carries a box of player commands, and
`de::postToPlayer` (`PlayerMailbox.h`) is what the six guild SG handlers and
`LGKickCharacter` now call instead of mutating gold, guild id and kick flags
on the `SharedServerManager` / `LoginServerManager` threads under only the
PCFinder lock — the data race CLAUDE.md listed first among the known
violations. The box is on the player, not the zone group, because the owner of
a player changes (main thread during login and zone transfer, a zone thread in
between) and the creature's zone pointer does not track that. Each owner
drains from the one place it processes its players: `ZonePlayerManager` runs
everything under the group mutex; `IncomingPlayerManager` runs only
`Scope::Player` commands (the kick, which must reach a stuck session precisely
while the main thread holds it) and leaves `Scope::Zone` commands queued for
a zone thread. A player that logs out first runs the commands' `ifGone`
handlers from `~GamePlayer`, right after the PCFinder removal, so exactly one
of command/`ifGone` runs. `ZoneGroup` has a box of thunks too, drained at the
top of the tick; its producer is dynamic-zone recycling, which hands an
instance's `init()` to the group that owns it. The boxes are deliberately
unbounded: the producers are the inter-server links, and blocking one of them
on a busy zone group's back-pressure would stall guild and login traffic for
every group; the drains log a pass deeper than their depth warning instead.

The other cross-thread race CLAUDE.md listed, the dynamic-zone `addZone()`,
turned out to be a read-mostly-table problem rather than a queue problem:
the requesting player's zone thread inserts into the template group's zone
map and the `ZoneInfoManager` tables while that group's heartbeat and every
transport on every thread read them. `de::Snapshot<T>`
(`src/server/Snapshot.h`) publishes such a table copy-on-write behind a
`shared_ptr<const T>`: readers load a snapshot they may iterate for a whole
tick, writers copy, change and swap the pointer under a leaf mutex held for
nothing else, so no lock order is created. (`std::atomic<std::shared_ptr>`
would be the C++20 spelling; the pinned libc++ 21 does not ship it, so the
slot is a mutex-guarded pointer.) `DynamicZoneGroup` serialises instance
selection/creation under its own mutex, and the instance status is a
`std::atomic<int>`. The readiness half of this row is retired: no start path in
any of the three processes waits on a sleep for a worker (the pool, the
managers and the three `main`s were checked), so there is no seam for a
`std::latch` to replace.

The 460 critical sections in `src/` are now RAII (the count moves with the code; the audit script, not this number, is what holds). `__ENTER_CRITICAL_SECTION(x)`
declares a scoped `CriticalSection` guard (`src/Core/Exception.h`) over any
BasicLockable — `Mutex`, `Zone`, `ZoneGroup`, `PCFinder`, `ObjectRegistry` — and
`__LEAVE_CRITICAL_SECTION` closes that block; the guard calls exactly `lock()`
and `unlock()`, so `ZoneGroup` keeps its `DE_OWNERSHIP_CHECKS` owner record. The
pair previously expanded to a bare `x.lock()` plus a
`catch (Throwable&) { x.unlock(); throw; }` tail, which released the lock only
for `Throwable`: a `std::bad_alloc`, a `std::out_of_range`, or the `const char*`
that `END_DB` rethrows walked past it holding the mutex, and a `return` inside
the section skipped the unlock entirely unless the author wrote one by hand.
Because the guard tracks ownership, hand-written `x.unlock()` inside a section
is now a double unlock and is forbidden; release early through
`__CRITICAL_SECTION_LOCK.unlock()` (and `.lock()` to retake it) instead.
`tests/critical_section_tests` pins the exits, and the ctest entry
`critical_section_audit` (`tests/tools/critical_section_audit.pl`) fails the
build on a hand-written unlock or a
`__LEAVE_CRITICAL_SECTION` whose argument does not match its `__ENTER`'s. This
removes a lock-leak class; it does not change which thread owns which state, so
the ownership violations CLAUDE.md listed have since been closed one by one;
its "Known violations" list keeps every entry struck through with the fix
beside it.

### Safer collection traversal

Ranges and named operations such as `contains` and `erase_if` can remove a lot
of iterator boilerplate in managers and registries. Apply them during focused
ownership refactors, not as a tree-wide style conversion. Views borrow their
source; returning or storing a view into a temporary or into a container after
its lock is released would replace visible iterator code with a subtler
lifetime bug.

### Features that are not first moves

- Coroutines do not turn the current blocking socket and thread-per-zone
  architecture into asynchronous I/O. They become useful only with an evented
  transport, explicit cancellation and back-pressure; that is an architecture
  project, not a language cleanup.
- Modules should wait until the macro-heavy include graph and generated packet
  headers have cleaner boundaries. Measure their effect against the current
  ccache/Ninja build before accepting the extra build-system complexity.
- Parallel algorithms are unsafe as a blanket optimisation over mutable
  gameplay containers. Deterministic ordering, ownership and lock scope must
  be established first.
- `std::format` is useful for typed diagnostics, but changing the logging
  surface is lower value than thread and packet safety and must be benchmarked
  on hot paths.

- Declarative packet layouts (a per-packet field table the codec is generated
  from) would be a protocol migration, not a language cleanup: the client
  repository keeps a hand-written copy of each of the 465 codecs, so a layout
  DSL on the server side alone changes nothing on the wire and doubles the
  places a field is described. The goldens, the measured-length frame and
  `WireString.h` are the pin that makes the hand-written codecs safe to keep.

### Status

The plan above is complete as of 2026-09-09. Every row of the priority table
is either done or retired with its reason written beside it, and the
`Outcome` adoption (task 3.1 in `docs/RESTRUCTURING.md`) has production
callers in all three server processes. New code keeps the conventions the
rows settled — `ManagedThread` for every worker, `de::WireScalar` and
`de::wire::readString`/`writeString` at the wire, `constexpr` factory
metadata, `std::source_location` diagnostics, `de::Mailbox`/`de::Snapshot` for
cross-thread state, `Outcome` for decisions — and adopts further C++20
facilities at focused boundaries, never as tree-wide style conversions.
