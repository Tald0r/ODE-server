---
type: Rule
title: "Module layering"
description: "de-kernel includes only kernel files and no server macro, de-core includes only itself, ServerCore pulls in no MySQL, Lua or socket transport, and the include checker names the offending edge."
tags: [architecture]
enforced_by: [arch_includes, ratchets]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Module layering

Four layers, each a strictly lower one than the next:

| Layer | Where to start | Responsibility |
|---|---|---|
| `de-kernel` | `tests/arch/kernel_files.txt`, `src/Core/` | every packet class and the shared utilities, compiled once without a server-type macro |
| `de-core` | `src/domain/` | pure formulas, independent of transport, database and server target |
| `ServerCore` | `src/server/CMakeLists.txt` | the common managers and repository implementations; the startup, worker and lifecycle helpers have their own targets |
| the runtimes | `src/server/{game,login,shared}server/CMakeLists.txt` | process-specific handlers and implementation objects |

`tests/arch/check_includes.pl` (ctest `arch_includes`) walks the include graph and fails, naming
the include or macro, on:

- **K1/K2/K3** - a kernel file that includes a non-kernel file, mentions a server-type macro or
  `__COMBAT__`. The kernel's membership is `tests/arch/kernel_files.txt`, and `TestPackets` is
  only the define-free factory/validator trio on top of it, so the wire layer stays buildable,
  and identical, alone. (The client's configuration, which defines `__GAME_CLIENT__=1`, is a
  different build.)
- **C1** - a core file that includes a MySQL, Lua or socket-transport header. The baseline in
  `tests/arch/baseline.txt` is shrink-only.
- **D1** - a `src/domain/` file that quote-includes anything but an existing `"domain/X.h"` or
  angle-includes anything but `<algorithm>` and `<cmath>`. That is what keeps the subset the
  client vendors compilable in both repositories ([de-core parity](../protocol/decore-parity.md)).

The ratchets hold the rest of the layering: no `executeQuery` outside the repository seams
([repository seams](repository-seams.md)), no packet with an `execute()` body (R4,
[protocol overview](../protocol/overview.md)), no `g_p*` extern
([contexts, not globals](contexts-not-globals.md)).
