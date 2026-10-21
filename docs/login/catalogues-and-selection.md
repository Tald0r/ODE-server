---
type: Rule
title: "Catalogues and selection"
description: "World, group, server, zone and character catalogues are loaded as complete owned tables, selection uses actual membership, and list replies own their records until sending succeeds."
tags: [login]
enforced_by: [login_server_runtime_tests, world_selection_tests, character_selection_tests]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Catalogues and selection

Fixtures named without a target are test groups in `login_server_runtime_tests`
([login lifecycle](overview.md)). The common world and server catalogues every process loads are
in [the gameserver folder](../game/world-and-server-catalogues.md).

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| Login world-list assembly uses explicit catalogue/account/player inputs and owns every row through lookup and sending; actual world IDs determine ascending reply order | `LoginWorldList` over the production assembler, packet ownership and supplied repositories | missing sparse/boundary IDs, wrapped saved selections, partial sends before preparation, leaked lookup/allocation/send failures, changed default/truncation/count policy or failed cleanup/retry |
| Login group/population loads prepare checked value-owned catalogues before publication; failure preserves borrowed rows and live counters, and unloaded managers have safe scoped lifetimes | `LoginCatalogue` over explicit repositories and production managers | unloaded-destructor crashes, wrapped dimensions/fields, partial reloads, leaked world-zero/duplicate rows, lost counts or pointers, damaged exception translation, failed allocation/reporting cleanup or failed retry |
| Login selection borrows explicit catalogues and uses actual world/group membership; missing worlds are refused, missing server-selection IDs use the highest configured keys, and empty tables are refused | `world_selection_tests` and `LoginWorldTopology`/`LoginWorldTopologyTest` over real managers with supplied rows | unselectable sparse IDs, absent count-derived lookups, unordered group replies, lost closed/down policy, stale reloads/populations or narrowed boundary IDs |
| Login server-list replies use explicit topology/account/player inputs, preserve each handler's account query and own partial records through synchronous sending | `LoginServerList` over the production assembler and packet ownership handoffs | changed query/default policy, wrapped saved groups, over-budget account queries, leaked partial/cleared rows, changed fields/status/truncation or failed exception cleanup/retry |
| Login zone/group routing loads accept explicit repositories and publish complete owned maps; failure preserves existing borrowed const rows | `LoginRoutingCatalogue` over production loaders | partial duplicate/allocation publication, leaked incoming rows, stale routes after reload, changed word-sized mappings or database-error translation, damaged borrowed rows or failed cleanup/retry |
| Login character-selection topology borrows explicit server/zone/group catalogues and preserves missing-reference exceptions and the decision's routing/gate policy | `LoginCharacterTopologyTest`, composed with production loaders and `decideSelectPC`, plus `character_selection_tests` | wrong world/group flags, narrowed sparse routes, hidden missing references, stale reloads, changed quest boundaries or non-PK/rejection precedence, or catalogue reads before early gates |
| Character selection accepts only the canonical stored slot names and preserves one-based account slots; malformed text is refused before ordinary or quest routing | `character_selection_tests` and `LoginCharacterTopologyTest` | accepted wrong prefixes, unsupported digits or other suffix bytes, changed earlier-gate precedence, missing-zone lookups after malformed slots, or zero-based account values |
| Login character-list assembly takes explicit world/account/repository inputs and returns a complete owned reply; records retain ownership until attachment succeeds | `LoginCharacterList` over the production assembler and real packet records | lost field/query order, leaked invalid or duplicate records, leaked allocation/query/reporting failures, changed exception policy or failed cleanup/retry |
| Character-list refresh owns the selected world/account and the assembled reply through explicit synchronous sending; character management is published only after the sender returns | `LoginCharacterListRefresh` over production assembly and real player output | early phase changes, stale selection inputs, changed query/send exception mapping, partial or missing replies, lost account ownership or leaked allocation/failure/retry storage |
| Server selection applies its normalized world/group before character lookup and reaches character management only after sending; refusal preserves the session | `LoginServerSelection` over real player state, production catalogues/assembly and packet serialization | stale-world queries, partially updated location, early status changes, lost exception policy, leaked send/allocation failures or failed retry |
