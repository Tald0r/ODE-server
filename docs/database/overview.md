---
type: Guide
title: "Database setup"
description: "MySQL 5.7 or 8 with the relaxed sql_mode, utf8mb4 end to end with a session pin, the two databases and how to load them, argon2id passwords and the one-time migrations."
tags: [database, operations]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Database setup

The project requires MySQL 5.7 or 8 with `NO_ZERO_DATE` and `STRICT_TRANS_TABLES` removed from
the SQL mode:

```sql
set @@global.sql_mode = 'ONLY_FULL_GROUP_BY,NO_ZERO_IN_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';
```

Two consequences the code relies on: the mode has no `NO_BACKSLASH_ESCAPES`, so in
`MySQLExchangeRepository.cpp` `mysql_real_escape_string` never returns `(unsigned long)-1` and
the fall-through from that return to the manual escaping pass is unreachable in a correct
deployment (the manual pass itself still runs whenever the thread has no live game connection);
and it keeps `ONLY_FULL_GROUP_BY`, under which the flag war's `FlagWarStat` roll-up fails with
error 1055 (`FlagWarRepository.h` says why that statement is reached only with `ActiveFlagWar`
switched on).

# Text is UTF-8 end to end

Both databases and every table are InnoDB in `utf8mb4` / `utf8mb4_unicode_ci`
([seed-data rules](seed-data-rules.md)), the MySQL containers run
`--character-set-server=utf8mb4 --collation-server=utf8mb4_unicode_ci`, and every server pins its
session to `utf8mb4` right after connecting (`src/server/database/Connection.cpp`). The pin
matters: the 8.0 client library asks MySQL 5.7 for a collation it does not know, and the server
then silently drops the session to latin1 and passes text through as raw bytes. The dumps' legacy
EUC-KR and GBK text was re-encoded to real UTF-8 with `tools/reencode_legacy_dump.pl`, which
documents how each literal's language was decided.

# Databases and schema

`DARKEDEN` is the game database and `USERINFO` the account database. `initdb/a-setup.sql`
creates both and the `elcastle` user; the docker compose setup applies all three files
automatically:

```bash
mysql -h 127.0.0.1 -u root -p < initdb/a-setup.sql
mysql -h 127.0.0.1 -u elcastle -D 'DARKEDEN' -p < initdb/DARKEDEN.sql
mysql -h 127.0.0.1 -u elcastle -D 'USERINFO' -p < initdb/USERINFO.sql
```

# Passwords and migrations

Account passwords live in `Player.Password` as argon2id hashes. A database created before that
change needs `initdb/migrations/001-argon2-password-column.sql` run once; its plaintext rows are
rehashed by the loginserver on each account's next login. `bin/hashpw` hashes a password for a
manual `UPDATE`. The later migrations are one-time too: `002-war-schedule-castle-war-kind.sql`
(the `CastleWarKind` column) and `003-castle-tax-balance-signed.sql` (the castle tax balance saved
as a signed relative change; on the old unsigned column an owner change can fail whole), and
`004-english-content.sql` (the English content text of the seed tables, written by the scripts in
`tools/i18n/`, which leaves accounts, characters and items alone). Nothing checks them at startup;
run each once on a database created before it.

All SQL is reached through repository seams ([repository seams](../architecture/repository-seams.md)),
and `make integration-test` runs those implementations against a throwaway MySQL 5.7 with this
schema ([testing overview](../testing/overview.md)). The connection settings are in
[configuration](configuration.md).
