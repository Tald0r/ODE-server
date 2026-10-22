---
type: Reference
title: "Configuration"
description: "The three conf files, the keys that matter most, the two connection blocks and the Exchange's point tables, and what diagnostics may and may not contain."
tags: [database, operations]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Configuration

`conf/` holds `gameserver.conf`, `loginserver.conf` and `sharedserver.conf` (the container image
uses `docker/conf/` instead, [running the servers](../operations/running-the-servers.md)). The
settings that matter most: `HomePath` (the repository directory, which must be set correctly),
`DB_HOST` (the database address) and `LoginServerIP`. The `WorldDBInfo` and `GameServerInfo`
database tables must agree with these files
([world and server catalogues](../game/world-and-server-catalogues.md)).

Code reads the loaded configuration through `de::kernelContext().config()`, which each `main()`
registers right after loading it and which asserts on a configuration nobody registered: a read
before that point is a startup-order bug ([contexts, not globals](../architecture/contexts-not-globals.md)).
Required listener ports are validated before publication ([lifecycle rules](../startup/lifecycle-rules.md)).

# Connections

Each connection is one block of keys: `DB_*` for the game database and `UI_DB_*` for the account
database, which the gameserver's second connection reads too; there is no `DIST_DB_*` block. The
Exchange's point tables (`AccountPoint`, `PointLedger`) are reached from the game connection by
the account schema's name (`UI_DB_DB`), so a purchase is one transaction on one connection: for
the Exchange both blocks must name one MySQL server, and `DB_USER` must be able to read and write
those two tables. The gameserver checks that at startup (`ExchangeService::openPointLedger`); when
it fails it starts anyway with Exchange purchases refused, and says why on stderr and in
`DBError.log`.

# What diagnostics may contain

Query diagnostics retain the operation and the numeric MySQL error code, never SQL literals or raw
driver text; invalid result access reports field bounds without the original query. Per-player
packet diagnostic files contain event, packet ID and body size, never payload descriptions. Login
character-deletion, SMS, shared broadcast failure, crash fallback and Mofus transport diagnostics
also omit private payloads. Required character and transaction audits remain separate, without
phone numbers or national IDs in Mofus records. Item and money audit records are independent of
these restrictions. Retention and severity are in [production logging](../operations/logging.md).
