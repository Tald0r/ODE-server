---
type: Reference
title: Packet and client compatibility
description: Trace wire contracts and the domain formulas shared with the client.
tags: [server, protocol, client]
source_revision: "d116b3408efff31247ca368cc037adc77d16c76b"
generated:
  by: codex/gpt-6
  at: "2026-10-04T09:28:06Z"
sources:
  - id: guide
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/CLAUDE.md
  - id: layout
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/tests/wire_layout_test.cpp
  - id: decore
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/tests/tools/decore_client_diff.sh
  - id: fixes
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/docs/FIXES.md
---

# Packet and client compatibility

Packet classes and factories live under `src/Core/`; handlers live under
`src/server/<server>/handler/` and register in each server's packet-dispatch
composition root. CG/GC and CL/LC are client links. GL/LG, GS/SG and GG connect
server processes; GM carries server-info datagrams.[^guide]

`tests/golden/` pins serialized bytes. `tests/wire-layout.txt` and
`wire_layout_test.cpp` pin factory IDs, names and maximum body sizes. Packet
reader changes also have frame-bound and fuzz-replay owners named in
`CLAUDE.md`. For client-facing packets, check the client's separately maintained
classes before changing wire bytes or factory contracts.[^layout][^guide]

The client vendors a subset of `src/domain/`, rather than every server formula.
`src/domain/CMakeLists.txt` selects the strict-build subset; parity vectors and
`tests/tools/decore_client_diff.sh` check the shared copy. After resynchronizing
the client with its `tools/decore/sync.pl`, compare using
`bash tests/tools/decore_client_diff.sh <client-root>`. Follow the existing
coordinated-change instructions in `CLAUDE.md`.[^decore][^guide]

Known defects and their status belong in the current `docs/FIXES.md`; historical
source links here do not establish that a pending fix has merged.[^fixes]
See [builds and tests](builds-and-tests.md) for verification entry points.

[^guide]: [CLAUDE.md](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/CLAUDE.md)
[^layout]: [tests/wire_layout_test.cpp](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/tests/wire_layout_test.cpp)
[^decore]: [tests/tools/decore_client_diff.sh](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/tests/tools/decore_client_diff.sh)
[^fixes]: [docs/FIXES.md](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/docs/FIXES.md)
