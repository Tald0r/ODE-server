---
type: Guide
title: "The knowledge bundle"
description: "How docs/ is organised as an Open Knowledge Format v0.2 bundle, which frontmatter keys and types it uses, how to add or move a document, and the ctest that keeps it whole."
tags: [process, knowledge]
---

# The knowledge bundle

`docs/` is a knowledge bundle in Google's Open Knowledge Format, version 0.2
([the spec](https://github.com/GoogleCloudPlatform/knowledge-catalog/blob/main/okf/SPEC.md)): a
tree of markdown files called concepts, each starting with a YAML frontmatter block. `CLAUDE.md`
is the working brief for an agent; the rules and the reasons behind them live here, one concept
each, and the brief links in. The sidecar repository's bundle is the model this one follows.

# Layout

| Folder | What lives there |
|---|---|
| `architecture/` | the processes, the library layering, contexts, repository seams, the source rules |
| `startup/` | the entry-point composition and the rules its lifecycle, socket and worker tests own |
| `threading/` | thread ownership, the mutation rule, critical sections, mailboxes and snapshots |
| `protocol/` | packets, the wire contract, registrations, the client link, the frame gate, fuzzing, de-core parity |
| `login/`, `shared/`, `game/` | one folder per server process: an overview and its rule tables |
| `database/` | MySQL setup, configuration, seed-data rules |
| `build/` | make targets, the container build, the native macOS build, formatting, CI, dependencies, toolchain notes |
| `testing/` | the suite, the whole rules-and-owners table, the ratchets, the fix log and task status conventions |
| `operations/` | running the servers, logging, the WebSocket gateway |
| `reviews/` | the restructuring plan and the fix log, the two living records the scripts name |

Every folder has an `index.md` listing what it holds, and [`log.md`](log.md) records changes to
the bundle, newest first.

# Frontmatter

```yaml
---
type: Rule                         # required
title: "Critical sections"
description: "One sentence. index.md and search show it."
tags: [threading]
enforced_by: [critical_section_audit]   # the test or gate that fails when the rule breaks
---
```

- **Types in use:** Rule, Design, Guide, Runbook, Reference, Record.
- **`enforced_by`** names the ctest, test fixture or build gate that pins a rule. A rule without
  it is a convention that only review protects.
- **`generated: { by, at }`** marks content an agent drafted (`claude-code/<model>`). The concepts
  written on 2026-10-21 carry it; the records and guides that moved in from the old `docs/` root
  were written by people and earlier sessions and do not.
- **`verified`**: a person who checks a concept against the code adds
  `verified: { by: human:<id>, at: <ISO 8601 datetime> }`. Until then the concept is unverified,
  and the code wins any disagreement.
- Wrap a title or description in double quotes when it contains a colon followed by a space, or
  a `#`.

# Adding or moving a document

1. Put it in the folder of what it is about: a loginserver rule goes in `docs/login/`.
2. Give it frontmatter with at least `type`, `title` and `description`.
3. Add a line for it to the folder's `index.md`, in the section for its type:

   ```markdown
   * [Title](file-name.md) - the description
   ```

4. Link to other documents with relative paths. When you move a document, grep the whole repo
   for its old path (comments under `src/` and `tests/` name documents too), fix every link and
   comment that names it, and add a `log.md` entry with the old-to-new map. The de-core files the
   client vendors byte-identically (`src/domain/CMakeLists.txt` lists them) change only with a
   client resync, so a path in their comments waits for that.
5. A new server process or a new area of rules gets a folder with an `index.md` and an
   `overview.md`.

The ctest `knowledge_bundle` (`tests/tools/check_knowledge_bundle.pl`, run by `make test` and
`make dev-test` with the rest of the suite) fails on a concept without a `type`, an index that
misses a concept or subfolder, a `log.md` heading that is not an ISO day, or a broken local link
in the bundle, `CLAUDE.md`, `README.md` or `AGENTS.md`. Run it alone before you push a docs change:

```bash
perl tests/tools/check_knowledge_bundle.pl .
```
