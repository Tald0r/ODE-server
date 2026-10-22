---
type: Reference
title: "Dependencies"
description: "libmysqlclient, Lua 5.1 or LuaJIT and zlib from the system; tinyxml2 and argon2 vendored; every one a C API, which the Zig toolchain depends on."
tags: [build]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Dependencies

Required libraries, all exposing a C API ([toolchain notes](toolchain.md) says why
re-introducing a C++-API dependency is a problem under the Zig toolchain):

- **libmysqlclient-dev** - the MySQL client library (Ubuntu 20.04 ships 8.0 / `libmysqlclient21`;
  the server talks to MySQL 5.7 or 8, [database setup](../database/overview.md));
- **lua5.1-dev** or **luajit** - Lua scripting, used by the quest system;
- **zlib**.

Install on Ubuntu/Debian:

```bash
sudo apt install libmysqlclient-dev liblua5.1-dev zlib1g-dev
```

XML parsing uses the vendored **tinyxml2** (10.0.0) in `third_party/tinyxml2`, wrapped by `SXml`
in Core; xerces-c is no longer needed (toolchain notes, section 1). Password hashing uses the
vendored **argon2** reference implementation (20190702, C API) in `third_party/argon2`; nothing to
install for it. The WebSocket gateway is a Rust program with its own toolchain
([WebSocket gateway](../operations/websocket-gateway.md)).
