---
type: Rule
title: "Character and account flows"
description: "Character selection, creation, deletion, name queries, registration and web authentication snapshot their inputs, use explicit persistence and replies, and publish session state only after the reply is sent."
tags: [login]
enforced_by: [login_server_runtime_tests, character_selection_tests, character_creation_tests, character_deletion_tests, registration_tests, login_decision_tests]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Character and account flows

Fixtures named without a target are test groups in `login_server_runtime_tests`
([login lifecycle](overview.md)); the `*_tests` targets are the isolated decision tests over the
same `Outcome` values.

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| Character selection snapshots explicit session/packet/rule inputs and composes the decision, refusal and incoming request without process contexts; diagnostics cannot change the outcome | `LoginCharacterSelection` in `login_server_runtime_tests` and `character_selection_tests` | changed refusal bytes or fatal messages, lost database/missing-data translation, diagnostics blocking dispatch, early reads, stale packet inputs, damaged partial effects/account ownership or leaked allocation failures |
| Character creation snapshots packet/session inputs and uses explicit persistence, cache and reply actions; attributes precede inserts and session publication follows the success reply | `LoginCharacterCreation` in `login_server_runtime_tests`, with the existing `character_creation_tests` decision owner | changed rejection bytes/fatal messages, wrong race or slot writes, stale packet inputs, early status changes, hidden partial persistence, changed database-error reply policy or leaked allocation failures |
| Vampire creation consumes two explicit raw draws after early gates and preserves the legacy modulo rules; low-Ousters reporting is optional and cannot change the result | `character_creation_tests` and `LoginCharacterCreation` in `login_server_runtime_tests`, including real random/logging adapters | hidden process randomness or logging, missing/extra draws, changed attribute spreads or balance lookups, swallowed random failures, lost cache reuse or reporting failures blocking creation/refusal |
| Character deletion snapshots decoded request/session fields and uses explicit persistence, replies and independent best-effort diagnostics; publication follows the success reply | `LoginCharacterDeletion` in `login_server_runtime_tests` with the existing `character_deletion_tests` decision owner | wrong refusal bytes or persistence order, diagnostics skipping replies, stale packet fields, hidden partial retirement/purge effects, early status changes, lost exception/account ownership or leaked allocation failures |
| Character-name availability validates actual world membership and snapshots its name before an explicit repository lookup; reserved-name policy follows lookup and publication follows sending | `LoginCharacterNameQuery` in `login_server_runtime_tests`, including the production guard and sender | rejected sparse IDs, admitted missing worlds, mismatched queried/replied names, changed reserved-name or exception policy, wrong reply bytes, early phase changes or leaked allocation failures |
| Account-name availability owns the requested ID through an explicit name probe and sender; repeated queries preserve identity/account ownership and publish registration only after sending | `LoginAccountNameQuery` in `login_server_runtime_tests` over supplied repositories and real player output | mismatched queried/replied IDs, substituted probes or character-name restrictions, account writes, wrong reply bytes, hidden send/query failures, early phase changes or leaked allocation failures |
| Registration owns request/connection inputs, checks read-back IDs and publishes identity/location only after sending; acquisition cleanup survives later failure and hashing diagnostics cannot change the outcome | `LoginRegistration` in `login_server_runtime_tests` with `registration_tests` over explicit hashing and repository inputs | unchecked sex indexing, wrapped or stale locations, early publication, changed refusal/retry/hash policy, lost partial writes/account ownership, diagnostics replacing disconnects or leaked allocation failures |
| Web/NetMarble authentication owns credentials, uses explicit password/repository actions, consumes web keys before granting a free pass and contains diagnostic failures; mismatch reporting receives no keys | `LoginAuthentication` in `login_server_runtime_tests` with `DecideWebLoginKey` in `login_decision_tests` | changed verification/migration/refusal policy, wrong account writes, early free-pass publication, hidden operation failures, lost partial effects/account ownership, credential logs or leaked allocation failures |

Login character-deletion diagnostics, like the SMS, shared-broadcast, crash-fallback and Mofus
ones, omit private payloads ([configuration](../database/configuration.md)).
