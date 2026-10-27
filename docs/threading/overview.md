---
type: Design
title: "Threads and what they own"
description: "The main thread, the zone-group threads and the manager threads of the gameserver, and the same contract in the login and shared servers, with the thread-local database connection each registers."
tags: [threading, architecture]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T13:00:00Z }
---

# Purpose

The gameserver's threading contract, as the code implements it. The rule it exists to serve is
[the mutation rule](mutation-rule.md): zone-group state belongs to the group's thread and its
mutex. Everything below says which thread that is for each kind of state.

# Threads in the gameserver process

- **Main thread** runs `GameServer::init()` (single-threaded startup: config, DB, zone loading),
  then `GameServer::start()`, which spawns the threads below and finally becomes
  `ClientManager::run()`, an infinite loop accepting client TCP connections and driving the
  pre-zone login/handshake phase before a player is handed to a zone group. Its loop runs the
  flag war's schedule (`FlagManager::heartbeat`), which a GM's `*command CTF` pulls forward from
  a zone thread or the login link's, so the queue has a leaf mutex of its own: the due schedule
  is taken out under it and run after it is released, as a castle's `WarScheduler` does, and
  every add and pull takes it (`tests/game_flag_schedule_test.cpp`).
- **`ZoneGroupThread`**, one per `ZoneGroup` (via `ThreadManager`/`ThreadPool`), owns all
  zone-group state. Its loop is: lock the group's mutex, `ZoneGroup::processPlayers()` (socket
  `select`, read, parse, `PacketDispatcher::dispatch` of **CG** packets) and
  `ZoneGroup::heartbeat()` (NPC/monster AI, effects, zone systems), unlock. So all CG handler code
  runs on the zone thread with the group mutex held. Each zone thread registers its own DB
  `Connection` keyed by thread id
  (`de::serverContext().database().addConnection(Thread::self(), ...)`): DB connections are
  thread-local by convention, never shared. The manager's tables of them are not: a worker
  registers while others look theirs up, so lookups take the tables' `std::shared_mutex` shared
  and registrations exclusive, a leaf held for the map access alone.
- **`LoginServerManager` thread** owns the UDP datagram link to the loginserver and dispatches
  **LG** and **GG** packets on its own thread under its own `m_Mutex`.
- **`SharedServerManager` thread** owns the TCP link to the sharedserver and dispatches **SG**
  packets on its own thread under its own `m_Mutex`.
- **`MPlayerManager` (mofus) and `GDRLairManager`** are auxiliary threads with their own loops,
  stopped and joined with the managers above. `MPlayerManager` is a `GameServer` member;
  `GDRLairManager` is still a singleton (`GDRLairManager::Instance()`). The mofus module is always
  built ([preprocessor macros](../architecture/preprocessor-macros.md)). `SMSServiceThread` is a
  `ManagedThread` too, but `GameServer::start()` never starts it; its queue is only filled by
  `CGSMSSendHandler`. Neither billing integration is in the tree: the external billing link had a
  thread of its own, and the China one its own tree.

# The login and shared servers

The same contract. The loginserver's main thread runs `ClientManager::run()` and its
`GameServerManager` worker owns the UDP link to the game servers; the sharedserver's main thread
runs `HeartbeatManager::run()` and its `GameServerManager` worker owns the TCP listener plus
`GuildManager::heartbeat()`. Each worker registers its own DB `Connection` keyed by
`Thread::self()` where it needs one.

Every worker is a `ManagedThread` ([workers and synchronization](../startup/workers-and-synchronization.md));
the shutdown order and the 30-second watchdog are in [lifecycle rules](../startup/lifecycle-rules.md).
