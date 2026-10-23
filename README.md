# opendarkeden-server

The server for **DarkEden**, an MMORPG, written in C++20 and split into three cooperating
processes:

- **loginserver** - authentication, world and character selection;
- **sharedserver** - guilds and the other data every game server shares;
- **gameserver** - the game itself, one process per world or zone group.

This guide takes a clone to running servers. How the code is organised, and the rules it follows
with the incidents behind them, live in the knowledge bundle under `docs/` (Google's Open
Knowledge Format): start at [docs/index.md](docs/index.md);
[docs/knowledge-bundle.md](docs/knowledge-bundle.md) says how it is kept, and
[CLAUDE.md](CLAUDE.md) is the index AI-assisted sessions read first.

Browser and native WebSocket clients can use the optional
[WebSocket gateway](docs/operations/websocket-gateway.md), which preserves player IPs across
login and game handoffs while retaining the existing TCP listeners.

## Prerequisites

- **To run the stack:** Docker with Compose. On macOS, OrbStack (verified); Docker Desktop should
  work too.
- **To build locally:** CMake and a C++20 compiler *and* standard library. The pinned Zig 0.16.0
  (Clang 21.1.0) container is the reference; Ubuntu 20.04's GCC 9 is too old, and CMake checks
  for `jthread`, `stop_token` and stop-aware condition-variable waits at configure time
  ([toolchain notes](docs/build/toolchain.md)). The libraries are the MySQL client, Lua 5.1 or
  LuaJIT, and zlib; XML parsing (tinyxml2) and password hashing (argon2) are vendored
  ([dependencies](docs/build/dependencies.md)).
- **To run the servers outside Compose:** MySQL 5.7 or 8.

## Quick start (docker compose)

Everything below builds the server **from the sources in this repository** - no pre-built image
is downloaded.

```sh
cd docker
docker compose up -d --build
```

That command:

1. builds `../Dockerfile`, which compiles `loginserver`, `sharedserver` and `gameserver` as
   C++20 with pinned Zig 0.16.0/Clang 21.1.0, then packages the binaries in an Ubuntu 20.04
   runtime together with `data/` and `docker/conf/`;
2. starts MySQL 5.7 and imports `initdb/*.sql` on first run;
3. applies `docker/initdb-docker.sql`, which points `DARKEDEN.WorldDBInfo` and
   `DARKEDEN.GameServerInfo` at this stack (the dumps ship with the original developers' LAN
   addresses);
4. starts the three servers in order once the database is ready (see `docker/start.sh`);
5. builds `src/server/websocketproxyserver` and starts the WebSocket gateway (`odk-websocket`) in
   the game servers' network namespace, published on `127.0.0.1:8080` for browser and native
   WebSocket clients (`docker/websocket.json`, see the
   [WebSocket gateway](docs/operations/websocket-gateway.md)).

Follow the logs:

```sh
docker compose logs -f odk-server
```

**macOS:** the same commands work under OrbStack (what this was verified with) and should under
Docker Desktop. On Apple Silicon the server images build natively for arm64, but
`mysql/mysql-server:5.7` is published for amd64 only, so Docker runs it under emulation and warns
that `The requested image's platform (linux/amd64) does not match the detected host platform`.
That warning is expected, and the stack comes up as it does on Linux.

**NOTE:** the compose setup assumes server and client run on the same machine. To run the client
on another machine, set the server IP in the `DARKEDEN.GameServerInfo` table (or in
`docker/initdb-docker.sql` before the first start) and restart the server container.

### Rebuild after changing the code

```sh
cd docker
docker compose up -d --build
```

The image uses C++20 and defaults to `CMAKE_BUILD_TYPE=Release`. The compose build arguments can
select Debug:

```sh
BUILD_TYPE=Debug docker compose up -d --build
```

### Stop

Stop everything (the database keeps its data in the `odk-mysql-data` volume):

```sh
docker compose down
```

Add `-v` to `docker compose down` to wipe the database as well.

`docker compose down` requests gameserver shutdown first and keeps the login/shared processes
alive until its workers finish. Gameserver has a 30-second shutdown deadline; Compose allows 45
seconds before killing the container. A deadline expiry is a failed, forced exit, not a completed
drain. This joins workers but does not introduce a full world-save operation
([running the servers](docs/operations/running-the-servers.md)).

### Start the servers by hand

Set `command: ["sleep","infinity"]` on the `odk-server` service, then:

