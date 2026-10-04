---
okf_version: "0.2"
---

# DarkEden server knowledge

A small [Open Knowledge Format v0.2](https://github.com/GoogleCloudPlatform/open-knowledge-format/blob/main/SPEC.md)
bundle for navigating the server. Read concepts selectively, then follow their
sources. Repository instructions remain in [CLAUDE.md](../../CLAUDE.md);
[README.md](../../README.md) owns setup guidance. This bundle adds no build,
database or CI policy.

- [Server architecture](architecture.md) — Locate process responsibilities, library boundaries and their enforcement.
- [Builds and tests](builds-and-tests.md) — Find the supported build commands and choose the relevant test boundary.
- [Packet and client compatibility](packet-compatibility.md) — Trace wire contracts and the domain formulas shared with the client.
- [Login lifecycle and authorization](login-lifecycle.md) — Distinguish credential grants, account cleanup ownership and session phases.

## Snapshot and refresh

Concepts describe source revision `d116b3408efff31247ca368cc037adc77d16c76b`. Source URLs are pinned to
that commit; `source_revision` is a bundle-specific metadata extension.
`generated` records agent authorship and edit time, not verification. No concept
claims human review or successful execution of its linked tests.

For current work, compare the cited source paths with the checkout and consult
[RESTRUCTURING.md](../RESTRUCTURING.md) and [FIXES.md](../FIXES.md) for task and
bug status. When refreshing a concept, re-read its sources, update its claims,
source URLs, revision and generation metadata together, and check YAML,
footnote IDs and links. Preserve existing repository instructions instead of
copying changing task lists or test counts into this bundle.
