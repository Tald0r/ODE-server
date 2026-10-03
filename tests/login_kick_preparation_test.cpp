#include <array>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "LCLoginError.h"
#include "LoginKickPreparation.h"
#include "LoginPlayer.h"
#include "Socket.h"
#include "SocketOutputStream.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginAccountRepository.h"
#include "support/FakeLoginCharacterRepository.h"

namespace {

struct KickCache {
    WorldID_t world;
    ServerGroupID_t group;
    uint slot;
    bool ready;
    PlayerStatus status;
    bool operator==(const KickCache&) const = default;
};

KickCache cache(const LoginPlayer& player) {
    return {player.getWorldID(), player.getGroupID(), player.getLastSlot(), player.isSetWorldGroupID(),
            player.getPlayerStatus()};
}

constexpr KickCache kInitial{3, 4, 2, false, LPS_WAITING_FOR_GL_KICK_VERIFY};
constexpr KickCache kPrepared{7, 9, 3, true, LPS_WAITING_FOR_GL_KICK_VERIFY};

class KickPlayer : public LoginPlayer {
public:
    KickPlayer() : LoginPlayer(new Socket()) {
        setID(expectedAccount);
        setWorldID(kInitial.world);
        setGroupID(kInitial.group);
        setLastSlot(kInitial.slot);
        setWorldGroupID(kInitial.ready);
        setServerGroupID(55);
        setPlayerStatus(kInitial.status);
    }
    ~KickPlayer() override {
        setPlayerStatus(LPS_END_SESSION);
    }
    void sendPacket(Packet* packet) override {
        const auto* error = dynamic_cast<LCLoginError*>(packet);
        if (!error)
            std::abort();
        errorID = error->getErrorID();
        observed = cache(*this);
        sentWithAccount = m_ID == expectedAccount;
        ++attempts;
        if (failure)
            std::rethrow_exception(failure);
        if (serialize)
            LoginPlayer::sendPacket(packet);
        ++sent;
    }
    std::string bufferedBytes() const {
        return {m_pOutputStream->getBuffer(), m_pOutputStream->length()};
    }

    const std::string expectedAccount = std::string(64, 'a');
    KickCache observed{};
    bool sentWithAccount = false;
    unsigned attempts = 0;
    unsigned sent = 0;
    BYTE errorID = 0;
    bool serialize = false;
    std::exception_ptr failure;
};

class KickAccounts : public FakeLoginAccountRepository {
public:
    explicit KickAccounts(KickPlayer& player) : player(player) {}
    bool loadLastLocation(const std::string& account, int& world, int& group, int& slot) override {
        observed = cache(player);
        correctAccount = account == player.expectedAccount;
        ++reads;
        if (failure)
            std::rethrow_exception(failure);
        world = savedWorld;
        group = savedGroup;
        slot = savedSlot;
        return found;
    }
    void markLoggedOff(const std::string&) override {
        std::abort();
    }
    void setCurrentLocation(int, int, int, const std::string&) override {
        std::abort();
    }

    KickPlayer& player;
    int savedWorld = 7;
    int savedGroup = 9;
    int savedSlot = 3;
    bool found = true;
    unsigned reads = 0;
    bool correctAccount = false;
    KickCache observed{};
    std::exception_ptr failure;
};

class KickCharacters : public FakeLoginCharacterRepository {
public:
    explicit KickCharacters(KickPlayer& player) : player(player) {}
    bool loadSlayerNameInSlot(WorldID_t world, const std::string& account, int slot, std::string& result) override {
        observed = cache(player);
        queryWorld = world;
        querySlot = slot;
        correctAccount = account == player.expectedAccount;
        ++reads;
        if (failure)
            std::rethrow_exception(failure);
        result = name;
        return found;
    }

