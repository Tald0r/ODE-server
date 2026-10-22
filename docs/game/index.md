# Overview

* [Gameserver](overview.md) - The gameplay process: a composition root per packet, handler, fill, repository and module directories, zone-group threads, and the Exchange's startup check.

# Rules

* [Connections and players](connections-and-players.md) - A game connection keeps an owner through authorization and registration, a player handoff keeps the source until the destination accepts it, and queued broadcasts own their bytes.
* [World and server catalogues](world-and-server-catalogues.md) - The common world and game-server loaders validate and own every row before replacing their catalogue, and a failed load keeps the previous rows.
