---
type: Design
title: "Server architecture"
description: "Three cooperating processes, each an executable of one main.cpp over a runtime object library, built on a macro-free kernel, pure domain formulas and a shared ServerCore."
tags: [architecture]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Purpose

The DarkEden server is one game split into three processes that cooperate over the wire:

1. **loginserver** - authentication and character selection ([login](../login/overview.md));
2. **sharedserver** - the data every game server shares, guilds above all
   ([shared](../shared/overview.md));
3. **gameserver** - the game itself, one process per world or zone group
   ([game](../game/overview.md)).

Start them in that order ([running the servers](../operations/running-the-servers.md)).

# Moving parts

- **Executables and runtimes.** `GameServerRuntime`, `LoginServerRuntime` and
  `SharedServerRuntime` are CMake OBJECT libraries holding the production implementation
  objects. Each executable compiles only its `main.cpp` and links its runtime; the runtime tests
  (`game_server_runtime_tests`, `login_server_runtime_tests`, `shared_server_runtime_tests`)
  link the same objects without process startup. Link a runtime directly with
  `target_link_libraries` to inherit its server definitions, headers and dependencies. Runtimes
  stay in separate executables: several classes share names across servers and packet
  registration is process-wide.
- **Libraries, by layer** ([module layering](module-layering.md)):
  `de-kernel` (`src/Core/`, every packet class and the shared utilities, compiled once with no
  server-type macro); `de-core` (`src/domain/`, pure formulas the client vendors a subset of);
  `ServerCore` (`src/server/`, the common managers and repository implementations all three
  binaries link); and beside it the startup, lifecycle, listener, socket and worker libraries
  each process composes its `main()` from ([startup](../startup/overview.md)).
- **Contexts.** Every process-wide manager is reached through the context of its layer:
  `de::kernelContext()`, `de::serverContext()`, `de::gameContext()`, `de::loginContext()`
  or `de::sharedContext()` ([contexts, not globals](contexts-not-globals.md)).
- **Build.** CMake is the build system and the `Makefile` wraps it; the `Makefile`s under
  `src/` are legacy. No target globs its sources: every `.cpp` is named, which is what lets a
  ratchet call a source no target compiles dead ([dead sources](dead-sources-and-encoding.md)).

# Key directories

```
src/
├── Core/                      # de-kernel: packets + shared utilities, no server-type dependencies
│   ├── [GC|CG|CL|LC|GL|LG|GS|SG|GG]*.{h,cpp}   # Protocol packet classes, directly in Core/
│   ├── [core utilities]       # Socket, datagram, player info, items, skills, etc.
│   └── CMakeLists.txt         # de-kernel, Core, and the per-server packet libraries
├── domain/                    # de-core: pure formula functions; vectors/ holds the parity rows
├── server/
│   ├── Thread.h, ManagedThread.h  # the worker-thread base
│   ├── Mailbox.h, Snapshot.h  # cross-thread command queue, copy-on-write tables
│   ├── ServerContext.h        # registry for the managers ServerCore defines
│   ├── database/              # Database abstraction layer and connection management
│   ├── repository/            # ServerCore persistence seams, linked into all three binaries
│   ├── gameserver/            # Main game server executable
│   │   ├── GamePacketDispatch.cpp   # composition root: packet id -> handler
│   │   ├── handler/           # CG/GC/LG/GG/SG packet handler bodies
│   │   ├── packetfill/        # Server-side packet fill helpers moved out of Core
│   │   ├── repository/        # *Repository.h interfaces + MySQL*Repository.cpp impls
│   │   ├── gm/                # GM and console command bodies behind a router
│   │   ├── skill/, item/, war/, quest/   # Skill, item, war and (Lua-scripted) quest modules
│   │   ├── guild/, party/, trade/, couple/, exchange/   # Social and trading decisions
│   │   ├── ctf/, mission/, mofus/        # Capture the flag, missions, game events
│   ├── loginserver/           # Login server executable (CL/GL/GM handlers and one CG)
│   ├── sharedserver/          # Shared server executable (GS handlers)
│   └── websocketproxyserver/  # the Rust WebSocket gateway
tests/
├── arch/                      # kernel_files.txt, check_includes.pl, baseline.txt
├── ratchet/                   # ratchets.sh and the files it pins
├── tools/                     # generators and audits ctest runs
├── fuzz/                      # packet-read fuzz targets, replay driver, regressions/
└── golden/, generated/, support/, integration/  # fixtures, generated lists, fakes, MySQL tier
third_party/
├── tinyxml2/                  # Vendored XML parser (10.0.0), replaces xerces-c
└── argon2/                    # Vendored argon2 reference implementation (C API)
.claude/skills/add-packet/     # checklist for adding or changing a packet
```

# Game concepts

**Races** are Slayer, Vampire and Ousters, one `PlayerCreature` subclass each; a **Zone** belongs
to a **ZoneGroup** and a **DynamicZone** is instanced content; **Creature** is the base of
players, monsters and NPCs; **Effects** are timed state on a creature; **Skills** are the
abilities; **Guild** and **Party** are the social groupings. Deep inheritance is the house style
(Creature -> PlayerCreature -> Slayer/Vampire/Ousters), and Lua drives the quest system
(`quest/luaScript/`).