    KickPlayer& player;
    std::string name = std::string(96, 'c');
    WorldID_t queryWorld = 0;
    int querySlot = -1;
    unsigned reads = 0;
    bool found = true;
    bool correctAccount = false;
    KickCache observed{};
    std::exception_ptr failure;
};

TEST(LoginKickPreparation, UncachedReadsSeeTheOriginalSessionUntilACompleteTargetIsPublished) {
    KickPlayer player;
    KickAccounts accounts(player);
    KickCharacters characters(player);

    const auto target = de::prepareLoginKick(player, accounts, characters);

    ASSERT_TRUE(target);
    EXPECT_EQ(accounts.observed, kInitial);
    EXPECT_EQ(characters.observed, kInitial);
    EXPECT_TRUE(accounts.correctAccount);
    EXPECT_TRUE(characters.correctAccount);
    EXPECT_EQ(characters.queryWorld, 7);
    EXPECT_EQ(characters.querySlot, 3);
    EXPECT_EQ(target->worldID, 7);
    EXPECT_EQ(target->groupID, 9);
    EXPECT_EQ(target->lastSlot, 3u);
    EXPECT_EQ(target->characterName, characters.name);
    EXPECT_EQ(cache(player), kPrepared);
    EXPECT_EQ(player.getLastCharacterName(), characters.name);
    EXPECT_EQ(player.getID(), player.expectedAccount);
    EXPECT_EQ(player.getServerGroupID(), 55);
    EXPECT_EQ(player.attempts, 0u);
}

TEST(LoginKickPreparation, CachedLocationUsesItsStoredSlotAndSkipsTheAccountQuery) {
    KickPlayer player;
    player.setWorldGroupID(true);
    const auto before = cache(player);
    KickAccounts accounts(player);
    KickCharacters characters(player);

    const auto target = de::prepareLoginKick(player, accounts, characters);

    ASSERT_TRUE(target);
    EXPECT_EQ(accounts.reads, 0u);
    EXPECT_EQ(characters.queryWorld, 3);
    EXPECT_EQ(characters.querySlot, 2);
    EXPECT_EQ(characters.observed, before);
    EXPECT_EQ(target->worldID, 3);
    EXPECT_EQ(target->groupID, 4);
    EXPECT_EQ(target->lastSlot, 2u);
    EXPECT_EQ(cache(player), before);
    EXPECT_EQ(player.getLastCharacterName(), characters.name);
}

TEST(LoginKickPreparation, MissingLocationRefusesBeforeAnyCharacterQueryOrCachePublication) {
    KickPlayer player;
    KickAccounts accounts(player);
    KickCharacters characters(player);
    accounts.found = false;
    accounts.savedWorld = accounts.savedGroup = accounts.savedSlot = -1;

    const auto target = de::prepareLoginKick(player, accounts, characters);

    EXPECT_FALSE(target);
    EXPECT_EQ(characters.reads, 0u);
    EXPECT_EQ(player.observed, kInitial);
    EXPECT_TRUE(player.sentWithAccount);
    EXPECT_EQ(player.errorID, ALREADY_CONNECTED);
    EXPECT_EQ(player.sent, 1u);
    EXPECT_EQ(player.getID(), "NONE");
    EXPECT_EQ(cache(player), (KickCache{3, 4, 2, false, LPS_BEGIN_SESSION}));
    EXPECT_TRUE(player.getLastCharacterName().empty());
}

TEST(LoginKickPreparation, MissingCharacterRefusesWithoutPublishingTheLoadedLocation) {
    KickPlayer player;
    KickAccounts accounts(player);
    KickCharacters characters(player);
    characters.found = false;

    const auto target = de::prepareLoginKick(player, accounts, characters);

    EXPECT_FALSE(target);
    EXPECT_EQ(characters.reads, 1u);
    EXPECT_EQ(player.observed, kInitial);
    EXPECT_TRUE(player.sentWithAccount);
    EXPECT_EQ(player.errorID, ALREADY_CONNECTED);
    EXPECT_EQ(player.sent, 1u);
    EXPECT_EQ(player.getID(), "NONE");
    EXPECT_EQ(cache(player), (KickCache{3, 4, 2, false, LPS_BEGIN_SESSION}));
    EXPECT_TRUE(player.getLastCharacterName().empty());
}

TEST(LoginKickPreparation, AnEmptySuccessfulCharacterLookupIsStillMissingACharacter) {
    KickPlayer player;
    KickAccounts accounts(player);
    KickCharacters characters(player);
    characters.name.clear();

    const auto target = de::prepareLoginKick(player, accounts, characters);

    EXPECT_FALSE(target);
    EXPECT_EQ(player.sent, 1u);
    EXPECT_EQ(player.errorID, ALREADY_CONNECTED);
    EXPECT_EQ(cache(player), (KickCache{3, 4, 2, false, LPS_BEGIN_SESSION}));
    EXPECT_EQ(player.getID(), "NONE");
}

TEST(LoginKickPreparation, InvalidSavedFieldsAreRefusedBeforeNarrowingOrPublishing) {
    for (int field = 0; field < 3; ++field) {
        for (const int value : {-1, 256, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
            SCOPED_TRACE(::testing::Message() << field << "/" << value);
            KickPlayer player;
            KickAccounts accounts(player);
            KickCharacters characters(player);
            (field == 0 ? accounts.savedWorld : field == 1 ? accounts.savedGroup : accounts.savedSlot) = value;

            EXPECT_THROW((void)de::prepareLoginKick(player, accounts, characters), Error);

            EXPECT_EQ(cache(player), kInitial);
            EXPECT_EQ(player.getID(), player.expectedAccount);
            EXPECT_TRUE(player.getLastCharacterName().empty());
            EXPECT_EQ(characters.reads, 0u);
            EXPECT_EQ(player.attempts, 0u);
        }
    }
}

TEST(LoginKickPreparation, CachedLocationAndNameSkipBothRepositoryQueries) {
    KickPlayer player;
    player.setWorldGroupID(true);
    player.setLastCharacterName("cached character");
    const auto before = cache(player);
    KickAccounts accounts(player);
    KickCharacters characters(player);

    const auto target = de::prepareLoginKick(player, accounts, characters);

    ASSERT_TRUE(target);
    EXPECT_EQ(target->worldID, 3);
    EXPECT_EQ(target->groupID, 4);
    EXPECT_EQ(target->characterName, "cached character");
    EXPECT_EQ(accounts.reads, 0u);
    EXPECT_EQ(characters.reads, 0u);
    EXPECT_EQ(cache(player), before);
    EXPECT_EQ(player.attempts, 0u);
}

TEST(LoginKickPreparation, KnownNamesStillLoadAnUncachedLocationWithoutACharacterQuery) {
    KickPlayer player;
    player.setLastCharacterName("cached character");
    KickAccounts accounts(player);
    KickCharacters characters(player);

    const auto target = de::prepareLoginKick(player, accounts, characters);

    ASSERT_TRUE(target);
    EXPECT_EQ(target->worldID, 7);
    EXPECT_EQ(target->groupID, 9);
    EXPECT_EQ(target->lastSlot, 3u);
    EXPECT_EQ(target->characterName, "cached character");
    EXPECT_EQ(accounts.reads, 1u);
    EXPECT_EQ(characters.reads, 0u);
    EXPECT_EQ(cache(player), kPrepared);
}

TEST(LoginKickPreparation, RepositoryExceptionsPreserveTheirIdentityAndTheUnpublishedCache) {
    const auto failure = std::make_exception_ptr(std::runtime_error("query failed"));
    for (const bool atAccount : {true, false}) {
        KickPlayer player;
        KickAccounts accounts(player);
        KickCharacters characters(player);
        (atAccount ? accounts.failure : characters.failure) = failure;
        std::exception_ptr caught;
        try {
            (void)de::prepareLoginKick(player, accounts, characters);
        } catch (...) {
            caught = std::current_exception();
        }
        EXPECT_EQ(caught, failure);
        EXPECT_EQ(cache(player), kInitial);
        EXPECT_TRUE(player.getLastCharacterName().empty());
        EXPECT_EQ(player.getID(), player.expectedAccount);
        EXPECT_EQ(player.attempts, 0u);
    }
}

TEST(LoginKickPreparation, StoredAndCachedBoundaryLocationsKeepEveryOneBasedSlot) {
    for (const bool cached : {false, true}) {
        for (const WorldID_t world : {0, 255}) {
            for (const ServerGroupID_t group : {0, 255}) {
                for (uint slot = 1; slot <= SLOT_MAX; ++slot) {
                    KickPlayer player;
                    KickAccounts accounts(player);
                    KickCharacters characters(player);
                    accounts.savedWorld = world;
                    accounts.savedGroup = group;
                    accounts.savedSlot = slot;
                    if (cached) {
                        player.setWorldID(world);
                        player.setGroupID(group);
                        player.setLastSlot(slot);
                        player.setWorldGroupID(true);
                    }
                    const auto before = cache(player);

                    const auto target = de::prepareLoginKick(player, accounts, characters);

                    ASSERT_TRUE(target);
                    EXPECT_EQ(target->worldID, world);
                    EXPECT_EQ(target->groupID, group);
                    EXPECT_EQ(target->lastSlot, slot);
                    EXPECT_EQ(characters.queryWorld, world);
                    EXPECT_EQ(characters.querySlot, static_cast<int>(slot));
                    EXPECT_EQ(characters.observed, before);
                    EXPECT_EQ(accounts.reads, cached ? 0u : 1u);
                    EXPECT_EQ(cache(player), (KickCache{world, group, slot, true, kInitial.status}));
                }
            }
        }
    }
}

TEST(LoginKickPreparation, TheDefaultZeroSlotRequiresAnAlreadyKnownCharacterName) {
    for (const bool cached : {false, true}) {
        for (const bool named : {false, true}) {
            KickPlayer player;
            player.setLastSlot(0);
            player.setWorldGroupID(cached);
            if (named)
                player.setLastCharacterName("known character");
            KickAccounts accounts(player);
            accounts.savedSlot = 0;
            KickCharacters characters(player);

            const auto target = de::prepareLoginKick(player, accounts, characters);

            EXPECT_EQ(target.has_value(), named);
            EXPECT_EQ(characters.reads, 0u);
            EXPECT_EQ(player.sent, named ? 0u : 1u);
            if (named) {
                ASSERT_TRUE(target);
                EXPECT_EQ(target->lastSlot, 0u);
                EXPECT_EQ(target->characterName, "known character");
                EXPECT_EQ(player.getPlayerStatus(), kInitial.status);
            } else {
                EXPECT_EQ(player.errorID, ALREADY_CONNECTED);
                EXPECT_EQ(player.getPlayerStatus(), LPS_BEGIN_SESSION);
                EXPECT_EQ(player.getID(), "NONE");
            }
        }
    }
}

TEST(LoginKickPreparation, UnsupportedSlotsAreConfigurationErrorsEvenWithAKnownName) {
    for (const bool named : {false, true}) {
        for (const bool cached : {false, true}) {
            for (const uint slot : {4u, 255u, std::numeric_limits<uint>::max()}) {
                KickPlayer player;
                player.setWorldGroupID(cached);
                if (cached)
                    player.setLastSlot(slot);
                if (named)
                    player.setLastCharacterName("known character");
                KickAccounts accounts(player);
                accounts.savedSlot = slot == std::numeric_limits<uint>::max() ? -1 : static_cast<int>(slot);
                KickCharacters characters(player);
                const auto before = cache(player);
                const auto previousName = player.getLastCharacterName();

                try {
                    (void)de::prepareLoginKick(player, accounts, characters);
                    FAIL() << "invalid slot was accepted";
                } catch (const Error& error) {
                    EXPECT_EQ(error.getMessage(), "invalid last character slot");
                }
                EXPECT_EQ(cache(player), before);
                EXPECT_EQ(player.getLastCharacterName(), previousName);
                EXPECT_EQ(characters.reads, 0u);
                EXPECT_EQ(player.attempts, 0u);
            }
        }
    }
}

TEST(LoginKickPreparation, AKnownCharacterStillNeedsAResolvedLocation) {
    KickPlayer player;
    player.setLastCharacterName("known character");
    KickAccounts accounts(player);
    accounts.found = false;
    KickCharacters characters(player);

    EXPECT_FALSE(de::prepareLoginKick(player, accounts, characters));

    EXPECT_EQ(accounts.reads, 1u);
    EXPECT_EQ(characters.reads, 0u);
    EXPECT_EQ(player.errorID, ALREADY_CONNECTED);
    EXPECT_EQ(player.sent, 1u);
    EXPECT_EQ(player.getLastCharacterName(), "known character");
    EXPECT_EQ(cache(player), (KickCache{3, 4, 2, false, LPS_BEGIN_SESSION}));
}

int checkQueryFailure(int stage, const std::exception_ptr& failure) {
    auto player = std::make_unique<KickPlayer>();
    KickAccounts accounts(*player);
    KickCharacters characters(*player);
    if (stage == 2)
        player->setWorldGroupID(true);
    (stage == 0 ? accounts.failure : characters.failure) = failure;
    const auto before = cache(*player);
    AllocationProbe probe;
    bool caught = false;
    try {
        (void)de::prepareLoginKick(*player, accounts, characters);
    } catch (...) {
        caught = std::current_exception() == failure;
    }
    if (!caught || cache(*player) != before || !player->getLastCharacterName().empty() || player->attempts != 0 ||
        probe.outstanding() != 0)
        return 1;
    accounts.failure = characters.failure = nullptr;
    auto retry = de::prepareLoginKick(*player, accounts, characters);
    if (!retry || retry->characterName != characters.name || player->getLastCharacterName() != characters.name)
        return 2;
    retry.reset();
    player.reset();
    return probe.outstanding() == 0 ? 0 : 3;
}

TEST(LoginKickPreparation, EveryQueryFailurePreservesIdentityReleasesPreparationAndPermitsRetry) {
    const std::array failures{std::make_exception_ptr(std::runtime_error("query failed")),
                              std::make_exception_ptr(DatabaseError("query failed")),
                              std::make_exception_ptr(Error("query failed"))};
    for (int stage = 0; stage < 3; ++stage) {
        for (const auto& failure : failures) {
            SCOPED_TRACE(stage);
            ASSERT_EXIT(std::_Exit(checkQueryFailure(stage, failure)), ::testing::ExitedWithCode(0), "");
        }
    }
}

enum class Missing { Location, Character, EmptyName, Slot };

void makeMissing(Missing missing, KickAccounts& accounts, KickCharacters& characters) {
    switch (missing) {
    case Missing::Location:
        accounts.found = false;
        break;
    case Missing::Character:
        characters.found = false;
        break;
    case Missing::EmptyName:
        characters.name.clear();
        break;
    case Missing::Slot:
        accounts.savedSlot = 0;
        break;
    }
}

int checkRefusalFailure(Missing missing, const std::exception_ptr& failure) {
    auto player = std::make_unique<KickPlayer>();
    KickAccounts accounts(*player);
    KickCharacters characters(*player);
    makeMissing(missing, accounts, characters);
    player->failure = failure;
    AllocationProbe probe;
    bool caught = false;
    try {
        (void)de::prepareLoginKick(*player, accounts, characters);
    } catch (...) {
        caught = std::current_exception() == failure;
    }
    if (!caught || cache(*player) != kInitial || player->observed != kInitial ||
        !player->getLastCharacterName().empty() || !player->sentWithAccount || player->sent != 0 ||
        player->attempts != 1 || probe.outstanding() != 0)
        return 1;
    player->failure = nullptr;
    const auto refused = de::prepareLoginKick(*player, accounts, characters);
    if (refused || player->sent != 1 || player->getID() != "NONE" || player->getPlayerStatus() != LPS_BEGIN_SESSION)
        return 2;
    player.reset();
    return probe.outstanding() == 0 ? 0 : 3;
}

TEST(LoginKickPreparation, FailedRefusalSendsPreserveSessionIdentityAndCanRetryTheReply) {
    const std::array failures{std::make_exception_ptr(std::runtime_error("send failed")),
                              std::make_exception_ptr(DatabaseError("send failed")),
                              std::make_exception_ptr(Error("send failed")), std::make_exception_ptr(std::bad_alloc{})};
    for (const auto missing : {Missing::Location, Missing::Character, Missing::EmptyName, Missing::Slot}) {
        for (const auto& failure : failures) {
            SCOPED_TRACE(static_cast<int>(missing));
            ASSERT_EXIT(std::_Exit(checkRefusalFailure(missing, failure)), ::testing::ExitedWithCode(0), "");
        }
    }
}

int checkAllocationFailure(std::size_t failAt, bool cached, bool named) {
    auto player = std::make_unique<KickPlayer>();
    player->setWorldGroupID(cached);
    if (named)
        player->setLastCharacterName(std::string(96, 'n'));
    KickAccounts accounts(*player);
    KickCharacters characters(*player);
    const auto before = cache(*player);
    const auto previousName = player->getLastCharacterName();
    const auto expectedName = named ? previousName : characters.name;
    const KickCache prepared{cached ? before.world : WorldID_t(7), cached ? before.group : ServerGroupID_t(9),
                             cached ? before.slot : 3u, true, before.status};
    AllocationProbe probe(failAt);
    bool failed = false;
    std::optional<de::LoginKickTarget> target;
    try {
        target = de::prepareLoginKick(*player, accounts, characters);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 64 && failed))
        return 1;
    if (failed) {
        if (target || cache(*player) != before || player->getLastCharacterName() != previousName ||
            probe.outstanding() != 0)
            return 2;
    } else if (!target || cache(*player) != prepared || target->characterName != expectedName) {
        return 3;
    }
    target.reset();
    auto retry = de::prepareLoginKick(*player, accounts, characters);
    if (!retry || retry->worldID != prepared.world || retry->groupID != prepared.group ||
        retry->lastSlot != prepared.slot || retry->characterName != expectedName || cache(*player) != prepared ||
        player->getLastCharacterName() != expectedName || player->attempts != 0)
        return 4;
    retry.reset();
    player.reset();
    return probe.outstanding() == 0 ? 0 : 5;
}

TEST(LoginKickPreparation, AllocationFailuresPreserveTheCacheAndCleanUpBeforeEveryRetry) {
    for (const bool cached : {false, true}) {
        for (const bool named : {false, true}) {
            for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
                SCOPED_TRACE(::testing::Message() << cached << "/" << named << "/" << failAt);
                ASSERT_EXIT(std::_Exit(checkAllocationFailure(failAt, cached, named)), ::testing::ExitedWithCode(0),
                            "");
            }
        }
    }
}

