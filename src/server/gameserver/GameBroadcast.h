#ifndef DARKEDEN_GAME_BROADCAST_H
#define DARKEDEN_GAME_BROADCAST_H

#include <functional>
#include <list>
#include <memory>
#include <span>

class BroadcastFilter;
class GamePlayer;
class Packet;
class Player;
class SocketOutputStream;
class Throwable;

namespace de {
using BroadcastErrorReporter = std::function<void(const Throwable&)>;

// An encoded snapshot and its optional filter clone. Neither the packet nor
// the original filter is retained. Preparation preserves writeHeaderNBody's
// framing, including its literal '0' sequence byte; it does not encrypt bytes.
class GameBroadcast {
public:
    ~GameBroadcast();
    GameBroadcast(GameBroadcast&&) noexcept;
    GameBroadcast& operator=(GameBroadcast&&) noexcept;
    GameBroadcast(const GameBroadcast&) = delete;
    GameBroadcast& operator=(const GameBroadcast&) = delete;

    std::span<const char> bytes() const noexcept;
    // A null filter selects every player. Predicate failures propagate;
    // Throwable send failures are reported and dispatch may continue. An
    // empty reporter disables those diagnostics. Recipients only borrow the
    // stream during sendStream and must not mutate or retain it.
    void sendTo(GamePlayer& player, const BroadcastErrorReporter& report = {}) const;

private:
    friend GameBroadcast makeGameBroadcast(const Packet&, BroadcastFilter*);
    GameBroadcast(std::unique_ptr<SocketOutputStream> stream, std::unique_ptr<BroadcastFilter> filter) noexcept;
    std::unique_ptr<SocketOutputStream> m_Stream;
    std::unique_ptr<BroadcastFilter> m_Filter;
};

// Failures in encoding or cloning release partial resources without publishing
// a message. A null filter means unfiltered delivery.
GameBroadcast makeGameBroadcast(const Packet& packet, BroadcastFilter* filter = nullptr);

// Requires exclusive queue access and the zone thread's ownership of players.
// Consume a message when dispatch starts: an uncaught predicate/send/reporting
// failure stops this flush without replaying its partially delivered message.
// Later messages remain queued for retry. Within a message, caught Throwable
// send failures preserve the existing best-effort delivery to other players.
void flushGameBroadcasts(std::list<GameBroadcast>& messages, std::span<Player* const> players,
                         const BroadcastErrorReporter& report = {});
} // namespace de

#endif
