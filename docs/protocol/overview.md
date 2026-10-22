---
type: Design
title: "Packet system"
description: "Packets are named for the link they ride, live in two layers (the wire class in Core, the handler body in the server), and are bound to their id at each server's composition root from constexpr factory lists."
tags: [protocol]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Purpose

Packets are the only communication between the three servers and the client. The shape below is
what lets the whole packet set compile once, macro-free, in `de-kernel` and be pinned byte for
byte against the client's copies ([wire contract](wire-contract.md)). The checklist for adding or
changing one is the `add-packet` skill (`.claude/skills/add-packet/SKILL.md`).

# Naming

Each packet is named for the link it rides, source letter first: **CG**/**GC** between client and
gameserver, **CL**/**LC** between client and loginserver, **GL**/**LG** between game and login
servers, **GS**/**SG** between game and shared servers, **GG** between gameservers, and **GM** for
the server-info datagram a gameserver sends the loginserver.

# Two layers

- `src/Core/PacketName.{h,cpp}` - the packet class (wire layout only; no `execute()`, ratchet R4
  holds that at zero), its `PacketNameFactory`, and the declaration of its handler.
- `src/server/<server>/handler/PacketNameHandler.cpp` - the handler body. There is no handler
  header: the class is declared beside the packet.

The handler is bound to its packet id at the server's composition root, `GamePacketDispatch.cpp`,
`LoginPacketDispatch.cpp` or `SharedPacketDispatch.cpp`, with `DE_REGISTER_PACKET_HANDLER(Name)`.
Each root declares the links it accepts as a `de::packet::DirectionSet` (`kReceivedDirections`),
so registering a handler for a packet on a link the server does not receive is a compile error. A
link can be narrowed to some of its packets: the gameserver's GC link admits only the GC-named
packets its client link admits ([client link](client-link.md)).

# Factories

Every `XFactory` states its packet's id, name and maximum body size as
`static constexpr kPacketID` / `kName` / `kMaxSize`; the virtual getters return them.
`src/Core/PacketMeta.h` names that contract (`de::PacketFactoryType`) and folds a pack of
factories into a `constexpr` table (`de::packet::FactoryList`) that rejects duplicate or
out-of-range ids while compiling. `PacketFactoryManager::init()` selects per server from four such
lists: edit the lists, not an `addFactory` sequence. The gameserver `Concat`s three, the
loginserver two, the sharedserver uses one as it is. Each server creates its own factory table and
validator and registers them on `de::kernelContext()`; readers call
`de::kernelContext().packetFactories()` and `.packetValidator()`. `tests/packet_meta_test.cpp`
compiles the whole kernel into one list, and a new packet without the three constants does not
satisfy the concept. [Toolchain notes](../build/toolchain.md) section 3 has the reasoning.

# What a read may do

A packet's `read()` sees exactly the body its header declares and refuses a malformed one only
with a `ProtocolException` ([frame gate](frame-gate.md)); the fuzz targets drive every read with
hostile bytes ([fuzzing](fuzzing.md)).