TEST(LoginKickPreparation, ProductionSendingBuffersTheExistingAlreadyConnectedReplyBeforeReset) {
    KickPlayer player;
    player.serialize = true;
    KickAccounts accounts(player);
    KickCharacters characters(player);
    accounts.found = false;

    EXPECT_FALSE(de::prepareLoginKick(player, accounts, characters));

    const auto bytes = player.bufferedBytes();
    ASSERT_EQ(bytes.size(), szPacketHeader + szBYTE);
    PacketID_t packetID;
    std::memcpy(&packetID, bytes.data(), szPacketID);
    EXPECT_EQ(packetID, Packet::PACKET_LC_LOGIN_ERROR);
    EXPECT_EQ(static_cast<BYTE>(bytes[szPacketHeader]), ALREADY_CONNECTED);
    EXPECT_EQ(player.observed, kInitial);
    EXPECT_TRUE(player.sentWithAccount);
    EXPECT_EQ(player.getID(), "NONE");
    EXPECT_EQ(player.getPlayerStatus(), LPS_BEGIN_SESSION);
}

TEST(LoginKickPreparation, ACompleteCacheIsReusedWithoutRepeatingEitherQuery) {
    KickPlayer player;
    KickAccounts accounts(player);
    KickCharacters characters(player);
    const auto first = de::prepareLoginKick(player, accounts, characters);
    ASSERT_TRUE(first);
    accounts.failure = characters.failure = std::make_exception_ptr(std::runtime_error("unexpected query"));

    const auto second = de::prepareLoginKick(player, accounts, characters);

    ASSERT_TRUE(second);
    EXPECT_EQ(accounts.reads, 1u);
    EXPECT_EQ(characters.reads, 1u);
    EXPECT_EQ(second->worldID, first->worldID);
    EXPECT_EQ(second->groupID, first->groupID);
    EXPECT_EQ(second->lastSlot, first->lastSlot);
    EXPECT_EQ(second->characterName, first->characterName);
}

