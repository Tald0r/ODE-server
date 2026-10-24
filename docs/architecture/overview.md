---
type: Design
title: "Server architecture"
description: "Three cooperating processes, each an executable of one main.cpp over a runtime object library, built on a macro-free kernel, pure domain formulas and a shared ServerCore."
tags: [architecture]
enforced_by: [game_server_runtime_tests, login_server_runtime_tests, shared_server_runtime_tests]
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

# Rules

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| Server implementations link without `main.cpp`, with the same definitions as production | `game_server_runtime_tests`, `login_server_runtime_tests`, `shared_server_runtime_tests` | duplicate `main` at link time, a mismatched server definition, or a production dispatch/handler test failure |

# Key directories

```
src/
├── Core/                      # de-kernel: packets + shared utilities, no server-type dependencies
│   ├── [GC|CG|CL|LC|GL|LG|GS|SG|GG]*.{h,cpp}   # Protocol packet classes, directly in Core/
│   ├── [core utilities]       # Socket, datagram, player info, items, skills, etc.
│   └── CMakeLists.txt         # de-kernel, Core, and the per-server packet libraries
├── domain/                    # de-core: pure formula functions (Formulas, SkillOutputFormulas, SkillRange,
│                              #   ItemPrice, ItemDurability, ItemGrade, ItemClass, EquipRequirement, and the
│                              #   server-only PartyExp and DarkLight), freestanding; vectors/ holds the parity
│                              #   rows the client asserts too, vectors/server/ the rows of the server-only ones
├── server/
│   ├── Thread.h, ManagedThread.h  # the worker-thread base (CooperativeThread.h is reached only through ManagedThread)
│   ├── Mailbox.h, Snapshot.h  # cross-thread command queue, copy-on-write tables
│   ├── ServerContext.h        # registry for the managers ServerCore defines (database, server and world tables)
│   ├── database/              # Database abstraction layer and connection management
│   ├── repository/            # ServerCore persistence seams, linked into all three binaries
│   ├── gameserver/            # Main game server executable
│   │   ├── GamePacketDispatch.cpp   # composition root: packet id -> handler
│   │   ├── handler/           # CG/GC/LG/GG/SG packet handler bodies
│   │   ├── packetfill/        # Server-side packet fill helpers moved out of Core
│   │   ├── repository/        # Persistence seams: *Repository.h interfaces + MySQL*Repository.cpp impls
│   │   ├── gm/                # GM and console command bodies behind a router
│   │   ├── skill/, item/, war/, quest/   # Skill, item, war and (Lua-scripted) quest modules
│   │   ├── guild/, party/, trade/, couple/, exchange/   # Social and trading decisions
│   │   ├── ctf/, mission/, mofus/        # Capture the flag, missions, game events
│   ├── loginserver/           # Login server executable (CL/GL/GM handlers and one CG, its own repository/)
│   ├── sharedserver/          # Shared server executable (GS handlers, its own repository/)
│   └── websocketproxyserver/  # the Rust WebSocket gateway
tests/
├── arch/                      # kernel_files.txt (de-kernel's source list), check_includes.pl, baseline.txt
├── ratchet/                   # ratchets.sh and the files it pins
├── tools/                     # generators and audits ctest runs
├── fuzz/                      # packet-read fuzz targets, replay driver, regressions/
└── golden/, generated/, support/, integration/  # fixtures, generated lists, fakes, MySQL tier
docs/
├── index.md                   # the knowledge bundle (OKF): one concept per rule, in folders by area
├── reviews/                   # RESTRUCTURING.md (the living plan, the R1-R19 table) and FIXES.md (the fix log)
└── build/toolchain.md         # Zig/Clang, C++20 adoption, wire and diagnostics contracts
third_party/
├── tinyxml2/                  # Vendored XML parser (10.0.0), replaces xerces-c
└── argon2/                    # Vendored argon2 reference implementation (C API)
.claude/skills/add-packet/     # checklist for adding or changing a packet
```

# Game concepts

**Races** are Slayer (human vampire hunters), Vampire and Ousters, one `PlayerCreature` subclass
each; a **Zone** is a geographic area where players exist, it belongs to a **ZoneGroup**, and a
**DynamicZone** is instanced content (dungeons, for example); **Creature** is the base class of
every entity, players, monsters and NPCs alike; **Effects** are time-based state applied to a
creature; **Skills** are the combat and utility abilities; **Guild** and **Party** are the social
groupings. Deep inheritance is the house style
(Creature -> PlayerCreature -> Slayer/Vampire/Ousters), and Lua drives the quest system
(`quest/luaScript/`).
