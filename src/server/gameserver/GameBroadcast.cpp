#include "GameBroadcast.h"

#include <utility>

#include "Assert.h"
#include "BroadcastFilter.h"
#include "GamePlayer.h"
#include "Packet.h"
#include "SocketOutputStream.h"

namespace de {
GameBroadcast::GameBroadcast(std::unique_ptr<SocketOutputStream> stream,
                             std::unique_ptr<BroadcastFilter> filter) noexcept
    : m_Stream(std::move(stream)), m_Filter(std::move(filter)) {}

GameBroadcast::~GameBroadcast() = default;
GameBroadcast::GameBroadcast(GameBroadcast&&) noexcept = default;
GameBroadcast& GameBroadcast::operator=(GameBroadcast&&) noexcept = default;

std::span<const char> GameBroadcast::bytes() const noexcept {
    if (!m_Stream)
        return {};
    return {m_Stream->getBuffer(), m_Stream->length()};
}

void GameBroadcast::sendTo(GamePlayer& player, const BroadcastErrorReporter& report) const {
    if (!m_Stream || (m_Filter && !m_Filter->isSatisfy(&player)))
        return;
    try {
        player.sendStream(m_Stream.get());
    } catch (const Throwable& error) {
        if (report)
            report(error);
    }
}

GameBroadcast makeGameBroadcast(const Packet& packet, BroadcastFilter* filter) {
    auto stream = std::make_unique<SocketOutputStream>(nullptr, szPacketHeader + packet.getPacketSize());
    packet.writeHeaderNBody(*stream);
    std::unique_ptr<BroadcastFilter> clone;
    if (filter)
        clone.reset(filter->Clone());
    return GameBroadcast(std::move(stream), std::move(clone));
}

void flushGameBroadcasts(std::list<GameBroadcast>& messages, std::span<Player* const> players,
                         const BroadcastErrorReporter& report) {
    while (!messages.empty()) {
        auto message = std::move(messages.front());
        messages.pop_front();
        for (auto* player : players) {
            if (!player)
                continue;
            auto* gamePlayer = dynamic_cast<GamePlayer*>(player);
            Assert(gamePlayer != nullptr);
            message.sendTo(*gamePlayer, report);
        }
    }
}
} // namespace de
