---
type: Rule
title: "Factory registrations"
description: "Each server registers exactly the factories it registered before, and the generated factory list matches the kernel membership."
tags: [protocol, testing]
enforced_by: [ratchets]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Factory registrations

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| Each server registers exactly the factories it registered before | `tests/tools/factory_registrations.pl` vs `tests/ratchet/factory_registrations.txt`, run by `ratchets.sh` | a membership diff, on an add **or** a drop |
| `tests/generated/AllPacketFactories.inc` matches the kernel membership | `ratchets.sh` re-runs `tests/tools/gen_factory_list.sh` and diffs | "AllPacketFactories.inc is stale" |

A drop fails as loudly as an add because a server that stops registering a factory stops reading
a packet the client still sends, and nothing else in the suite would notice. The lists the
registrations come from are the `constexpr` factory lists `PacketFactoryManager::init()` selects
per server ([packet system](overview.md)); the add-packet skill walks through both files.
