# Overview

* [Login lifecycle](overview.md) - The loginserver's main flow from CLLogin to character selection, the account-ownership model that outlives a connection, and where its tests start.

# Rules

* [Account ownership and kicks](account-ownership-and-kicks.md) - Every acquisition owns an account identity before writing LOGON, cleanup is attempted step by step and retried, and a kick of an existing session prepares, dispatches, verifies, retries and completes under explicit ownership.
* [Catalogues and selection](catalogues-and-selection.md) - World, group, server, zone and character catalogues are loaded as complete owned tables, selection uses actual membership, and list replies own their records until sending succeeds.
* [Character and account flows](character-and-account-flows.md) - Character selection, creation, deletion, name queries, registration and web authentication snapshot their inputs, use explicit persistence and replies, and publish session state only after the reply is sent.
