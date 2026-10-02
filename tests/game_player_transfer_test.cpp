#include <fcntl.h>

#include <cstdlib>
#include <list>
#include <memory>
#include <new>
#include <stdexcept>

#include <gtest/gtest.h>

#include "DescriptorPollSet.h"
#include "GCReconnectLogin.h"
#include "GamePlayerHandoff.h"
#include "PlayerManager.h"
#include "support/AllocationProbe.h"
#include "support/LoopbackListener.h"

namespace {
class CountedPacket : public GCReconnectLogin {
public:
    explicit CountedPacket(unsigned& destroyed) : destroyed(destroyed) {}
    ~CountedPacket() override {
        ++destroyed;
    }

private:
    unsigned& destroyed;
};

de::GameConnection connection(unsigned& destroyed) {
    auto player = de::makeGameConnection(std::make_unique<Socket>());
    player->setReconnectPacket(new CountedPacket(destroyed));
    return player;
}

TEST(GamePlayerTransfer, AllocationFailuresRetainQueuedOwnersWithoutStarvingTheRemainingBatch) {
    for (std::size_t failAt = 1; failAt <= 8; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                const int available = nextSocketDescriptor();
                unsigned destroyed = 0;
                de::DescriptorPollSet poll(0);
                std::list<GamePlayer*> source;
                std::list<GamePlayer*> destination;
                GamePlayer* order[3];
                for (auto& raw : order) {
                    auto player = connection(destroyed);
                    raw = player.get();
                    source.push_back(raw);
                    player.release();
                }
                unsigned refused = 0;
                unsigned attempted = 0;
                AllocationProbe probe(failAt);
                de::transferGamePlayerBatch(
                    source,
                    [&](GamePlayer* player) {
                        if (player != order[attempted++])
                            std::_Exit(1);
                        destination.push_back(player);
                    },
                    [&](std::exception_ptr error) {
                        try {
                            std::rethrow_exception(error);
                        } catch (const std::bad_alloc&) {
                            ++refused;
                        }
                    });
                probe.stopFailing();
                if (attempted != 3 || source.size() != refused || source.size() + destination.size() != 3 ||
                    destroyed != 0 || (failAt == 8 && probe.rejected()))
                    std::_Exit(2);
                std::size_t matched = 0;
                for (auto* player : destination) {
                    while (matched < 3 && order[matched] != player)
                        ++matched;
                    if (matched == 3)
                        std::_Exit(3);
                    ++matched;
                }
                auto* retry = source.empty() ? nullptr : source.front();
                de::transferGamePlayerBatch(
                    source, [&](GamePlayer* player) { destination.push_back(player); },
                    [](std::exception_ptr) { std::_Exit(4); });
                if (!source.empty() || destination.size() != 3 || (retry && destination.back() != retry))
                    std::_Exit(5);
                de::releaseGamePlayers({}, source, destination, poll, false);
                std::_Exit(destroyed == 3 && probe.outstanding() == 0 && nextSocketDescriptor() == available ? 0 : 6);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(GamePlayerTransfer, ThrowingDiagnosticsPreserveFailedAndUnattemptedOwnersForRetry) {
    ASSERT_EXIT(
        {
            const int available = nextSocketDescriptor();
            unsigned destroyed = 0;
            AllocationProbe probe;
            {
                de::DescriptorPollSet poll(0);
                std::list<GamePlayer*> source;
                std::list<GamePlayer*> destination;
                GamePlayer* order[3];
                for (auto& raw : order) {
                    auto player = connection(destroyed);
                    raw = player.get();
                    source.push_back(raw);
                    player.release();
                }
                unsigned reports = 0;
                bool threw = false;
                try {
                    de::transferGamePlayerBatch(
                        source, [](GamePlayer*) { throw std::runtime_error("destination unavailable"); },
                        [&](std::exception_ptr error) {
                            try {
                                std::rethrow_exception(error);
                            } catch (const std::runtime_error&) {
                                ++reports;
                            }
                            throw std::bad_alloc();
                        });
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                if (!threw || reports != 1 || source.size() != 3 || source.front() != order[1] ||
                    source.back() != order[0] || destroyed != 0)
                    std::_Exit(1);
                de::transferGamePlayerBatch(
                    source, [&](GamePlayer* player) { destination.push_back(player); },
                    [](std::exception_ptr) { std::_Exit(2); });
                if (!source.empty() || destination.front() != order[1] || destination.back() != order[0])
                    std::_Exit(3);
                de::releaseGamePlayers({}, source, destination, poll, false);
            }
            std::_Exit(destroyed == 3 && probe.outstanding() == 0 && nextSocketDescriptor() == available ? 0 : 4);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(GamePlayerTransfer, AcceptanceMayDestroyThePlayerAndNullEntriesDoNotBlockLaterOwners) {
    ASSERT_EXIT(
        {
            unsigned destroyed = 0;
            const int available = nextSocketDescriptor();
            AllocationProbe probe;
            {
                std::list<GamePlayer*> source;
                auto player = connection(destroyed);
                source.push_back(nullptr);
                source.push_back(player.get());
                player.release();
                unsigned accepted = 0;
                const auto accept = [&](GamePlayer* raw) {
                    de::GameConnection owner(raw);
                    ++accepted;
                };
                de::transferFirstGamePlayer(source, accept);
                if (accepted != 0 || source.size() != 1)
                    std::_Exit(1);
                de::transferFirstGamePlayer(source, accept);
                de::transferFirstGamePlayer(source, accept);
                if (accepted != 1 || !source.empty())
                    std::_Exit(2);
            }
            std::_Exit(destroyed == 1 && probe.outstanding() == 0 && nextSocketDescriptor() == available ? 0 : 3);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(GamePlayerTransfer, TableTransferChecksBoundsAndPlayerIdentityBeforeChangingEitherOwner) {
    ASSERT_EXIT(
        {
            unsigned destroyed = 0;
            const int available = nextSocketDescriptor();
            AllocationProbe probe;
            {
                Player* table[PlayerManager::nMaxPlayers]{};
                std::list<GamePlayer*> pending;
                auto first = connection(destroyed);
                auto* registered = first.get();
                const int descriptor = first->getSocket()->getSOCKET();
                first->getSocket()->close();
                auto replacement = connection(destroyed);
                if (replacement->getSocket()->getSOCKET() != descriptor)
                    std::_Exit(1);
                table[descriptor] = first.release();
                bool refused = false;
                try {
                    de::enqueueRegisteredGamePlayer({}, pending, *replacement);
                } catch (const OutOfBoundException&) {
                    refused = true;
                }
                if (!refused)
                    std::_Exit(2);
                refused = false;
                try {
                    de::enqueueRegisteredGamePlayer(table, pending, *replacement);
                } catch (const NoSuchElementException&) {
                    refused = true;
                }
                if (!refused || !pending.empty() || table[descriptor] != registered)
                    std::_Exit(3);
                const auto occupied = de::gamePlayerDescriptorRange(table);
                if (occupied.first != descriptor || occupied.last != descriptor ||
                    de::enqueueRegisteredGamePlayer(table, pending, *registered) != descriptor || table[descriptor] ||
                    pending.size() != 1 || !de::gamePlayerDescriptorRange(table).empty())
                    std::_Exit(4);
                const auto withListener = de::gamePlayerDescriptorRange(table, descriptor);
                if (withListener.first != descriptor || withListener.last != descriptor ||
                    !de::gamePlayerDescriptorRange(table, PlayerManager::nMaxPlayers).empty())
                    std::_Exit(5);
                auto owner = de::takeFirstGamePlayer(pending);
                owner.reset();
                if (!pending.empty() || ::fcntl(descriptor, F_GETFD) < 0 || destroyed != 1)
                    std::_Exit(6);
            }
            std::_Exit(destroyed == 2 && probe.outstanding() == 0 && nextSocketDescriptor() == available ? 0 : 7);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(GamePlayerTransfer, QuiescentReleaseRemovesAllAliasedReferencesBeforeDestroyingEachOwnerOnce) {
    ASSERT_EXIT(
        {
            const int available = nextSocketDescriptor();
            unsigned destroyed = 0;
            AllocationProbe probe;
            {
                Player* table[PlayerManager::nMaxPlayers]{};
                de::DescriptorPollSet poll(PlayerManager::nMaxPlayers);
                std::list<GamePlayer*> incoming;
                std::list<GamePlayer*> outgoing;
                for (int index = 0; index < 3; ++index) {
                    auto player = connection(destroyed);
                    const auto descriptor = player->getSocket()->getSOCKET();
                    table[descriptor] = player.get();
                    poll.watch(descriptor, de::DescriptorPollSet::kWrite);
                    incoming.push_back(player.get());
                    outgoing.push_back(player.get());
                    outgoing.push_back(player.get());
                    player.release();
                }
                incoming.push_back(nullptr);
                outgoing.push_back(nullptr);
                de::releaseGamePlayers(table, incoming, outgoing, poll, false);
                if (!incoming.empty() || !outgoing.empty() || destroyed != 3 ||
                    !de::gamePlayerDescriptorRange(table).empty())
                    std::_Exit(1);
                de::releaseGamePlayers(table, incoming, outgoing, poll, false);
            }
            std::_Exit(destroyed == 3 && probe.outstanding() == 0 && nextSocketDescriptor() == available ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}
} // namespace
