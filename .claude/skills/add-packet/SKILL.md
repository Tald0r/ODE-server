---
name: add-packet
description: Checklist for adding a packet to the DarkEden wire protocol, or changing an existing packet's layout, id or max size — every registration, generated file, golden and client-repo counterpart the tests demand.
---

# Adding or changing a packet

The wire is a contract with a separate repository. The client keeps
hand-maintained copies of every packet class, so a byte that moves here and
not there breaks live sessions with no compile error anywhere. Everything
below is enforced by something that fails; work the list in order and run
`make dev-test` (or `make test`) at the end.

Throughout, `<Name>` is the packet class, e.g. `CGMove`, and `<Name>Factory`
its factory.

## 1. The packet id

Add the enumerator to the `enum` in `src/Core/Packet.h`. **Appending is
cheap; inserting is a protocol change**: the enumerators are positional, so
a new name in the middle shifts every id after it and the client must take
the identical insertion in its own `Client/Packet/Packet.h`. The `// 53`
style comments beside the names are hand-maintained and have gone stale
before — fix the ones your change moves. The id must stay below
`Packet::PACKET_MAX`; `de::packet::validateRegistry` rejects an
out-of-range or duplicated id while compiling.

## 2. The packet class and its factory

`src/Core/<Name>.{h,cpp}` — wire layout only. `read()`, `write()`,
`getPacketID()`, `getPacketSize()`, `getPacketName()`, `toString()`, plus
the getters and setters. **No `execute()` on the packet** (ratchet R4 greps
`src/Core` for `void execute(Player`); the handler is a separate class).

The factory states its three facts as constants, which is what
`de::PacketFactoryType` in `src/Core/PacketMeta.h` requires — a factory
without them does not satisfy the concept and will not go into a
`FactoryList`:

```cpp
class <Name>Factory : public PacketFactory {
public:
    static constexpr PacketID_t kPacketID = Packet::PACKET_<NAME>;
    static constexpr std::string_view kName = "<Name>";
    static constexpr PacketSize_t kMaxSize{/* the largest body write() can emit */};
    Packet* createPacket() override { return new <Name>(); }
    PacketID_t getPacketID() const override { return kPacketID; }
    string getPacketName() const override { return string(kName); }
    PacketSize_t getPacketMaxSize() const override { return kMaxSize; }
};
```

`kName` must begin with a known link prefix (CG/GC/CL/LC/GL/LG/GS/SG/GG/GM)
— `de::packet::KnownDirection` fails compilation otherwise, naming the
factory. `kMaxSize` is a **read-buffer budget**: it must be at least what
`write()` can emit at its widest, or the receiver rejects the packet as a
protocol error and drops the connection.

Strings on the wire are a length prefix then that many bytes: use
`de::wire::readString`/`writeString` (BYTE prefix) or the `...String16`
pair (WORD prefix) from `src/Core/WireString.h`. Ratchet R9 holds the
hand-written shape at zero.

The handler class is **declared in this same header** (see the bottom of
`src/Core/CGMove.h`); there are no headers under `handler/`.

## 3. Kernel membership

Add both files to `tests/arch/kernel_files.txt`, alphabetically in the
"Packet classes and info classes" block, `.cpp` before `.h` (a convention
only: `gen_factory_list.sh` sorts the names itself). That list *is*
`de-kernel`'s source list (`src/Core/CMakeLists.txt` reads it), so there is
no separate CMake edit for the packet itself.

Kernel files must obey K1/K2: quote-include only other kernel files, and
mention no server-type macro (`__GAME_SERVER__`, `__LOGIN_SERVER__`,
`__SHARED_SERVER__`, `__GAME_CLIENT__`) and no `__COMBAT__`. If the packet
cannot comply, the game-object part of it belongs in
`src/server/gameserver/packetfill/`, not behind an `#ifdef`.
Owner: `tests/arch/check_includes.pl`, ctest `arch_includes`.

## 4. Factory registration

Add `<Name>Factory` to the right list in `src/Core/PacketFactoryManager.cpp`
— `ClientLinkFactories`, `GuildLinkFactories`, `LoginOnlyFactories` or
`GameOnlyFactories` — and the matching `#include`. Edit the lists, not an
`addFactory` sequence: `init()` is a `Concat` of them selected per server.

Then regenerate the pinned membership:

```bash
perl tests/tools/factory_registrations.pl > tests/ratchet/factory_registrations.txt
```

`tests/ratchet/ratchets.sh` diffs the committed file against a fresh run, so
an added **or dropped** registration fails until this is done deliberately.

## 5. Handler

Body in `src/server/<server>/handler/<Name>Handler.cpp`, and the file added
to that server's `CMakeLists.txt` source list. Bind it to the id at the
composition root — `src/server/gameserver/GamePacketDispatch.cpp`,
`src/server/loginserver/LoginPacketDispatch.cpp` or
`src/server/sharedserver/SharedPacketDispatch.cpp` — keeping the list
alphabetical:

```cpp
DE_REGISTER_PACKET_HANDLER(<Name>);          // static execute(<Name>*, Player*)
DE_REGISTER_PACKET_HANDLER_NOPLAYER(<Name>); // static execute(<Name>*)
DE_REGISTER_PACKET_HANDLER_FN(<Name>, fn);   // a hand-written entry point
```

Each macro static-asserts the packet against that root's
`kReceivedDirections`, so registering a handler for a link the server does
not receive is a compile error. So is a handler for a packet a narrowed
link refuses: the gameserver narrows GC to the GC-named packets its
client link admits (`src/Core/GameClientLink.h`).

