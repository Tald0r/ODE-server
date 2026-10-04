---
type: Reference
title: Login lifecycle and authorization
description: Distinguish credential grants, account cleanup ownership and session phases.
tags: [server, login, ownership]
source_revision: "d116b3408efff31247ca368cc037adc77d16c76b"
generated:
  by: codex/gpt-6
  at: "2026-10-04T09:28:06Z"
sources:
  - id: handler
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/src/server/loginserver/handler/CLLoginHandler.cpp
  - id: authentication
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/src/server/loginserver/LoginAuthentication.cpp
  - id: ownership
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/src/server/loginserver/LoginAccountOwnership.h
  - id: disconnect
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/src/server/loginserver/LoginAccountSession.cpp
  - id: plan
    resource: https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/docs/RESTRUCTURING.md
---

# Login lifecycle and authorization

Read `CLLoginHandler` for the main flow at this snapshot: ID normalization and
address checks precede web/NetMarble authentication; ordinary password checking
and `decideLogin` follow. A normal success reply is sent before publishing
`LPS_WAITING_FOR_CL_GET_PC_LIST`. A kick path retains identity for later
completion instead of sending that reply immediately.[^handler]

`LoginAuthentication` handles web keys, NetMarble credentials and refusal
sending through supplied actions. Web authorization grants a free pass only
after key deletion returns. Authentication flags, account identity and player
phase are distinct state; a free-pass boolean is not account cleanup ownership.
Read the main handler as well as the gate when investigating retries.[^authentication][^handler]

`LoginAccountOwnership` prepares the account name before an acquisition write
and owns it after acknowledged success. An existing owner cannot be replaced
by another acquisition. Disconnect attempts flush/close/logout cleanup despite
earlier failures, preserves the first failure and retains ownership if logout
fails, so cleanup can retry.[^ownership][^disconnect]

Start verification with `tests/login_authentication_test.cpp`,
`tests/login_decision_test.cpp` and `tests/login_account_session_test.cpp`. The current `docs/RESTRUCTURING.md`
login tasks and `docs/FIXES.md` are the status authorities for remaining flow
extraction and retry/reporting defects; this source map does not certify their
resolution.[^plan]

See [architecture](architecture.md) and [builds and tests](builds-and-tests.md)
for the runtime boundary and test commands.

[^handler]: [src/server/loginserver/handler/CLLoginHandler.cpp](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/src/server/loginserver/handler/CLLoginHandler.cpp)
[^authentication]: [src/server/loginserver/LoginAuthentication.cpp](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/src/server/loginserver/LoginAuthentication.cpp)
[^ownership]: [src/server/loginserver/LoginAccountOwnership.h](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/src/server/loginserver/LoginAccountOwnership.h)
[^disconnect]: [src/server/loginserver/LoginAccountSession.cpp](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/src/server/loginserver/LoginAccountSession.cpp)
[^plan]: [docs/RESTRUCTURING.md](https://github.com/bound2/opendarkeden-server/blob/d116b3408efff31247ca368cc037adc77d16c76b/docs/RESTRUCTURING.md)
