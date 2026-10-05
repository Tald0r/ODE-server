# Local browser client

Open <http://127.0.0.1:18739>, choose **Load game**, then **Play**.

Verified locally: browser login, character selection, game entry, movement and
disconnect. The server repairs for Debug startup and character login (#272),
the client's audio path fix (#276), the Rust WebSocket gateway (#273) and the
Rust browser test harness (#277) are all merged on master.

Two compose stacks share one network:

1. This directory: `docker compose up -d --build` starts MySQL, the three game
   servers and the WebSocket gateway (`odk-websocket`, built from
   `../src/server/websocketproxyserver`, configured by `websocket.json`, published
   on `127.0.0.1:8080`). `docker-compose.override.yml` here only switches the
   server to a Debug build.
2. The client repository's `docker/docker-compose.yml`: `docker compose up -d
   --build` there builds the WebAssembly client into an nginx image and runs it
   as `odk-web` on this stack's network (`docker_odk-network`), publishing
   `127.0.0.1:18739`. nginx proxies `/game` to the gateway, so the page needs no
   separate gateway URL, and `client-config.json` is rendered from the container's
   environment (`DARKEDEN_LOGIN_HOST`/`PORT`, default `127.0.0.1:9999`). The asset
   pack is mounted from `DARKEDEN_ASSETS` (default `../build/web/assets` in that
   repository).

The gateway shares the game server's network namespace to reach its loopback
proxy listeners on ports 19099 and 19098 (`GatewayProxyPort` in
`conf/loginserver.conf` and `conf/gameserver.conf`). Through the nginx proxy the
gateway sees the web container as its peer; list that container's address in
`websocket.json`'s `trustedProxies` when players must keep their own IP identity.

The existing database uses the `docker_odk-mysql-data` volume. It was backed up
before applying the repository's migrations 002 and 003; the backup is
`../build/webgl-local/database-before-webgl-20260925-064227.sql`.

After changing server sources, rebuild with `docker compose build odk-server`
and recreate both the server and its shared-network gateway together, since
the gateway's namespace belongs to the server container:

```powershell
docker compose up -d --no-deps --force-recreate odk-server odk-websocket
```

## Diagnostic storage

`docker compose logs` receives prefixed server output directly; `start.sh` no
longer creates duplicate timestamped `.out` files. The Compose stack uses the
Docker `local` logging driver with `max-size: 10m` and `max-file: 3` for the game
server, database and gateway. Change these deployment options deliberately if
a collector or retention policy requires different bounds. Docker's default
blocking delivery can stall a producer if its log sink stops accepting data;
the forwarder drain timeout does not remove that backpressure.

C++ destinations explicitly classified as diagnostic-only use `diagnosticFilelog`
and rotate at 10 MiB, retaining two backups (`.1` and `.2`). These include packet
and packet-size diagnostics, general exception logs and selected manager/thread/
movement errors. Legacy `filelog` remains append-only: item, reward, billing,
security and mixed destinations retain their existing history and still require
a separately agreed retention policy. Rotation is selected at the call site,
never inferred from a filename; all writers to one destination must agree.

Stable `.lock` sidecars coordinate both APIs across processes; do not delete or
rotate those lock files while servers run. Existing oversized diagnostic logs
are preserved as they rotate and age out through retention, rather than being
truncated on upgrade. Bounds apply per diagnostic filename: per-player packet
files still require lifecycle-based directory cleanup if many distinct players
are explicitly traced. Database item/money audit records are unchanged.

Both APIs visibly mark oversized individual messages with `[truncated]` and
contain formatting and sink failures. Failed writes are reported on stderr once
immediately and at most once per minute thereafter, with suppressed failure
counts. This is a log-loss signal, not a successful persistence acknowledgement;
investigate disk capacity or permissions.
