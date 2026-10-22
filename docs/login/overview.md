---
type: Design
title: "Login lifecycle"
description: "The loginserver's main flow from CLLogin to character selection, the account-ownership model that outlives a connection, and where its tests start."
tags: [login, architecture]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Purpose

The loginserver authenticates an account, lets it pick a world, a server group and a character,
and hands the client to a gameserver. Its main thread runs `ClientManager::run()` and its
`GameServerManager` worker owns the UDP link to the game servers ([threads](../threading/overview.md)).
The handlers are CL/GL/GM packets and one CG, bound in `src/server/loginserver/LoginPacketDispatch.cpp`;
the flows they drive are extracted into libraries under `src/server/loginserver/` (one `Login*.h`
per flow), each tested through `login_server_runtime_tests` with explicit repositories and real
packet serialization.

# The main flow

`CLLoginHandler` (`src/server/loginserver/handler/CLLoginHandler.cpp`): ID normalization and
address checks precede web/NetMarble authentication; ordinary password checking and
`decideLogin` follow. A normal success reply is sent before publishing
`LPS_WAITING_FOR_CL_GET_PC_LIST`. A kick path, when the account is already logged in elsewhere,
retains identity for later completion instead of sending that reply immediately
([account ownership and kicks](account-ownership-and-kicks.md)).

- **`LoginAuthentication`** handles web keys, NetMarble credentials and refusal sending through
  supplied actions. Web authorization grants a free pass only after key deletion returns.
  Authentication flags, account identity and player phase are distinct state; a free-pass boolean
  is not account cleanup ownership.
- **`LoginAccountOwnership`** prepares the account name before an acquisition write and owns it
  after acknowledged success. An existing owner cannot be replaced by another acquisition.
  **`LoginAccountSession`**'s disconnect attempts flush, close and logout cleanup despite earlier
  failures, preserves the first failure and retains ownership if the logout fails, so cleanup
  can retry.
- **Decisions** are `Outcome` values tested without a database: `decideLogin` in
  `login_decision_tests`, the reconnect decision in `reconnect_decision_tests`, character
  selection, creation and deletion in their own `character_*_tests`, registration in
  `registration_tests` ([exceptions and outcomes](../architecture/exceptions-and-outcomes.md)).
- **Passwords** are argon2id hashes in `Player.Password`; a plaintext row is rehashed on the
  account's next login ([database overview](../database/overview.md)), and
  `password_hash_tests` pins the hashing against argon2's own vectors.

# Where to start

`tests/login_authentication_test.cpp`, `tests/login_decision_test.cpp` and
`tests/login_account_session_test.cpp` for the gate and the ownership model; the rule tables in
this folder name the fixture for every other flow. The login tasks in
[the restructuring plan](../reviews/RESTRUCTURING.md) and [the fix log](../reviews/FIXES.md) are the
status authorities for what remains.
