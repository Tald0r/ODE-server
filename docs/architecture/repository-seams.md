---
type: Rule
title: "Repository seams"
description: "Every SQL statement lives behind a *Repository interface with a MySQL implementation under a repository/ directory, reached through a default accessor, and the header is the authority on its tables."
tags: [architecture, database]
enforced_by: [ratchets, "make integration-test"]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Repository seams

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| No `executeQuery` outside `src/server/database/` and the `repository/` directories | ratchets R2/R3 | R2/R3 above 0 |
| Repository SQL behaves against a real MySQL | `make integration-test` (`tests/integration/`, needs docker) | the failing statement |

**All SQL lives behind repository seams.** A seam is an interface `*Repository.h` with a
`MySQL*Repository.cpp` implementation under a `repository/` directory, reached through a
`default*Repository()` accessor, never a `g_p*` extern. Ratchets R2 and R3 fail on an
`executeQuery` written anywhere but `src/server/database/` and a `repository/` directory.

There are 44 seams: 36 under `src/server/gameserver/repository/`, two in ServerCore
(`src/server/repository/`, compiled into all three binaries), four in the loginserver and two in
the sharedserver, the last two sets prefixed `Login` and `Shared` because the integration binary
links every implementation.

- **The header is the authority** on its tables' quirks. Where SQL on the same tables lives
  outside the seam, the header carries an explicit `Not enclosed` list of it (nine headers do;
  grep for the phrase). Read it before adding a method.
- **Statements move byte-for-byte**, quirks included, and use the parameterized `executeQuery`
  form with each argument's own conversion: `%u` for DWORD/WORD/BYTE, `%d` for int, `%ld` for
  `time_t`.
- **Two races sharing a row shape share a record**, and the seam chooses the table:
  `CharacterExpsRecord` with `saveExps(ownerName, race, record)` over `characterRaceTable()`
  (`src/server/gameserver/repository/CharacterRace.h`), which `characterRaceOf()`
  (`src/server/gameserver/PlayerRace.h`) maps the game's `Race_t` onto.
- **Diagnostics carry no SQL.** Query diagnostics retain the operation and the numeric MySQL error
  code, never SQL literals or raw driver text; invalid result access reports field bounds without
  the original query ([configuration](../database/configuration.md)).

`make integration-test` runs the repository implementations against a real MySQL
([testing overview](../testing/overview.md)).
