---
type: Runbook
title: "Running the servers"
description: "Start the login, shared and game servers in that order with their conf files and arguments; the container image does it in docker/start.sh, and docker compose down drains the gameserver first within 45 seconds."
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

Each executable accepts `-f <config file>`. The loginserver also accepts `-i ID`, a signed
decimal offset added to `LoginServerBasePort`, `LoginServerBaseUDPPort` and `LoginServerBaseID`
(`src/server/ServerStartup.cpp`). Argument, file-loading and override errors return a failure
status before server initialization.

In the container image `docker/start.sh` does this; it uses `docker/conf/` rather than
`conf/`, and reads the database host out of the login configuration first. To start the servers
by hand there, set `command: ["sleep","infinity"]` on the `odk-server` service in
`docker/docker-compose.yml`, then run `./start.sh` from `/home/darkeden/vs/bin` in the container.

# Stopping

`docker compose down` stops the servers in `docker/start.sh`'s order: it sends the gameserver
SIGTERM first and keeps the login and shared processes alive until the gameserver's workers
finish (a 35-second supervisor bound backs the gameserver's own 30-second deadline), then stops
login and shared with an 8-second bound. Compose allows 45 seconds (`stop_grace_period`) before
killing the container. A deadline expiry is a failed, forced exit, not a completed drain. This
joins workers but does not introduce a full world-save operation. The keys that must
be right before anything starts are in [configuration](../database/configuration.md), and the
database they point at is set up as in [database setup](../database/overview.md).

What a process does between its arguments and its main loop, which signals stop it and the
30-second deadline that bounds a stuck shutdown are in [lifecycle rules](../startup/lifecycle-rules.md).
A gameserver whose Exchange ledger check fails starts anyway with purchases refused and says why
on stderr and in `DBError.log`. Diagnostics are off by default and switched on per process with
`DARKEDEN_TRACE=1` ([production logging](logging.md)); browser clients reach the servers through
the [WebSocket gateway](websocket-gateway.md).
