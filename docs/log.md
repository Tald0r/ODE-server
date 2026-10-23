# Knowledge Bundle Update Log

## 2026-10-23

* **Update**: `CLAUDE.md`'s "Rules and the tests that own them" table has one home, the area
  concepts, each holding its own rows; `CLAUDE.md` no longer repeats it. Where an area row had
  dropped detail (the runtime test binary a
  fixture runs in, the frame gate's caveats) it has the fuller wording back, and the ten rules
  only prose stated have a row: [module layering](architecture/module-layering.md),
  [packet system](protocol/overview.md), [server architecture](architecture/overview.md),
  [repository seams](architecture/repository-seams.md) (two),
  [critical sections](threading/critical-sections.md), [the mutation rule](threading/mutation-rule.md),
  [dead sources and source encoding](architecture/dead-sources-and-encoding.md),
  [assertions and NDEBUG](architecture/assertions-never-ndebug.md) and
  [client link](protocol/client-link.md). Module layering states C1's real scope, the gameserver's
  domain module directories, and [database setup](database/overview.md) no longer calls the
  Exchange's manual escaping pass unreachable: only its fall-through on a refused
  `mysql_real_escape_string` is.
* **Creation**: [test coverage](testing/test-coverage.md), the README's account of what the
  runtime, startup, socket and worker tests exercise, grouped by process and library; it is the
  one place that coverage is described, and [startup composition](startup/overview.md) names
  each library's owning test and links here.
* **Creation**: [native macOS builds](build/native-macos.md), the README's native macOS build,
  its test-suite workaround and the three macOS-only failures.
* **Update**: the facts `CLAUDE.md` and `README.md` held that no concept stated yet moved into
  their concepts: the startup libraries' dependencies
  ([startup composition](startup/overview.md)); the directory tree's de-core file list, `docs/`
  entry and repository directories and the game-concept glosses
  ([server architecture](architecture/overview.md)); the fuzz switches' gate detail
  ([fuzzing](protocol/fuzzing.md)); the billing threads ([threads](threading/overview.md)); R10a
  and R10b ([exceptions and outcomes](architecture/exceptions-and-outcomes.md)); migration 004
  ([database setup](database/overview.md)); the server arguments and the compose shutdown order
  ([running the servers](operations/running-the-servers.md)).
* **Update**: `CLAUDE.md` is an index in the sidecar repository's shape: a short intro, the
  build and test commands, and one line per rule linking its concept. Its sections moved as
  follows: the rules table -> each row to its area's concept; working in this
  repository -> [ratchets](testing/ratchets.md), [fix log and task status](testing/fix-log-and-status.md),
  [de-core parity](protocol/decore-parity.md), [wire contract](protocol/wire-contract.md), [CI](build/ci.md);
  build, formatting, tests and the container -> [build commands](build/build-commands.md),
  [formatting](build/formatting.md), [testing overview](testing/overview.md),
  [container builds](build/container-builds.md); packet-read fuzzing -> [fuzzing](protocol/fuzzing.md);
  project architecture and its directory tree -> [server architecture](architecture/overview.md) and
  [startup composition](startup/overview.md); the packet system -> [packet system](protocol/overview.md);
  preprocessor macros -> [preprocessor macros](architecture/preprocessor-macros.md);
  configuration -> [configuration](database/configuration.md); database setup ->
  [database setup](database/overview.md) and [repository seams](architecture/repository-seams.md);
  dependencies -> [dependencies](build/dependencies.md); game concepts -> [server architecture](architecture/overview.md);
  thread ownership -> the [threading](threading/index.md) folder; running the servers ->
  [running the servers](operations/running-the-servers.md); development notes ->
  [dead sources and source encoding](architecture/dead-sources-and-encoding.md) and
  [exceptions and outcomes](architecture/exceptions-and-outcomes.md).
* **Update**: the comments under `src/` that cited `CLAUDE.md`'s "Thread ownership" section or
  its `sql_mode` now name the threading and database concepts (except
  `MySQLExchangeRepository.cpp`, a CRLF file, whose "see CLAUDE.md" the index's database line
  still answers), and the mentions in
  [toolchain notes](build/toolchain.md), [the fix log](reviews/FIXES.md) and
  [the plan](reviews/RESTRUCTURING.md) (tasks 3.4 and 5.1) point at the concepts that hold what
  they cited.
* **Update**: `README.md` is a getting-started guide: prerequisites, the compose quick start,
  local and container builds, database setup, configuration, running the servers, accounts,
  English content, troubleshooting and the pre-built image. Its long account of what the tests
  cover moved to [test coverage](testing/test-coverage.md), and its native macOS build and tests
  to [native macOS builds](build/native-macos.md), which [the fix log](reviews/FIXES.md) now
  names for the forced include.

## 2026-10-21

* **Creation**: the bundle. `docs/` is now an Open Knowledge Format v0.2 bundle structured like
  the sidecar repository's: a root [index](index.md), [the knowledge bundle](knowledge-bundle.md)
  guide, this log, and one folder per area with an `index.md` and one concept per rule, design,
  guide or record. The ctest `knowledge_bundle` (`tests/tools/check_knowledge_bundle.pl`) owns
  the structure.
* **Creation**: the concepts under [architecture](architecture/), [startup](startup/),
  [threading](threading/), [protocol](protocol/), [login](login/), [shared](shared/),
  [game](game/), [database](database/), [build](build/), [testing](testing/) and
  [operations](operations/), drafted from `CLAUDE.md`, the restructuring plan and the code. The
  rule tables in `CLAUDE.md` are partitioned by area into the `Rule` concepts, each listing its
  owners in `enforced_by`.
* **Move**: the old `docs/knowledge/` bundle is retired; its four concepts are superseded:
  * `docs/knowledge/index.md` -> `docs/index.md`
  * `docs/knowledge/architecture.md` -> [server architecture](architecture/overview.md) and
    [module layering](architecture/module-layering.md)
  * `docs/knowledge/builds-and-tests.md` -> [build commands](build/build-commands.md) and
    [testing overview](testing/overview.md)
  * `docs/knowledge/packet-compatibility.md` -> [wire contract](protocol/wire-contract.md) and
    [de-core parity](protocol/decore-parity.md)
  * `docs/knowledge/login-lifecycle.md` -> [login lifecycle](login/overview.md)
* **Move**: the documents at the old `docs/` root, each given frontmatter and otherwise unchanged;
  every reference in the repo was re-pointed, except the comments in the de-core files the client
  vendors byte-identically (`src/domain/{EquipRequirement,ItemGrade,ItemPrice,SkillOutputFormulas,SkillRange}.h`
  and `src/domain/vectors/{price,skill_output,stats}.tsv`), which still say `docs/FIXES.md` until
  the next client resync:
  * `docs/RESTRUCTURING.md` -> `docs/reviews/RESTRUCTURING.md`
  * `docs/FIXES.md` -> `docs/reviews/FIXES.md`
  * `docs/TOOLCHAIN.md` -> `docs/build/toolchain.md`
  * `docs/logging.md` -> `docs/operations/logging.md`
  * `docs/websocket.md` -> `docs/operations/websocket-gateway.md`
* **Update**: `CLAUDE.md`, `README.md` and `AGENTS.md` point at `docs/index.md` and
  `docs/knowledge-bundle.md` instead of the retired `docs/knowledge/index.md`.
