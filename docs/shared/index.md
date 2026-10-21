# Overview

* [Sharedserver](overview.md) - The process that holds what every game server shares, guilds above all: a heartbeat on the main thread, a worker that owns the TCP listener, and GS/SG packets.

# Rules

* [Construction and catalogues](construction-and-catalogues.md) - The server owns every manager before publishing its graph, catalogue loads prepare owned rows before publication and keep the previous table on failure, and accepted connections keep an owner through registration.
* [Guilds](guilds.md) - Guild loads prepare a complete owned guild and roster graph before publishing under the table lock, initialization checks its ID maxima, and replies own every partial record until a complete batch is prepended.
