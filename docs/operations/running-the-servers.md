---
type: Runbook
title: "Running the servers"
description: "Start the login, shared and game servers in that order with their conf files; the container image does it in docker/start.sh with its own conf directory."
tags: [operations]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Running the servers

Start the servers in this order:

```bash
./bin/loginserver -f ./conf/loginserver.conf
./bin/sharedserver -f ./conf/sharedserver.conf
./bin/gameserver -f ./conf/gameserver.conf
```

In the container image `docker/start.sh` does this; it uses `docker/conf/` rather than
`conf/`, and reads the database host out of the login configuration first. The keys that must
be right before anything starts are in [configuration](../database/configuration.md), and the
database they point at is set up as in [database setup](../database/overview.md).

What a process does between its arguments and its main loop, which signals stop it and the
30-second deadline that bounds a stuck shutdown are in [lifecycle rules](../startup/lifecycle-rules.md).
A gameserver whose Exchange ledger check fails starts anyway with purchases refused and says why
on stderr and in `DBError.log`. Diagnostics are off by default and switched on per process with
`DARKEDEN_TRACE=1` ([production logging](logging.md)); browser clients reach the servers through
the [WebSocket gateway](websocket-gateway.md).
