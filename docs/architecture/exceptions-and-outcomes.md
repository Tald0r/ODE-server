---
type: Rule
title: "Exceptions and outcomes"
description: "Throw an Error, never a string literal; gameplay results are Outcome values and exceptions are for programming and configuration errors."
tags: [architecture, conventions]
enforced_by: [ratchets]
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

A malformed packet body is the one exception class with a contract of its own: a reader refuses it
only with a `ProtocolException`, and the fuzz targets abort on anything else
([frame gate](../protocol/frame-gate.md)).
