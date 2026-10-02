#ifndef DARKEDEN_GAME_PLAYER_HANDOFF_H
#define DARKEDEN_GAME_PLAYER_HANDOFF_H

#include <exception>
#include <list>
#include <span>
#include <utility>

#include "DescriptorTable.h"
#include "GameConnection.h"

namespace de {
class DescriptorPollSet;

// The caller holds the source table and destination queue locks. Validate
// identity and allocate the queue node before clearing the source slot. On
// failure both containers are unchanged. The returned descriptor lets the
// caller finish its nonthrowing count/range/poll bookkeeping under those locks.
SOCKET enqueueRegisteredGamePlayer(std::span<Player*> players, std::list<GamePlayer*>& destination, GamePlayer& player);

DescriptorRange gamePlayerDescriptorRange(std::span<Player* const> players, int listener = -1) noexcept;

// Accept must take ownership only on success. Neither the transfer nor the
// caller may dereference a player after handing it to another thread. The
// source queue stays intact when acceptance throws. Empty/null entries carry
// no owner; consuming a null entry allows the rest of the queue to progress.
template <typename Accept> void transferFirstGamePlayer(std::list<GamePlayer*>& source, Accept&& accept) {
    if (source.empty())
        return;
    auto* player = source.front();
    if (player)
        std::forward<Accept>(accept)(player);
    source.pop_front();
}

// Attempt each current entry once. A failed transfer keeps its owner and moves
// behind the remaining entries without allocating, so one bad destination does
// not starve other players. Even throwing diagnostics leave every owner queued.
template <typename Accept, typename Report>
void transferGamePlayerBatch(std::list<GamePlayer*>& source, Accept&& accept, Report&& report) {
    for (auto remaining = source.size(); remaining != 0; --remaining) {
        try {
            transferFirstGamePlayer(source, accept);
        } catch (...) {
            source.splice(source.end(), source, source.begin());
            report(std::current_exception());
        }
    }
}

// Use a local owner for a queued player whose session is ending, including
// through failures in logout or its diagnostics. Requires exclusive access.
GameConnection takeFirstGamePlayer(std::list<GamePlayer*>& source) noexcept;

// Quiescent teardown, shared by incoming and zone managers. Remove all table
// and queue references before destroying an owner. Explicit clear attempts
// disconnect; destruction only ends the local session and releases resources.
void releaseGamePlayers(std::span<Player*> players, std::list<GamePlayer*>& incoming, std::list<GamePlayer*>& outgoing,
                        DescriptorPollSet& poll, bool disconnect) noexcept;
} // namespace de

#endif