A packet the client may send **before** `GPS_NORMAL` also needs its id in
the matching `PacketIDSet` in `src/Core/PacketValidator.cpp`. `GPS_NORMAL`
admits every CG packet the gameserver registers, folded from the factory
lists while compiling, so an ordinary in-game CG packet needs nothing
there. It admits nothing else: a GC-named packet the client sends the
gameserver needs its id in `kClientSentGCPacketIDs`
(`src/Core/GameClientLink.h`), without which its registration in
`GamePacketDispatch.cpp` does not compile, and a CG packet the client
sends only as a datagram goes in `kDatagramOnlyCGPacketIDs` there.
`tests/game_client_link_test.cpp` keeps its own list of the GC names and
fails until the two agree.

## 6. Generated factory list

```bash
bash tests/tools/gen_factory_list.sh
```

Rewrites `tests/generated/AllPacketFactories.inc` from
`tests/arch/kernel_files.txt`. `ratchets.sh` regenerates it into a scratch
tree and diffs, so a stale file fails. This is also what makes the packet
visible to the wire-layout inventory and to `tests/packet_meta_test.cpp`,
which folds the whole kernel into one `FactoryList`.

## 7. Golden fixture

Add the packet to the family test file it belongs to (`tests/packet_*.cpp`
— chat, combat, inventory, store, guild, party, trade, exchange, movement,
creature, skill, quest/war, session, login, zone scan, interserver,
handshake). Each
file has a `fill()` / `expectEqual()` pair per packet and a macro that
produces the three pins: golden bytes, a loopback round trip through real
sockets, and `getPacketSize()` against both the bytes `write()` emits and
the factory max.

Fixture conventions, from the existing files: values distinct per field and
`>= 128` in every byte the width allows, so a signedness flip or two
transposed fields move the golden; every exception (enumerators, bools,
text, range-limited fields) named at the point of use; a second fixture for
each `write()` branch one instance cannot reach.

Record, then review the new files as part of the diff:

```bash
UPDATE_GOLDENS=1 ./bin/wire_tests          # local build
bash tools/devbuild.sh test --record       # container build
```

Goldens land in `tests/golden/<Name>.code<N>.hex`, a variant fixture in
`tests/golden/<Name>.<variant>.code<N>.hex`. A recording run is not a
passing run — re-run without the variable.

Nothing fails automatically when a plain packet has no golden, so add it in
the same commit as the packet.

## 8. Shuffle branches, if the packet is encrypted

"Shuffled" means the packet's `read()`/`write()` call
`readEncrypt`/`writeEncrypt` and order their fields through
`SHUFFLE_STATEMENT_2`..`_5` (`src/Core/EncryptUtility.h`): the field order
on the wire depends on `code % N`, so the shuffle *is* part of the layout.

Such a packet needs goldens at codes 0..5 — 1..5 reach every `code % N`
case of every table — added through `tests/packet_encrypter_test.cpp`'s
`ENCRYPTER_PACKET_TESTS`. `ratchets.sh` looks for
`tests/golden/<Name>.code5.hex` for every `src/Core/*.cpp` that calls the
encrypter and fails without it; a deliberate omission (an abstract base with
no packet id) goes in `tests/ratchet/encrypter_exceptions.txt` with its
reason.

A packet that does *not* touch the encrypter is pinned at code 0 only, and
the family macro asserts its bytes do not vary with the code — so adopting
the encrypter later fails loudly instead of silently voiding the pin.

## 9. Wire-layout inventory, both repos

`UPDATE_GOLDENS=1 ./bin/wire_tests` also re-records `tests/wire-layout.txt`
(packet id, name, max body size, one line per factory). Review that diff:
it is the id and size half of the protocol change.

A packet on a link the client never sees (GG, GS/SG, most of GL/LG, GM) has
no client copy: list its name in `tests/wire-layout-exceptions.txt` with the
reason and skip to the cross-check. Everything the client sends or receives
takes the same change in the client repo
(`/c/Users/donjulio/Documents/git/client`):

1. Add the packet class under the directory that matches its prefix —
   `Client/Packet/Cpackets` (CG, CL), `Gpackets` (GC, GL) or `Lpackets`
   (LC, LG) — and register its factory in
   `Client/Packet/PacketFactoryManager.cpp`.
2. `perl tests/tools/gen_wire_inventory.pl` — rewrites
   `tests/generated/WireInventory.inc`; the `wire_inventory_fresh` ctest
   fails until you do.
3. Re-record the client's `tests/wire-layout.txt` with `UPDATE_GOLDENS=1`,
   and copy the golden `.hex` files so the two `tests/golden/` directories
   stay byte-identical for the packets both repos pin.

Cross-check from the server repo root:

```bash
bash tests/tools/wire_inventory_diff.sh ../client
```

It must exit 0. Do not add an exceptions line to make the diff green: a
one-sided packet that is not deliberately one-sided is a finding.

Other checks in `tests/ratchet/ratchets.sh` a new packet can trip: a
registered factory that is in neither `tests/generated/AllPacketFactories.inc`
nor `tests/ratchet/factory_exceptions.txt` fails "every registered factory
is covered by the wire inventory"; a header copied from a sibling with its
include guard fails R13; a non-ASCII byte in the new files fails R17; a
bare `throw "..."`, a `throw x.c_str()` or a `catch (const char*)` in
`read()` fails R11, R10a or R10b.

## 10. Before the PR

```bash
make dev-test                    # or: make test
bash tests/ratchet/ratchets.sh   # what ctest runs as `ratchets`
```

Format only the files you touched — never bare `make fmt`:

```bash
git diff --name-only master HEAD | grep -E '\.(cpp|h|hpp)$' | xargs clang-format -i
```

In the PR description, state the layout change in bytes and **link the
client-repo commit that ships the identical change**. A golden or inventory
diff with no counterpart link is an unreviewed protocol change.
