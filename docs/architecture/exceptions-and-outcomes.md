---
type: Rule
title: "Exceptions and outcomes"
description: "Throw an Error, never a string literal; gameplay results are Outcome values and exceptions are for programming and configuration errors."
tags: [architecture, conventions]
enforced_by: [ratchets, game_server_runtime_tests]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Exceptions and outcomes

**`throw Error("text")`, never `throw "text"`.** A bare literal throws a `const char*`, a type
nothing in the tree catches, so it walks past every handler into a `catch (...)` backstop or
`std::terminate`. Ratchet R11 holds bare string-literal throws at zero and R10b the
`catch (const char*)` handlers, so a `const char*` is neither thrown nor caught anywhere; R10a
holds throws of a pointer into a local string (`throw s.c_str()`) at zero, and R12 throw messages
carrying non-ASCII text ([ratchets](../testing/ratchets.md)).

**Gameplay results are `Outcome<Events, Rejection>`** (`src/Core/Outcome.h`): every refusal, the
precedence between refusals and the content of the reply are values a test checks against a fake
repository with no database, the way `decideLogin()` is tested in `login_decision_tests`.
Exceptions are for programming and configuration errors.

**A catch that drops a `Throwable` is control flow.** A `catch (Throwable&)` whose body is empty,
comments and `;` aside, ends the exception there: the caller never learns its block failed. The
gameserver's swallowing try macros became such catches unchanged, and each was judged test first; a
catch whose block cannot throw a `Throwable` (it only clears standard containers, deletes objects
whose destructors are `noexcept` and calls code that throws no `Throwable`) is dead and goes without
a test, since nothing can drive it. A `noexcept` destructor cannot let the exception out, so there
the decision is what it does instead: it takes no lock of its own object ([critical
sections](../threading/critical-sections.md)), and it checks an invariant with `AssertNoThrow`
(`src/Core/Assert.h`), which appends the entry a failed `Assert` writes to `assertion_failed.log`
and returns; when its work fails (an effect's `unaffect`, a player's save, the server's stop), it
logs the `Throwable` to `destructorError.log` with the bounded `diagnosticFilelog` and carries on
(`Slayer`'s and `Vampire`'s saves keep their own `slayerDestructor.txt` and
`vampireDestructor.txt`). Outside a destructor, an exception a caller handles propagates, and one no
caller handles is logged where it is caught. R19 holds such catches at zero in R5's scope (the
gameserver outside `gm/`, `handler/` and `packetfill/`), so a new one fails the suite, and R5 holds
the try macros there at zero.

A malformed packet body is the one exception class with a contract of its own: a reader refuses it
only with a `ProtocolException`, and the fuzz targets abort on anything else
([frame gate](../protocol/frame-gate.md)).
