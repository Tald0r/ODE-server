---
type: Rule
title: "Assertions and NDEBUG"
description: "NDEBUG is never defined, so Assert and __BEGIN_TRY keep one meaning in every configuration; the Debug-only ownership checks ride their own macro."
tags: [architecture, build]
enforced_by: ["the #error in src/Core/Assert.h and src/Core/Exception.h"]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Assertions and NDEBUG

**`NDEBUG` is never defined.** `Assert` and `__BEGIN_TRY` / `__END_CATCH` keep one meaning in
every configuration, so an optimized build refuses the same input a debug build refuses. The
optimized configurations pass `-UNDEBUG` because `zig c++` defines it from `-O1` up, and an
`#error` in `src/Core/Assert.h` and `src/Core/Exception.h` fails the compile of every project
file in any configuration that defines it.

The one check that is Debug-only, `DE_OWNERSHIP_CHECKS`, therefore rides its own macro rather
than `NDEBUG`: it is defined for Debug builds alone and compiled away completely elsewhere
([the mutation rule](../threading/mutation-rule.md)). For development always choose the debug
build (`make` or `make debug`), because that is the only one with the checks armed
([build commands](../build/build-commands.md)).
