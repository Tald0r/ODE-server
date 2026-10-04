# Browser and native WebSocket connections

The gateway carries the existing binary TCP byte stream in binary WebSocket
messages. Packet layouts, encryption, authentication and login/world handoffs
are unchanged. Native TCP remains available. Clients select a gateway URL and
send the original advertised host and port as routing parameters; the gateway
only accepts routes configured by the operator.

The gateway is a small Rust program in `src/server/websocketproxyserver` (tokio and
tungstenite, no C or TLS dependencies).

## Local setup

Add a different private listener port to each server's configuration:

```ini
# loginserver.conf
GatewayProxyPort : 19099

# gameserver.conf (choose a distinct port for every game process)
GatewayProxyPort : 19098
```

These listeners bind **127.0.0.1 only**. Run the gateway in the same network
namespace as the C++ processes. It sends a PROXY v1 header with the original
IPv4 peer before any game bytes. The C++ listener validates and consumes that
header before admitting a player, preserving the IP identity used by login and
world-transfer records. Never expose a forwarded-identity listener publicly.
Other local processes share this trust boundary.

Install a stable Rust toolchain (for example with `rustup`), then:

```sh
cd src/server/websocketproxyserver
cp config.example.json config.json
cargo run --release -- config.json
```

The gateway prints `DarkEden gateway listening on 127.0.0.1:8080` once it is
bound and runs until ctrl-c or SIGTERM. `gateway` with no argument reads
`config.json` from the working directory. Diagnostics go to stderr; set
`RUST_LOG=debug` to log every refused handshake and every connection.
Backend-unavailable warnings are limited to one per configured local backend
port every 30 seconds, including the number of failures suppressed since the
previous report. Listener-accept failures have an independent budget. A
successful operation after an emitted warning produces one recovery event;
recovery does not reset the warning budget. Remaining suppressed counts are
reported when the gateway shuts down. These limits affect diagnostics only:
unavailable backends still refuse each handshake with HTTP 502.

The configuration keys:

| Key | Default | Meaning |
|-----|---------|---------|
| `bind` | `"127.0.0.1"` | Listen address (IP or host name) |
| `port` | `8080` | Listen port |
| `origins` | required | Non-empty array of exact browser page origins |
| `allowNative` | `false` | Admit clients whose Origin header is absent or empty |
| `trustedProxies` | `[]` | Reverse-proxy peer IPs whose `X-Forwarded-For` is used |
| `maxConnections` | `2000` | Open TCP connections, handshakes included (1 to 100000) |
| `routes` | required | Advertised `host:port` to local `GatewayProxyPort` |

An invalid configuration stops the gateway at startup with a message naming
the offending key.

Set `routes` to **every advertised host:port** in the client configuration,
login replies, game transfers and return-to-login replies. Each value is that
process's `GatewayProxyPort`, not its public TCP port. Unknown routes fail
closed; the gateway cannot connect to arbitrary destinations. The example maps
`127.0.0.1:9999` to the login process and `127.0.0.1:9998` to the game process;
adjust these to the deployment's actual settings.

Set the browser client's `client-config.json` `websocketUrl` to
`ws://127.0.0.1:8080/game` for local development. Its `loginHost` and `loginPort`
must name a configured route. On native clients set `DARKEDEN_WEBSOCKET_URL`
to the same URL; leaving it unset uses TCP. Optional `DARKEDEN_LOGIN_HOST` and
`DARKEDEN_LOGIN_PORT` override the initial login endpoint.

## The handshake

A client opens `ws://<gateway>/game?host=<advertised host>&port=<advertised port>`
offering the `binary` subprotocol. The gateway answers:

| Request | Answer |
|---------|--------|
| Not a WebSocket upgrade | `200 text/plain` "DarkEden WebSocket gateway" |
| Path other than `/game`, or anything but exactly one `host` and one `port` parameter | `404` |
| `host:port` not in `routes` | `403` |
| Non-empty `Origin` not in `origins` | `403` |
| Absent or empty `Origin` without `allowNative` | `403` |
| Client address not IPv4, or bad `X-Forwarded-For` from a trusted proxy | `400` |
| Not `GET` | `405` |
| Invalid WebSocket headers, or `binary` not offered | `400` |
| Backend not reachable within 5 seconds | `502` |
| Admitted | `101`, selecting `binary` |

The native client sends `Origin: ` with an empty value on purpose; that is
treated exactly like a missing Origin. The gateway connects to
`127.0.0.1:<GatewayProxyPort>` and writes
`PROXY TCP4 <client IP> 127.0.0.1 <client source port> <GatewayProxyPort>\r\n`
**before** it writes the `101` response, so a dead backend fails the
handshake instead of opening a WebSocket that closes at once.

## TLS and reverse proxies