```sh
docker exec -w /home/darkeden/vs/bin -it odk-server /bin/bash
./start.sh
```

## Building locally

### Linux

Install the libraries, then build from the repository root:

```bash
sudo apt install libmysqlclient-dev liblua5.1-dev zlib1g-dev
make            # Debug build (default; same as make debug)
make release    # Release build
make clean      # remove build/, bin/, lib/
```

`make` wraps CMake and builds with tests off. Binaries go to `bin/` (`loginserver`,
`sharedserver`, `gameserver` and the password tool `hashpw`), libraries to `lib/`. For
development choose the Debug build: it is the one with the zone-group ownership checks armed
([build commands](docs/build/build-commands.md)).

### macOS

The servers also build natively with Apple Clang and Homebrew:

```bash
xcode-select --install
brew install cmake mysql-client@8.4 luajit
make debug      # or: make release
```

CMake finds the keg-only `mysql-client@8.4` by itself. The Docker stack is still the way to run
the servers on a Mac; [native macOS builds](docs/build/native-macos.md) has the verified versions,
what a native run would need, and the workaround the test suite needs there.

### Development builds and tests in the container

`Dockerfile.dev` provides the same pinned Zig/Clang compiler as the production builder. Build it
once from the repository root:

```bash
docker build -f Dockerfile.dev -t darkeden-dev .
```

The development helper copies only build inputs into a Docker volume, avoiding the cost of
compiling directly from a Windows bind mount. Artifacts remain in that volume rather than updating
the checkout's `bin/` and `lib/` directories.

```bash
make dev-test     # build and run the whole test suite
make dev-build    # build every production target
make dev-shell    # a shell in the build workspace
```

The same commands work on macOS under OrbStack (verified) and should under Docker Desktop; on
Apple Silicon the image is arm64 and the build is native to it. `make test` runs the same suite
with a local compiler. The suite includes `game_server_runtime_tests`,
`login_server_runtime_tests` and `shared_server_runtime_tests`, which link the same server
implementation objects as the production executables, so the first suite build also compiles the
server runtimes. [Container builds](docs/build/container-builds.md),
[the testing overview](docs/testing/overview.md) and [test coverage](docs/testing/test-coverage.md)
say more.

## Database setup

The compose stack does all of this itself. For a MySQL of your own (5.7 or 8), first remove
`NO_ZERO_DATE` and `STRICT_TRANS_TABLES` from the SQL mode:

```sql
set @@global.sql_mode = 'ONLY_FULL_GROUP_BY,NO_ZERO_IN_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';
```

Then load the schema. `initdb/a-setup.sql` creates the `DARKEDEN` game database, the `USERINFO`
account database and the `elcastle` user:

```bash
mysql -h 127.0.0.1 -u root -p < initdb/a-setup.sql
mysql -h 127.0.0.1 -u elcastle -D 'DARKEDEN' -p < initdb/DARKEDEN.sql
mysql -h 127.0.0.1 -u elcastle -D 'USERINFO' -p < initdb/USERINFO.sql
```

A database created before a change in `initdb/migrations/` needs that migration run once; nothing
checks them at startup. Text is utf8mb4 end to end, and each server pins its session to it.
[Database setup](docs/database/overview.md) lists the migrations and explains the character set.

## Configuration

`conf/` holds `gameserver.conf`, `loginserver.conf` and `sharedserver.conf` (the container uses
`docker/conf/` instead). Set these before the first start:

- `HomePath` - the repository directory; it must be right;
- `DB_HOST` (and the rest of the `DB_*` block) - the game database;
- `UI_DB_HOST` (and the `UI_DB_*` block) - the account database, which the gameserver reads too;
- `LoginServerIP` - where the game servers find the loginserver.

The `WorldDBInfo` table (the database the servers reach) and the `GameServerInfo` table (the
address and ports the client is told) must agree with these files: `docker/initdb-docker.sql`
sets both for the container, so copy its two `UPDATE`s with your values. For Exchange
purchases both connection blocks must name one MySQL server, and `DB_USER` must be able to read
and write the `AccountPoint` and `PointLedger` tables. Every key that matters, and what
diagnostics may contain, is in [configuration](docs/database/configuration.md).

## Running the servers

Start them in this order:

```bash
./bin/loginserver -f ./conf/loginserver.conf
./bin/sharedserver -f ./conf/sharedserver.conf
./bin/gameserver -f ./conf/gameserver.conf
```

