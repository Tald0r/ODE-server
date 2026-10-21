# Guides

* [WebSocket gateway](websocket-gateway.md) - The Rust gateway that carries the binary TCP stream in WebSocket messages for browser and native clients: setup, the handshake, TLS, Docker, limits and verification.

# Runbooks

* [Production logging](logging.md) - DARKEDEN_TRACE is off by default and turns on the migrated diagnostics; the severity policy for new events and how a deployment migrates its logging.
* [Running the servers](running-the-servers.md) - Start the login, shared and game servers in that order with their conf files; the container image does it in docker/start.sh with its own conf directory.
