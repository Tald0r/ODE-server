---
type: Rule
title: "De-core parity"
description: "A change to src/domain is done only when the client's byte-identical copy is resynced and the diff is clean; a changed vector row is a balance change."
tags: [protocol, domain]
enforced_by: [formula_tests, de-core-strict, "tests/tools/decore_client_diff.sh"]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# De-core parity

**A change to `src/domain` is done only when
`bash tests/tools/decore_client_diff.sh <client-root>` is clean** against a client checkout
resynced from this one (its `tools/decore/sync.pl`). The client builds a byte-identical copy of
the vendored subset, so the two land together, the server first (task 3.6 in
[the plan](../reviews/RESTRUCTURING.md)). `src/domain/CMakeLists.txt` names the subset the
client vendors; `DarkLight.cpp` and `PartyExp.cpp` are server-only, and their parity rows live in
`vectors/server/`, a directory the client's sync does not copy.

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| The de-core subset the client vendors computes exactly the parity vectors and compiles without a warning under a GCC/Clang proxy for the client's warning set (MSVC's C4146 and C4805 have no counterpart) | `formula_tests`' `SharedVectors` over `src/domain/vectors/`; `de-core-strict` (`-Werror`, built by `make dev-test`) | the row, named, with its expected and actual value; the warning, as an error |

A changed vector row is a balance change: review the `.tsv` diff as one. The same byte-identity
means a comment in a vendored file, even a document path, changes only with a resync, which is why
the vendored headers and `.tsv` files still name `docs/FIXES.md` by its old path
([the knowledge bundle](../knowledge-bundle.md)). The include rule that keeps the subset
compilable in both repositories is D1 ([module layering](../architecture/module-layering.md)).
