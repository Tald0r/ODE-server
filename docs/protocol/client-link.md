---
type: Rule
title: "Client link"
description: "A client's connection admits only the packets a client sends; the gameserver's in-game set is folded from its factory lists, and a handler for a packet the link refuses does not compile."
tags: [protocol, security]
enforced_by: [game_client_link_tests, login_client_link_tests, game_server_runtime_tests]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Client link

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| A client's connection admits only the packets a client sends: the gameserver's `GPS_NORMAL` set is folded from its factory lists (every CG packet but the datagram-only `CGPortCheck`, plus the GC-named packets `src/Core/GameClientLink.h` lists), and no other game or login status admits a registered packet a client does not send | `game_client_link_tests` and `login_client_link_tests` (`tests/*_client_link_test.cpp`, each compiled as its server), which check every id against the factory table's names; the gameserver's `kReceivedDirections`, whose GC link is narrowed to that list (`DirectionSet::narrowed`) and which every `DE_REGISTER_PACKET_HANDLER*` checks with `admits()` | the id, named, with the status that admits or refuses it; a GC handler registration the client link would refuse, as a compile error of `GameServerRuntime` in both `make dev-test` and `make dev-build`; `game_server_runtime_tests` also exercises that production dispatch table, while `tests/packet_meta_test.cpp` pins the narrowing mechanism and `game_client_link_tests` pins the admitted ids |

**A client's connection admits only the packets a client sends.** The gameserver's `GPS_NORMAL`
validator set is folded from its factory lists: every CG packet it registers, bar the
datagram-only `CGPortCheck`, plus the GC-named packets the live client sends, listed in
`src/Core/GameClientLink.h`. No other game or login status admits a registered packet a client
does not send.

The gameserver's `kReceivedDirections` narrows its GC link to that list
(`DirectionSet::narrowed`), and every `DE_REGISTER_PACKET_HANDLER*` checks it with `admits()`,
so a GC handler registration the client link would refuse is a compile error of
`GameServerRuntime` in both `make dev-test` and `make dev-build`. A new client-sent GC packet
needs its id in `GameClientLink.h` first.

`game_client_link_tests` and `login_client_link_tests` (`tests/*_client_link_test.cpp`, each
compiled as its server) check every id against the factory table's names and fail naming the id
and the status that admits or refuses it; `game_server_runtime_tests` exercises the production
dispatch table, and `tests/packet_meta_test.cpp` pins the narrowing mechanism. The fuzz targets'
game validator is the gameserver's, so in `GPS_NORMAL` they read only what a client sends
([fuzzing](fuzzing.md)).
