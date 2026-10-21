---
type: Rule
title: "Dead sources and source encoding"
description: "Every .cpp is compiled by a named target and every header is included; sources are UTF-8 with English comments that describe behaviour."
tags: [architecture, conventions]
enforced_by: [ratchets]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Dead sources and source encoding

**Every `src/**/*.cpp` is compiled by some target and every header is included.** No target globs
its sources, so ratchet R15 can list a source nothing compiles and R16 a header nothing includes;
both are at zero. A file that is not built is dead text that drifts from the code around it, which
is how a never-built header kept redeclaring a global the ratchet R1 had to count
([contexts, not globals](contexts-not-globals.md)).

**Source encoding is UTF-8.** The project was migrated from legacy encodings, and R17 holds the
source lines that carry a non-ASCII byte at zero: the legacy comments and string literals are all
English now, and a literal that must keep foreign bytes because data is matched against it is
written as escaped UTF-8 with an English comment beside it. R13 holds duplicated include-guard
names at zero and R18 commented-out code inside `/* */` blocks.

**Comments are English and describe behaviour.** No task numbers, dates, review history or
migration narrative in `src/` or `tests/`; translate the legacy Korean or mojibake comments you
come across. C++20 is the required standard, verified with the pinned Zig/Clang container
([toolchain](../build/toolchain.md)).
