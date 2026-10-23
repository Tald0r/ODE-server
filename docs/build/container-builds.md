---
type: Guide
title: "Container builds"
description: "Never compile off the bind mount; tools/devbuild.sh syncs the inputs into a volume and builds there with Ninja and ccache: a full build takes ~3.5 minutes instead of ~20."
tags: [build]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Container builds

**Do not compile straight off the bind-mounted checkout.** Measured inside the container, the
Windows mount costs ~160x on `stat` and ~145x on reads versus the container's own filesystem, and
since every translation unit opens dozens of headers the build becomes I/O bound: a full build
took ~20 minutes at ~20% CPU on 8 cores. `tools/devbuild.sh` syncs the build *inputs* (`cmake/`,
`src/`, `tests/`, `third_party/`, `data/`, `initdb/`, `tools/`, `docs/`, `docker/start.sh`, the
top-level CMakeLists/Makefile and the `CLAUDE.md`/`README.md`/`AGENTS.md` the knowledge-bundle
check reads) into a container volume, builds there with Ninja and ccache, and copies only
generated test data back (`tests/golden/`, `tests/generated/`, `tests/wire-layout.txt`, and,
after `--record` only, de-core's parity vectors in `src/domain/vectors/`). Same build: **~3.5
minutes at ~95% CPU**, and a no-op rebuild in seconds.

```bash
make dev-test                          # build wire_tests + ctest
bash tools/devbuild.sh test --record   # re-record goldens and vectors, then run
make dev-build                         # all production targets
make dev-shell                         # shell in the workspace
make dev-clean                         # drop the workspace + cache volumes
```

Needs the image once: `docker build -f Dockerfile.dev -t darkeden-dev .`. It pins Zig 0.16.0
(Clang 21.1.0) and carries Ninja, ccache and rsync. Artifacts live in
compiler/target/build-type-specific directories in the volume, so `bin/` and `lib/` in the
checkout are **not** updated by these targets; `bash tools/devbuild.sh output-dir` prints the
artifact root. Two concurrent checkouts (git worktrees) sharing one workspace volume race the
script's `rsync --delete`: give each its own `DEVBUILD_WORK_VOLUME` (the ccache and Zig cache
volumes are safe to share). `DEVBUILD_JOBS=N` caps the build at N jobs and pins the container to
N CPUs. On a Windows host this is the loop to use ([testing overview](../testing/overview.md)).
