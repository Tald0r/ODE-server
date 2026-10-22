---
type: Rule
title: "World and server catalogues"
description: "The common world and game-server loaders validate and own every row before replacing their catalogue, and a failed load keeps the previous rows."
tags: [game, login, shared]
enforced_by: [world_catalogue_tests, server_catalogue_tests]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# World and server catalogues

The `WorldDBInfo` and `GameServerInfo` tables are loaded by ServerCore into every process; the
loginserver's own selection over them is in [catalogues and selection](../login/catalogues-and-selection.md)
and the sharedserver's in [construction and catalogues](../shared/construction-and-catalogues.md).
The tables must agree with the `conf/` files ([configuration](../database/configuration.md)).

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| The common world loader validates and owns every row before replacing its catalogue; load failures preserve previous rows and borrowed const pointers | `world_catalogue_tests` over the production `ServerCore` loader with an explicit repository | lost data after fetch/reporting/allocation failure, leaked rows, swallowed duplicates, wrapped IDs or invalid status, stale removed worlds or failed retry |
| The common game-server loader owns rows and traversal storage and publishes only after complete base/flag preparation; failed loads retain borrowed rows and the matrix view, and clear restores an empty owner | `server_catalogue_tests` over the production `ServerCore` loader with an explicit repository | unsafe cold/partial destruction, world-zero crashes, partial dimensions/flags, wrapped IDs, uninitialized/truncated castle followers, leaked reloads, damaged traversal or failed cleanup/retry |

The seams are the two in `src/server/repository/`, `ServerInfoRepository` and
`PayPlayRepository`, compiled into all three binaries
([repository seams](../architecture/repository-seams.md)).
