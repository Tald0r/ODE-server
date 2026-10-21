# Overview

* [Packet system](overview.md) - Packets are named for the link they ride, live in two layers (the wire class in Core, the handler body in the server), and are bound to their id at each server's composition root from constexpr factory lists.

# Guides

* [Packet-read fuzzing](fuzzing.md) - Two libFuzzer targets mirror the game and login receive loops up to read(), deliver every input three ways, and replay their corpus and regressions in ctest.

# Rules

* [Client link](client-link.md) - A client's connection admits only the packets a client sends; the gameserver's in-game set is folded from its factory lists, and a handler for a packet the link refuses does not compile.
* [De-core parity](decore-parity.md) - A change to src/domain is done only when the client's byte-identical copy is resynced and the diff is clean; a changed vector row is a balance change.
* [Factory registrations](factory-registrations.md) - Each server registers exactly the factories it registered before, and the generated factory list matches the kernel membership.
* [Frame gate](frame-gate.md) - A packet's read() sees exactly the body its header declares, and the gameserver checks the sequence byte only where the frame is consumed.
* [Wire contract](wire-contract.md) - A packet's bytes never change silently: goldens pin them, the inventory pins every factory's id, name and size, and a diff is a protocol change the client ships identically.
