#include "GamePlayerHandoff.h"

#include "DescriptorPollSet.h"

namespace de {
SOCKET enqueueRegisteredGamePlayer(std::span<Player*> players, std::list<GamePlayer*>& destination,
                                   GamePlayer& player) {
    const auto descriptor = player.getSocket()->getSOCKET();
    if (!fitsDescriptorTable(descriptor, static_cast<int>(players.size())))
        throw OutOfBoundException();
    if (players[descriptor] != &player)
        throw NoSuchElementException();
    destination.push_back(&player);
    players[descriptor] = nullptr;
    return descriptor;
}

DescriptorRange gamePlayerDescriptorRange(std::span<Player* const> players, int listener) noexcept {
    auto range = kEmptyDescriptorRange;
    for (int descriptor = 0; descriptor < static_cast<int>(players.size()); ++descriptor) {
        if (players[descriptor] || descriptor == listener) {
            if (range.empty())
                range.first = descriptor;
            range.last = descriptor;
        }
    }
    return range;
}

GameConnection takeFirstGamePlayer(std::list<GamePlayer*>& source) noexcept {
    if (source.empty())
        return nullptr;
    GameConnection player(source.front());
    source.pop_front();
    return player;
}

void releaseGamePlayers(std::span<Player*> players, std::list<GamePlayer*>& incoming, std::list<GamePlayer*>& outgoing,
                        DescriptorPollSet& poll, bool disconnect) noexcept {
    const auto release = [&](GamePlayer* raw) {
        incoming.remove(raw);
        outgoing.remove(raw);
        if (!raw)
            return;
        for (std::size_t descriptor = 0; descriptor < players.size(); ++descriptor) {
            if (players[descriptor] == raw) {
                players[descriptor] = nullptr;
                poll.unwatch(descriptor);
            }
        }
        GameConnection player(raw);
        if (disconnect) {
            try {
                player->disconnect();
            } catch (...) {
                // One failed logout must not leave the remaining owners behind.
            }
        }
    };
    for (auto* player : players)
        if (player)
            release(static_cast<GamePlayer*>(player));
    while (!incoming.empty())
        release(incoming.front());
    while (!outgoing.empty())
        release(outgoing.front());
}
} // namespace de
