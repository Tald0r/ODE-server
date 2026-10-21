---
type: Rule
title: "Contexts, not globals"
description: "No g_p* extern survives; a manager is taken from the context of its layer, whose accessor asserts that its creator registered it."
tags: [architecture]
enforced_by: [ratchets]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Contexts, not globals

**Ratchet R1 holds the `g_p*` extern count at zero.** Every process-wide manager is taken from
the context of its layer: `de::kernelContext()` (`src/Core/KernelContext.h`, a kernel file so
the kernel's own readers can use it), `de::serverContext()` (ServerCore: the database manager,
the server and world tables), `de::gameContext()`, `de::loginContext()` or
`de::sharedContext()`. Each accessor asserts that the manager's creator registered it, so a read
before that point is a startup-order bug, not a condition to branch on.
`de::kernelContext().config()` is the example every `main()` sets up first: it registers the
loaded configuration right after loading it, and a read of an unregistered configuration asserts.

Repositories follow the same shape with a function instead of a global:
`default*Repository()` accessors, never a `g_p*` extern
([repository seams](repository-seams.md)). The one singleton left in the gameserver is
`GDRLairManager::Instance()`; `MPlayerManager` became a `GameServer` member
([threading overview](../threading/overview.md)).

The baseline moved from 332 to 0 between 2026-08-29 and the end of the restructuring; the
ratchet table in [the restructuring plan](../reviews/RESTRUCTURING.md) records each step and what
moved it, and `tests/ratchet/ratchets.sh` is the enforcing copy ([ratchets](../testing/ratchets.md)).
