---
type: Rule
title: "Wire contract"
description: "A packet's bytes never change silently: goldens pin them, the inventory pins every factory's id, name and size, and a diff is a protocol change the client ships identically."
tags: [protocol, testing]
enforced_by: [wire_tests, ratchets]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Wire contract

**A failing golden or inventory diff is a protocol change, not a test to silence.** The client
repository keeps hand-maintained copies of every packet class; ship the identical change there and
link the two commits. Re-record deliberately: `UPDATE_GOLDENS=1 ./bin/wire_tests`.

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| A packet's bytes never change silently | the golden fixtures in `tests/golden/`, compared by `wire_tests` | a byte diff in the golden, a protocol change the client repo must ship identically |
| Every factory's packet id, name and max body size stay as inventoried | `tests/wire_layout_test.cpp` against `tests/wire-layout.txt` | the inventory diff (`tests/tools/wire_inventory_diff.sh` cross-checks the client's copy) |
| A packet that touches the encrypter is pinned at codes 0..5 | `ratchets.sh`, looking for `tests/golden/<Name>.code5.hex` | "packets use the encrypter but have no per-code goldens"; exceptions live in `tests/ratchet/encrypter_exceptions.txt` |

The wire-layout inventory covers all 465 registered factories and the goldens 451 of them. The
client commits the same inventory from its own packet classes; a diff between the two is a
protocol bug in one repo or the other, and the restructuring plan's task 1.4 records the findings
([the plan](../reviews/RESTRUCTURING.md)). Which factories each server registers is pinned
separately ([factory registrations](factory-registrations.md)).
