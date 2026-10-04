#include <cstdlib>
#include <cstring>
#include <list>
#include <memory>
#include <new>

#include <gtest/gtest.h>

#include "BroadcastFilter.h"
#include "GCDisconnect.h"
#include "GameBroadcast.h"
#include "GameConnection.h"
#include "SocketOutputStream.h"
#include "ZonePlayerManager.h"
#include "support/AllocationProbe.h"

namespace {
struct FilterLifetime {
    unsigned cloned = 0;
    unsigned destroyed = 0;
    unsigned checked = 0;
    unsigned failCheckAt = 0;
};

class ObservedFilter : public BroadcastFilter {
public:
    explicit ObservedFilter(FilterLifetime& lifetime, bool failClone = false, bool cloned = false)
        : lifetime(lifetime), failClone(failClone), cloned(cloned) {}
    ~ObservedFilter() override {
        if (cloned)
            ++lifetime.destroyed;
    }
    bool isSatisfy(GamePlayer*) override {
        if (++lifetime.checked == lifetime.failCheckAt)
            throw Error("broadcast predicate failed");
        return selected;
    }
    BroadcastFilter* Clone() override {
        if (failClone)
            throw std::bad_alloc();
        auto clone = std::make_unique<ObservedFilter>(lifetime, false, true);
        clone->selected = selected;
        ++lifetime.cloned;
        return clone.release();
    }
    bool selected = true;

private:
    FilterLifetime& lifetime;
    bool failClone;
    bool cloned;
};

class ThrowingPacket : public GCDisconnect {
public:
    void write(SocketOutputStream& stream) const override {
        stream.write("x", 1);
        throw std::bad_alloc();
    }
};

struct RecipientState {
    unsigned attempted = 0;
    unsigned delivered = 0;
    unsigned failAt = 0;
    bool failWithThrowable = false;
    std::size_t lengths[8]{};
    char frames[8][128]{};
};

class ObservedRecipient : public GamePlayer {
public:
    ObservedRecipient(Socket* socket, RecipientState& state) : GamePlayer(socket), state(state) {}
    void sendStream(SocketOutputStream* stream) override {
        if (++state.attempted == state.failAt) {
            if (state.failWithThrowable)
                throw IOException("broadcast send failed");
            throw std::bad_alloc();
        }
        if (state.delivered >= 8 || stream->length() > sizeof(state.frames[0]))
            std::_Exit(90);
        state.lengths[state.delivered] = stream->length();
        std::memcpy(state.frames[state.delivered], stream->getBuffer(), stream->length());
        ++state.delivered;
    }

private:
    RecipientState& state;
};

de::GameConnection recipient(RecipientState& state) {
    auto socket = std::make_unique<Socket>();
    de::GameConnection player(new ObservedRecipient(socket.release(), state));
    player->setPlayerStatus(GPS_NORMAL);
    return player;
}

bool receivedMessage(const RecipientState& state, unsigned index, const char* message) {
    GCDisconnect packet;
    packet.setMessage(message);
    SocketOutputStream expected(nullptr, 128);
    packet.writeHeaderNBody(expected);
    return index < state.delivered && state.lengths[index] == expected.length() &&
           std::memcmp(state.frames[index], expected.getBuffer(), expected.length()) == 0;
}

TEST(GameBroadcast, SnapshotKeepsOriginalBytesAndClonedSelectionAfterBorrowedInputsDie) {
    ASSERT_EXIT(
        {
            FilterLifetime lifetime;
            RecipientState state;
            AllocationProbe probe;
            {
                auto prepared = [&] {
                    GCDisconnect packet;
                    packet.setMessage("broadcast");
                    ObservedFilter filter(lifetime);
                    auto prepared = de::makeGameBroadcast(packet, &filter);
                    packet.setMessage("changed");
                    filter.selected = false;
                    return prepared;
                }();
                // Packet id 233, measured body size 10, literal sequence '0',
                // one-byte string length 9 and the original body.
                const char expected[] = "\xE9\x00\x0A\x00\x00\x00\x30\x09"
                                        "broadcast";
                if (prepared.bytes().size() != sizeof(expected) - 1 ||
                    std::memcmp(prepared.bytes().data(), expected, sizeof(expected) - 1) != 0)
                    std::_Exit(1);
                auto player = recipient(state);
                prepared.sendTo(*player);
                if (state.delivered != 1 || !receivedMessage(state, 0, "broadcast") || lifetime.checked != 1)
                    std::_Exit(2);
            }
            std::_Exit(lifetime.cloned == 1 && lifetime.destroyed == 1 && probe.outstanding() == 0 ? 0 : 3);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(GameBroadcast, RejectedFiltersSkipDeliveryAndNullFiltersDeliverWithoutDiagnostics) {
    ASSERT_EXIT(
        {
            FilterLifetime lifetime;
            RecipientState state;
            AllocationProbe probe;
            {
                auto player = recipient(state);
                GCDisconnect packet;
                packet.setMessage("filter");
                ObservedFilter filter(lifetime);
                filter.selected = false;
                auto excluded = de::makeGameBroadcast(packet, &filter);
                excluded.sendTo(*player);
                if (state.attempted != 0 || lifetime.checked != 1)
                    std::_Exit(1);
                auto unfiltered = de::makeGameBroadcast(packet);
                unfiltered.sendTo(*player);
                if (state.delivered != 1 || !receivedMessage(state, 0, "filter"))
                    std::_Exit(2);
                state.failAt = 2;
                state.failWithThrowable = true;
                unfiltered.sendTo(*player);
                if (state.attempted != 2 || state.delivered != 1)
                    std::_Exit(3);
            }
            std::_Exit(lifetime.cloned == 1 && lifetime.destroyed == 1 && probe.outstanding() == 0 ? 0 : 4);
        },
        ::testing::ExitedWithCode(0), "");
}

enum class FlushFailure { SendAllocation, ReportedSend, Diagnostic, Predicate };

void checkFlushFailure(FlushFailure failure) {
    FilterLifetime lifetime;
    RecipientState states[3];
    AllocationProbe probe;
    {
        de::GameConnection owners[3];
        Player* players[3]{};
        for (unsigned index = 0; index < 3; ++index) {
            owners[index] = recipient(states[index]);
            players[index] = owners[index].get();
        }
        ObservedFilter filter(lifetime);
        std::list<de::GameBroadcast> messages;
        for (const char* text : {"first", "partial", "last"}) {
            GCDisconnect packet;
            packet.setMessage(text);
            messages.push_back(de::makeGameBroadcast(packet, &filter));
        }
        if (failure == FlushFailure::Predicate)
            lifetime.failCheckAt = 5;
        else {
            states[1].failAt = 2;
            states[1].failWithThrowable = failure != FlushFailure::SendAllocation;
        }
        unsigned reports = 0;
        const auto report = [&](const Throwable& error) {
            if (!dynamic_cast<const IOException*>(&error))
                std::_Exit(80);
            ++reports;
            if (failure == FlushFailure::Diagnostic)
                throw std::bad_alloc();
        };
        bool threw = false;
        try {
            de::flushGameBroadcasts(messages, players, report);
        } catch (const std::bad_alloc&) {
            if (failure != FlushFailure::SendAllocation && failure != FlushFailure::Diagnostic)
                std::_Exit(81);
            threw = true;
        } catch (const Error&) {
            if (failure != FlushFailure::Predicate)
                std::_Exit(82);
            threw = true;
        }
        if (failure == FlushFailure::ReportedSend) {
            if (threw || !messages.empty() || reports != 1 || states[0].delivered != 3 || states[1].delivered != 2 ||
                states[2].delivered != 3)
                std::_Exit(1);
        } else {
            if (!threw || messages.size() != 1 || lifetime.destroyed != 2 || states[0].delivered != 2 ||
                states[1].delivered != 1 || states[2].delivered != 1 ||
                reports != (failure == FlushFailure::Diagnostic ? 1u : 0u))
                std::_Exit(2);
            states[1].failAt = 0;
            lifetime.failCheckAt = 0;
            de::flushGameBroadcasts(messages, players, report);
            if (!messages.empty() || states[0].delivered != 3 || states[1].delivered != 2 || states[2].delivered != 2)
                std::_Exit(3);
        }
        if (!receivedMessage(states[0], 0, "first") || !receivedMessage(states[0], 1, "partial") ||
            !receivedMessage(states[0], 2, "last") || !receivedMessage(states[1], 0, "first") ||
            !receivedMessage(states[1], 1, "last") || !receivedMessage(states[2], states[2].delivered - 1, "last"))
            std::_Exit(4);
        de::flushGameBroadcasts(messages, players, report);
    }
    std::_Exit(lifetime.cloned == 3 && lifetime.destroyed == 3 && probe.outstanding() == 0 ? 0 : 5);
}

TEST(GameBroadcast, FlushConsumesStartedMessagesAndKeepsLaterEntriesAcrossPredicateSendAndDiagnosticFailures) {
    for (auto failure : {FlushFailure::SendAllocation, FlushFailure::ReportedSend, FlushFailure::Diagnostic,
                         FlushFailure::Predicate}) {
        SCOPED_TRACE(static_cast<int>(failure));
        ASSERT_EXIT(checkFlushFailure(failure), ::testing::ExitedWithCode(0), "");
    }
}

TEST(GameBroadcastQueue, EveryAllocationFailurePreservesExistingMessagesAndAllowsRetryWithoutLeaking) {
    for (std::size_t failAt = 1; failAt <= 16; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                auto manager = std::make_unique<ZonePlayerManager>();
                RecipientState state;
                auto player = recipient(state);
                manager->addPlayer(player.get());
                player.release();
                FilterLifetime lifetime;
                ObservedFilter filter(lifetime);
                GCDisconnect before;
                before.setMessage("first");
                GCDisconnect candidate;
                candidate.setMessage("candidate");
                GCDisconnect after;
                after.setMessage("last");
                manager->pushBroadcastPacket(&before, &filter);
                AllocationProbe probe(failAt);
                bool threw = false;
                try {
                    manager->pushBroadcastPacket(&candidate, &filter);
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                probe.stopFailing();
                // Diagnostic allocation failures are contained; delivery below
                // still verifies the complete candidate when queuing succeeds.
                if ((threw && !probe.rejected()) || (failAt == 16 && probe.rejected()) ||
                    (threw && probe.outstanding() != 0))
                    std::_Exit(1);
                manager->pushBroadcastPacket(&after, &filter);
                manager->flushBroadcastPacket();
                if (state.delivered != (threw ? 2u : 3u) || !receivedMessage(state, 0, "first") ||
                    !receivedMessage(state, state.delivered - 1, "last") ||
                    (!threw && !receivedMessage(state, 1, "candidate")))
                    std::_Exit(2);
                if (threw) {
                    manager->pushBroadcastPacket(&candidate, &filter);
                    manager->flushBroadcastPacket();
                    if (state.delivered != 3 || !receivedMessage(state, 2, "candidate"))
                        std::_Exit(3);
                }
                manager.reset();
                std::_Exit(lifetime.cloned == lifetime.destroyed && probe.outstanding() == 0 ? 0 : 4);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(GameBroadcastQueue, RetryDoesNotReplayDispatchedMessagesAfterAnUncaughtSendFailure) {
    ASSERT_EXIT(
        {
            auto manager = std::make_unique<ZonePlayerManager>();
            RecipientState first;
            RecipientState second;
            second.failAt = 2;
            for (auto* state : {&first, &second}) {
                auto socket = std::make_unique<Socket>();
                de::GameConnection player(new ObservedRecipient(socket.release(), *state));
                player->setPlayerStatus(GPS_NORMAL);
                manager->addPlayer(player.get());
                player.release();
            }
            FilterLifetime lifetime;
            ObservedFilter filter(lifetime);
            for (const char* message : {"first", "partial", "last"}) {
                GCDisconnect packet;
                packet.setMessage(message);
                manager->pushBroadcastPacket(&packet, &filter);
            }
            bool threw = false;
            try {
                manager->flushBroadcastPacket();
            } catch (const std::bad_alloc&) {
                threw = true;
            }
            if (!threw || first.delivered != 2 || second.delivered != 1)
                std::_Exit(1);
            second.failAt = 0;
            manager->flushBroadcastPacket();
            if (first.delivered != 3 || second.delivered != 2)
                std::_Exit(2);
            manager.reset();
            std::_Exit(lifetime.cloned == 3 && lifetime.destroyed == 3 ? 0 : 3);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(GameBroadcastQueue, DefaultNullFilterCanBeQueuedAndFlushedWithoutWorldOrDatabaseStartup) {
    ASSERT_EXIT(
        {
            AllocationProbe probe;
            {
                ZonePlayerManager manager;
                GCDisconnect packet;
                packet.setMessage("broadcast");
                manager.pushBroadcastPacket(&packet);
                manager.flushBroadcastPacket();
            }
            std::_Exit(probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(GameBroadcastQueue, FlushReleasesItsStreamAndFilterCloneWhileKeepingTheBorrowedInputs) {
    ASSERT_EXIT(
        {
            ZonePlayerManager manager;
            FilterLifetime lifetime;
            ObservedFilter filter(lifetime);
            GCDisconnect packet;
            packet.setMessage("broadcast");
            AllocationProbe probe;
            manager.pushBroadcastPacket(&packet, &filter);
            if (lifetime.cloned != 1 || lifetime.destroyed != 0)
                std::_Exit(1);
            manager.flushBroadcastPacket();
            if (lifetime.destroyed != 1 || probe.outstanding() != 0 || packet.getMessage() != "broadcast")
                std::_Exit(2);
            manager.flushBroadcastPacket();
            std::_Exit(lifetime.destroyed == 1 && lifetime.checked == 0 ? 0 : 3);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(GameBroadcastQueue, ManagerDestructionReleasesPendingStreamsAndFilterClones) {
    ASSERT_EXIT(
        {
            auto manager = std::make_unique<ZonePlayerManager>();
            FilterLifetime lifetime;
            ObservedFilter filter(lifetime);
            GCDisconnect packet;
            packet.setMessage("pending");
            AllocationProbe probe;
            manager->pushBroadcastPacket(&packet, &filter);
            manager.reset();
            std::_Exit(lifetime.cloned == 1 && lifetime.destroyed == 1 && probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(GameBroadcastQueue, FailedPacketEncodingReleasesThePartialStreamWithoutPublishingAnEntry) {
    ASSERT_EXIT(
        {
            ZonePlayerManager manager;
            FilterLifetime lifetime;
            ObservedFilter filter(lifetime);
            ThrowingPacket packet;
            AllocationProbe probe;
            bool threw = false;
            try {
                manager.pushBroadcastPacket(&packet, &filter);
            } catch (const std::bad_alloc&) {
                threw = true;
            }
            manager.flushBroadcastPacket();
            std::_Exit(threw && lifetime.cloned == lifetime.destroyed && lifetime.checked == 0 &&
                               probe.outstanding() == 0
                           ? 0
                           : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(GameBroadcastQueue, FailedFilterCloningReleasesThePreparedStreamWithoutPublishingAnEntry) {
    ASSERT_EXIT(
        {
            ZonePlayerManager manager;
            FilterLifetime lifetime;
            ObservedFilter filter(lifetime, true);
            GCDisconnect packet;
            packet.setMessage("broadcast");
            AllocationProbe probe;
            bool threw = false;
            try {
                manager.pushBroadcastPacket(&packet, &filter);
            } catch (const std::bad_alloc&) {
                threw = true;
            }
            manager.flushBroadcastPacket();
            std::_Exit(threw && lifetime.cloned == 0 && probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}
} // namespace