Each executable accepts `-f <config file>`. Loginserver also accepts `-i ID`, a signed decimal
offset added to `LoginServerBasePort`, `LoginServerBaseUDPPort` and `LoginServerBaseID`.
Argument, file-loading and override errors return a failure status before server
initialization. In the container image `docker/start.sh` does this, with `docker/conf/`
([running the servers](docs/operations/running-the-servers.md)).

## Accounts and passwords

`initdb/DARKEDEN.sql` ships two development accounts, each with characters:

| Account  | Password |
|----------|----------|
| `111111` | `111111` |
| `222222` | `222222` |

Passwords are stored as argon2id hashes in `Player.Password` (the loginserver's `PasswordHash`
module, over the vendored `third_party/argon2`), never in plain text. Registering from the client
creates a hashed account. To set or reset a password by hand, hash it with `bin/hashpw` (the
password is read from stdin so it stays out of shell history) and store the result:

```sh
docker exec -i odk-server ./hashpw <<< 'new-password'
```

```SQL
UPDATE DARKEDEN.Player SET Password = '$argon2id$v=19$...' WHERE PlayerID = 'someone';
```

An existing database needs the column widened once
(`initdb/migrations/001-argon2-password-column.sql`). Its rows can keep their old plaintext value:
the loginserver still accepts it and rewrites the row as a hash on that account's next successful
login, so nobody is locked out. `bin/hashpw --verify '<stored value>'` checks a password against a
stored value.

## English content

The content tables of `initdb/DARKEDEN.sql` (NPC names and dialogue, zone, monster and item names,
system messages, nicknames and the rest) and the quest lists in `data/` are English; the
translations and the scripts that write them are in `tools/i18n/` (its README explains the
tables). A fresh install gets the English rows from the seed; a database created before them takes
them once, without touching accounts, characters or items:

```sh
docker exec -i odk-mysql mysql -u elcastle -pelca110 DARKEDEN < initdb/migrations/004-english-content.sql
```

The client repository ships the matching English for the client-side data (`tools/i18n` there);
the NPC and place spellings are shared between the two.

## Troubleshooting

**Look at the database.** Log in to the compose stack's MySQL:

```sh
docker exec -it odk-mysql mysql -u elcastle -pelca110
```

```SQL
use DARKEDEN;
update GameServerInfo set IP = '192.168.0.16';
```

- **The client cannot reach the game server from another machine:** the server IP comes from
  the `DARKEDEN.GameServerInfo` table; set it as above and restart the server container.
- **A server exits at once:** an argument, configuration-file or port error returns a failure
  status before initialization; listener ports must be whole decimal numbers in 1..65535
  ([lifecycle rules](docs/startup/lifecycle-rules.md)).
- **Exchange purchases are refused:** the gameserver checks its point tables at startup and says
  why on stderr and in `DBError.log`; see [Configuration](#configuration).
- **Text comes out garbled:** every table must be InnoDB in `utf8mb4_unicode_ci`, as the seed's
  are; the servers pin their sessions to utf8mb4 ([database setup](docs/database/overview.md)).
- **CMake stops at `jthread` or `stop_token`:** the compiler's library is too old; build in the
  container (`make dev-build`).
- **The test suite does not compile on macOS:** force the system `assert.h` in, as
  [native macOS builds](docs/build/native-macos.md) shows.
- **Stopping takes 30 seconds or exits non-zero:** a worker did not drain before the shutdown
  deadline ([running the servers](docs/operations/running-the-servers.md)).
- **More diagnostics:** they are off by default; `DARKEDEN_TRACE=1` turns them on per process
  ([production logging](docs/operations/logging.md)).

## Pack pre-built binaries into an image

`Dockerfile.pub` packages an already-compiled `bin/` directory instead of compiling from source,
which is useful when publishing a release image. It installs the same runtime libraries and
applies the same `start.sh`/CRLF handling as the source build's runtime stage. The checkout's
`bin/` must hold Linux binaries built for the Ubuntu 20.04 runtime (e.g. copied out of the
`darkeden-dev` volume; `make dev-build` does not update `bin/`). BuildKit is required so that
`Dockerfile.pub.dockerignore` (which keeps `bin/` in the context) is used instead of
`.dockerignore`:

```sh
DOCKER_BUILDKIT=1 docker build . -t darkeden:latest -f Dockerfile.pub
```

Production diagnostics and the logging migration policy are documented in
[docs/operations/logging.md](docs/operations/logging.md).