Production browser pages use HTTPS and a `wss://` endpoint. Terminate TLS at a
reverse proxy, retaining the gateway's loopback bind. Configure exact allowed
page origins; wildcards are not supported. `allowNative` permits clients that
omit the browser Origin header. This is origin validation, not authentication:
accounts are still authenticated by the game server.

When using a reverse proxy, list its actual peer address in `trustedProxies`.
It must **overwrite**, not append to, `X-Forwarded-For` with one verified IPv4
address; a list, a repeated header or a missing header from a trusted proxy is
refused with `400`. Untrusted peers' forwarding headers are ignored. An example
nginx location, with nginx and the gateway on the same host:

```nginx
location = /game {
    proxy_pass http://127.0.0.1:8080;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";
    proxy_set_header X-Forwarded-For $remote_addr;
    proxy_set_header Host $host;
    proxy_read_timeout 75s;
}
```

Use `trustedProxies: ["127.0.0.1"]` with this configuration. Public client
addresses must be IPv4 because the existing game protocol and handoff tables
use IPv4. IPv6-only clients are rejected rather than assigned a shared identity
(IPv4-mapped IPv6 peers count as IPv4). The existing game's limitations for
multiple accounts behind one NAT remain.

## Docker

Build `src/server/websocketproxyserver/Dockerfile` using `src/server/websocketproxyserver` as its context and
mount the configuration at `/config/config.json`:

```sh
docker build -t darkeden-gateway src/server/websocketproxyserver
docker run --rm -v "$PWD/src/server/websocketproxyserver/config.json:/config/config.json:ro" darkeden-gateway
```

The image is a two-stage build (`rust:1-bookworm`, then `debian:bookworm-slim`)
running `gateway /config/config.json` as an unprivileged user; it also carries
the `client-fixture` binary. Use Compose `network_mode: "service:server"` to
share the C++ server's namespace. A reverse proxy in that namespace can keep
the gateway bound to loopback; otherwise bind the gateway to the container's
private interface and restrict access to the reverse proxy. Do not publish the
`GatewayProxyPort` ports.

`docker/docker-compose.yml` does all of this: the `odk-websocket` service
builds this directory, runs in `odk-server`'s namespace with
`docker/websocket.json` as its configuration, and `odk-server` publishes it on
`127.0.0.1:8080`. `docker/conf/` already carries the two `GatewayProxyPort`
settings. Serve the browser client on `http://127.0.0.1:18739` (or
`localhost:18739`), the origins that configuration admits, with
`websocketUrl` set to `ws://127.0.0.1:8080/game`.

## Limits

- WebSocket messages and frames are limited to 1 MiB; a larger one closes the
  connection with 1009. Text messages close it with 1003. Compression
  (permessage-deflate) is never negotiated.
- Each direction writes one message or TCP chunk and waits for the write to
  finish before reading more, so a slow client or backend applies
  backpressure to the other side instead of growing a queue.
- At most `maxConnections` TCP connections are open at once, counting those
  still in the handshake; excess connections are closed unanswered. Each
  admitted client also costs a backend descriptor, so the process needs a
  descriptor limit above twice `maxConnections` (the Docker image has one;
  a bare host may default to 1024). An idle connection holds about 40 KiB;
  a client that announces a 1 MiB frame reserves 1 MiB until the frame
  arrives or the ping deadline drops it, so budget `maxConnections` MiB for
  the worst case.
- The HTTP request head is limited to 16 KiB and 64 headers, and the whole
  handshake, backend connection included, must finish within 10 seconds.
- The gateway pings every 30 seconds and disconnects a client that has not
  answered the previous ping, or that has not accepted a message within one
  ping interval (a client that stops reading cannot hold a session open).
- When the backend closes, the client gets close code 1000; when the client
  closes, the backend connection is dropped. On shutdown clients get 1001 and
  have five seconds to finish closing.

The C++ listener caps unfinished headers at 128 and expires them after five
seconds without blocking the player loop.

## Verification

`make dev-test` includes real-socket tests for split headers, byte preservation,
distinct client identities, malformed headers, premature close and timeout.
`cd src/server/websocketproxyserver && cargo test` exercises binary fragmentation, handoff
routing, origin and destination rejection, forwarding trust, oversized frames,
text rejection, the subprotocol requirement, the connection cap, handshake and
ping timeouts, shutdown and configuration validation. CI also runs
`cargo fmt --check` and `cargo clippy --all-targets -- -D warnings`.

`cargo run --release --bin client-fixture` starts controlled endpoints for the
client repository's native `transport_tests` and browser
`browser-tests transport` probes (`tools/web/browser-tests`): a gateway on `ws://127.0.0.1:18740/game`
in front of an echo backend, and a text-frame endpoint on
`ws://127.0.0.1:18741/game`. `--bind 0.0.0.0` moves both WebSocket listeners
for use inside a container.
