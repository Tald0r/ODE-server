//////////////////////////////////////////////////////////////////////////////
// Filename    : PlayerMailbox.h
// Description : Run a command against a logged-in player on the thread
//               that owns the player, under whatever protects that owner.
//
//               This is how the cross-thread packet handlers (SG/LG/GG, on
//               the SharedServerManager / LoginServerManager threads) reach
//               a PlayerCreature without racing its owner. Before, they
//               mutated gold, guild id and kick flags holding only the
//               PCFinder lock, which serialises lookup and removal but not
//               the zone tick -- the "SG/LG/GG handlers" entry in
//               docs/threading/closed-violations.md. Sending a packet to the player's socket is
//               still fine from any thread and does not need this.
//
//               The box belongs to the GamePlayer, not to a zone group, and
//               that is the point: a player's owner changes over its life --
//               the main thread's IncomingPlayerManager during login and
//               zone transfer, a ZonePlayerManager on a zone thread while
//               in a zone group -- and the creature's own zone pointer does
//               not say which: it is set as soon as the character loads,
//               before any zone thread owns it, and stays on the old zone
//               during a transfer. So the box moves with the player and
//               keeps its commands in posting order across group changes.
//               Each owner drains it from the one place it processes the
//               players it owns -- the zone thread under the group mutex,
//               the main thread as the sole thread that touches its
//               players (IncomingPlayerManager takes no lock in that loop):
//
//                 * ZonePlayerManager::processCommands, on the zone thread
//                   under the group mutex, runs everything -- the creature
//                   is in one of that group's zones, so zone state is safe;
//                 * IncomingPlayerManager::processCommands, on the main
//                   thread, runs only Scope::Player commands (kick flags),
//                   because while it owns the player the zone the creature
//                   points at is ticking on another thread. Scope::Zone
//                   commands wait, in order, until a zone thread owns the
//                   player again; a Scope::Player command may overtake them.
//
//               A player that logs out with commands pending runs their
//               ifGone handlers instead, from ~GamePlayer right after the
//               PCFinder removal that makes it "gone".
//////////////////////////////////////////////////////////////////////////////

#ifndef DARKEDEN_PLAYER_MAILBOX_H
#define DARKEDEN_PLAYER_MAILBOX_H

#include <cstddef>
#include <functional>
#include <string>

#include "Mailbox.h"

class GamePlayer;
class Player;
class PlayerCreature;
class ZonePlayerManager;

namespace de {

using PlayerCommand = std::function<void(PlayerCreature& pc, Player& player)>;
using GoneCommand = std::function<void()>;

// What a command may touch, i.e. which owner may run it.
enum class Scope {
    Zone,  // creature state, the creature's zone: only a zone thread that owns the player
    Player // GamePlayer-only state (kick flags): whichever thread owns the player
};

struct PostedPlayerCommand {
    PlayerCommand command;
    GoneCommand ifGone; // may be empty
    Scope scope;
};

using PlayerCommandMailbox = Mailbox<PostedPlayerCommand>;

// Posts `command` to the player named `name` and returns true. Returns
// false, running nothing, when no logged-in PC of that name exists at the
// time of the call -- the caller's "offline" branch belongs there. Two
// things the caller must accept:
//
//  * The command runs later, on the owner's next pass over the player, and
//    only if the player is still logged in then. A player that logged out
//    in between is skipped -- unless `ifGone` is given, which then runs in its
//    place (on the thread that destroys the player), for the handlers whose
//    offline branch does something material, like charging a fee in the
//    database instead of in memory. Exactly one of the two ever runs: the
//    post and the PCFinder removal both take the PCFinder lock, and the
//    abandon follows the removal.
//  * Everything the command needs must be captured by value. A Guild* or
//    GuildMember* captured by pointer may be deleted before the command
//    runs (SGDeleteGuildOK deletes both); capture ids and names and look
//    the objects up again inside.
//
// The command runs without the PCFinder lock (its owner's lock is what
// protects it), so it may call postToPlayer itself, e.g. for another
// player. A command that throws -- any type -- is logged and dropped; the
// rest of the batch still runs.
bool postToPlayer(const std::string& name, PlayerCommand command, GoneCommand ifGone = nullptr,
                  Scope scope = Scope::Zone);

// postToPlayer for a player known by its account id (Player::getID())
// rather than its character's name, as the login link's replies know it.
// The same rules: the account's logged-in PC is looked up under the
// PCFinder lock, and false means none was found and nothing was posted.
// A player keeps its PCFinder entry from character load until it is
// destroyed, so this also reaches a player that is logging out through the
// main thread's IncomingPlayerManager.
bool postToAccount(const std::string& playerID, PlayerCommand command, GoneCommand ifGone = nullptr,
                   Scope scope = Scope::Zone);

// Owner side; see the file comment for which owner runs what. Both return
// the number of commands run. When the creature's zone group is not
// `owner`'s -- a listing mismatch the manager itself logs as ZPMCheck -- the
// zone form runs only Scope::Player commands, like the main thread, rather
// than mutate a zone another thread is ticking. A backlog the owner cannot
// run is logged (rate-limited) so a box that never drains is visible.
// abandonPlayerMailbox runs the ifGone handlers of everything pending and
// drops the commands; called at logout.
std::size_t drainPlayerMailbox(GamePlayer& player, const ZonePlayerManager& owner);
std::size_t drainPlayerMailboxOnMainThread(GamePlayer& player);
std::size_t abandonPlayerMailbox(GamePlayer& player);

} // namespace de

#endif // DARKEDEN_PLAYER_MAILBOX_H
