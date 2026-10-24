# Overview

* [Threads and what they own](overview.md) - The main thread, the zone-group threads and the manager threads of the gameserver, and the same contract in the login and shared servers, with the thread-local database connection each registers.

# Rules

* [Critical sections](critical-sections.md) - An __ENTER/__LEAVE block is owned by a scoped guard, so a hand-written unlock() inside it is a double unlock; the audit script finds one. A gameserver destructor takes no lock of its own object.
* [The mutation rule](mutation-rule.md) - Zone-group state is touched only while that group's mutex is held, and the Debug ownership check aborts on a violation at the gateways it covers.

# Designs

* [Cross-thread communication](cross-thread-communication.md) - Sending to a player's socket is the one legitimate cross-thread operation; everything else goes through the player's mailbox, the group's mailbox or a copy-on-write snapshot.

# Records

* [Closed violations](closed-violations.md) - The known violations of the mutation rule, each closed, and the rules they left behind: guild retirement, tile gateways, snapshot zone maps, GDRLair locking, posted war work and the CGReady drain.