TEST(LoginKickPreparation, ReturnedTargetsOwnTheirValuesAcrossLaterCacheChanges) {
    KickPlayer player;
    KickAccounts accounts(player);
    KickCharacters characters(player);
    const auto target = de::prepareLoginKick(player, accounts, characters);
    ASSERT_TRUE(target);

    player.setWorldID(255);
    player.setGroupID(255);
    player.setLastSlot(1);
    player.setLastCharacterName("different character");
    characters.name = "different row";

    EXPECT_EQ(target->worldID, 7);
    EXPECT_EQ(target->groupID, 9);
    EXPECT_EQ(target->lastSlot, 3u);
    EXPECT_EQ(target->characterName, std::string(96, 'c'));
}

TEST(LoginKickPreparation, ANewAuthenticatedAttemptAfterRefusalCanResolveFreshLocationData) {
    for (const auto missing : {Missing::Location, Missing::Character, Missing::EmptyName, Missing::Slot}) {
        KickPlayer player;
        KickAccounts accounts(player);
        KickCharacters characters(player);
        makeMissing(missing, accounts, characters);
        EXPECT_FALSE(de::prepareLoginKick(player, accounts, characters));
        EXPECT_FALSE(player.isSetWorldGroupID());
        player.setID(player.expectedAccount);
        accounts.found = characters.found = true;
        accounts.savedWorld = accounts.savedGroup = 255;
        accounts.savedSlot = 1;
        characters.name = "retry character";

        const auto target = de::prepareLoginKick(player, accounts, characters);

        ASSERT_TRUE(target);
        EXPECT_EQ(target->worldID, 255);
        EXPECT_EQ(target->groupID, 255);
        EXPECT_EQ(target->lastSlot, 1u);
        EXPECT_EQ(target->characterName, "retry character");
        EXPECT_EQ(cache(player), (KickCache{255, 255, 1, true, LPS_BEGIN_SESSION}));
        EXPECT_EQ(player.sent, 1u);
    }
}

} // namespace
