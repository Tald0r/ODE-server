# Knowledge Bundle Update Log

## 2026-10-23

* **Creation**: [rules and owners](testing/rules-and-owners.md), the whole "Rules and the tests
  that own them" table `CLAUDE.md` carried, row for row, with a column naming the concept that
  explains each rule.
* **Creation**: [test coverage](testing/test-coverage.md), the README's account of what the
  runtime, startup, socket and worker tests exercise, grouped by process and library.
* **Creation**: [native macOS builds](build/native-macos.md), the README's native macOS build,
  its test-suite workaround and the three macOS-only failures.
* **Update**: the facts `CLAUDE.md` and `README.md` held that no concept stated yet moved into
  their concepts: the startup libraries' dependencies and test scope
  ([startup composition](startup/overview.md)); the directory tree's de-core file list, `docs/`
  entry and repository directories and the game-concept glosses
  ([server architecture](architecture/overview.md)); the fuzz switches' gate detail
  ([fuzzing](protocol/fuzzing.md)); the billing threads ([threads](threading/overview.md)); R10a
  and R10b ([exceptions and outcomes](architecture/exceptions-and-outcomes.md)); migration 004
  ([database setup](database/overview.md)); the server arguments and the compose shutdown order
  ([running the servers](operations/running-the-servers.md)).

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
