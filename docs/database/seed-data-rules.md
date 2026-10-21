---
type: Rule
title: "Seed-data rules"
description: "Every initdb table is InnoDB in utf8mb4_unicode_ci, every seed Player row ships a current argon2id hash, and the seed guild unions are consistent."
tags: [database, testing]
enforced_by: [ratchets, password_hash_tests]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Seed-data rules

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| Every seed `Player` row ships a current argon2id hash | `tests/password_hash_test.cpp`, which reads `initdb/DARKEDEN.sql` | the seed account missing from the test's password map |
| Every `initdb/` table is InnoDB in `utf8mb4` / `utf8mb4_unicode_ci` | `ratchets.sh` | the table, named |
| No seed guild leads or joins two guild unions | `ratchets.sh` | the guild and its unions, named |
| Every seed union has a member or a pending join offer, and every seed join offer names a union | `ratchets.sh` | the union or the offer, named |

The charset rule exists because the servers pin their session to `utf8mb4`
([database setup](overview.md)); a table in another charset would round-trip text as bytes. The
union rules mirror what the gameserver enforces at run time, `dissolveIfAbandoned_LOCKED` and
`purgeOffers` ([closed violations](../threading/closed-violations.md)), so a seed dump cannot
start the server in a state its own code would never produce. `password_hash_tests` also pins the
loginserver's argon2id hashing (`src/server/loginserver/PasswordHash.cpp`) against upstream
argon2's own vectors.
