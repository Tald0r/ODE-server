---
type: Rule
title: "Formatting"
description: "clang-format only the files you touched; a bare make fmt buries a diff in churn because most of src/ was formatted with an older version."
tags: [build, process]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Formatting

```bash
make fmt            # format all C++ code under src/ and tests/
make fmt-check      # check the files you modified (fast)
make fmt-check-all  # check every file (slow)
```

The project uses clang-format with a `.clang-format` configuration file. Format checking runs via
GitHub Actions only on pushes and merges to master (`.github/workflows/format-check.yml`), on the
files changed since the previous master tip, using whatever clang-format `ubuntu-latest`
installs (18.x as of 2026-08).

**Do not run bare `make fmt` before committing: the tree is not v18-clean.** Most of `src/` was
formatted with an older clang-format, so `make fmt` reformats files you never touched and buries
your diff in unrelated churn. Format only what you touched:

```bash
git diff --name-only master HEAD | grep -E '\.(cpp|h|hpp)$' | xargs clang-format -i
```

Touching a file that was never v18-formatted makes CI demand you reformat it. That is expected;
the reformat lands in your PR. `make fmt` / `fmt-check-all` cover `src/` **and** `tests/`,
matching CI ([CI](ci.md)).
