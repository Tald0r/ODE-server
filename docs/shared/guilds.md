---
type: Rule
title: "Guilds"
description: "Guild loads prepare a complete owned guild and roster graph before publishing under the table lock, initialization checks its ID maxima, and replies own every partial record until a complete batch is prepended."
tags: [shared, guild]
enforced_by: [shared_server_runtime_tests, shared_guild_decision_tests, guild_retirement_tests, guild_union_registry_tests, guild_union_join_offer_tests, guild_union_teardown_tests, guild_join_decision_tests]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Guilds

The fixtures are test groups in `shared_server_runtime_tests` ([sharedserver](overview.md)); the
`guild_*_tests` targets own the gameserver side's retirement model and union registry
([closed violations](../threading/closed-violations.md)).

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| Shared guild loads prepare a complete owned guild/roster graph and publish under the table lock; failures preserve previous borrowed guild/member pointers | `SharedGuildLoading` in `shared_server_runtime_tests` over explicit repository inputs and the production reply builder | partial guild/roster publication, unattached-member leaks, allocation aborts in copying string accessors, invalid narrowed fields, lost borrowed rows, leaked reloads or failed retry |
| Shared guild initialization prepares checked ID maxima and a complete roster before publishing counters; failed initialization preserves both the graph and counters | `SharedGuildStartup` in `shared_server_runtime_tests` over supplied repositories/configuration and allocation faults | partial counter updates, wrapped IDs, malformed configuration, unsafe empty-table MAX queries, changed exception identity, lost borrowed rows or failed retry |
| A live guild founding never wraps a word-sized counter: a full guild id counter founds nothing and stays full, a race whose next zone id is the last founds nothing, and only an accepted founding claims a guild id | `SharedAddGuild` in `shared_server_runtime_tests` over the production handler; `DecideAddGuild` in `shared_guild_decision_tests` | a guild founded with id 0 or a wrapped zone id, a counter moved by a refused founding |
| Shared guild replies own every partial record and prepend only a complete batch; count or allocation failure preserves the destination | `SharedGuildReplies` in `shared_server_runtime_tests` over production assembly and packet ownership handoffs | leaked guild/member records, partial destination changes, lost row identity/order, allocating publication, damaged refusal/retry or leaked repeated construction |

The seed data's guild-union rules (no guild in two unions, every union with a member or an offer)
are held by the ratchets ([seed-data rules](../database/seed-data-rules.md)).
