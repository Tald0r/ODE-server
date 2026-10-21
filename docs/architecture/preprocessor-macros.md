---
type: Rule
title: "Preprocessor macros"
description: "The server-type macros and __COMBAT__ are set per target, a macro nothing defines is never reintroduced, and the mofus guards are not the switches they look like."
tags: [architecture, build]
enforced_by: [ratchets, arch_includes]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Preprocessor macros

The compile definitions that control behaviour are set per target in the `CMakeLists.txt` files,
plus `LoginServer.h` and `SharedServer.h`, which define their own server macro so that including
the header makes the translation unit that server's:

- `__GAME_SERVER__` / `__LOGIN_SERVER__` / `__SHARED_SERVER__` say which server a translation unit
  is compiled for;
- `__COMBAT__` enables combat code (the gameserver targets define it);
- `__LINUX__` comes from the top-level `CMakeLists.txt`.

A kernel file may mention none of the first group nor `__COMBAT__` (K2,
[module layering](module-layering.md)).

**A macro no build defines makes the block behind it dead text**, so ratchet R14 holds the count
of the ones that were removed at zero; do not reintroduce one. Two that look like switches are
not: the mofus pair sits behind `#ifdef __MOFUS__`, which `mofus/Mofus.h` defines when
`__METRO_SERVER__` is, and `src/Core/types/ServerType.h` defines that unconditionally for
everything that includes `Types.h`, so the module is always built; `__METRO_SERVER__` also gates
`Encrypter.h`, so it is not a knob to turn.
