---
type: Rule
title: "Fix log and task status"
description: "A bug found while restructuring is recorded in the fix log with its status, and a restructuring task is done only once its owner exists, both updated in the same commit as the work."
tags: [testing, process]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Fix log and task status

**Bugs found while restructuring are recorded, not fixed silently.** [The fix log](../reviews/FIXES.md)
takes an entry per bug with a `> **Status:**` line in the convention `fixed (<branch>)` |
`recorded, not fixed (<branch>)` | `open` | `not a defect`, updated in the same commit as the
fix. Entries are newest first.

**Task status lines.** [The restructuring plan](../reviews/RESTRUCTURING.md) is the living plan.
Each task has a checkbox and a `> **Status:**` line, `not started` | `in progress (<what
remains>)` | `done (<commit>)` | `dropped (<why>)` | `blocked (<on what>)`, updated in the same
commit as the work it describes. It records the current state and what the next reader needs to
act (conventions, open bugs, what remains); the narrative of a change belongs in its PR
description and commit message. A task is `done` only once its **Owner** exists, the test or
mechanism that keeps the rule true from then on, which is the same rule the ratchets apply to
their numbers ([ratchets](ratchets.md)).

Comments under `src/` and `tests/` describe behaviour and carry none of this bookkeeping
([dead sources and source encoding](../architecture/dead-sources-and-encoding.md)).
