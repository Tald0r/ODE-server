#include <array>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "LCPCList.h"
#include "LoginPlayer.h"
#include "LoginServerSelection.h"
#include "LoginWorldTopology.h"
#include "Socket.h"
#include "SocketOutputStream.h"
#include "WorldSelection.h"
#include "repository/LoginConfigRepository.h"
#include "repository/ServerInfoRepository.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginCharacterRepository.h"

namespace {

struct SessionState {
    WorldID_t world;
    ServerGroupID_t group;
    PlayerStatus status;
    bool operator==(const SessionState&) const = default;
};

SessionState state(const LoginPlayer& player) {
    return {player.getWorldID(), player.getServerGroupID(), player.getPlayerStatus()};
}

constexpr SessionState kInitial{3, 2, LPS_WAITING_FOR_CL_GET_PC_LIST};
constexpr SessionState kSelected{7, 9, LPS_WAITING_FOR_CL_GET_PC_LIST};

class SelectionPlayer : public LoginPlayer {
public:
    SelectionPlayer() : LoginPlayer(new Socket()) {
        setID(std::string(64, 'a'));
        setWorldID(kInitial.world);
        setServerGroupID(kInitial.group);
        setPlayerStatus(kInitial.status);
    }
    ~SelectionPlayer() override {
        setPlayerStatus(LPS_END_SESSION);
    }

    void sendPacket(Packet* packet) override {
        observed = state(*this);
        auto* reply = dynamic_cast<LCPCList*>(packet);
        if (!reply)
            std::abort();
        replySize = reply->getPacketSize();
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

    SessionState observed{};
    unsigned attempts = 0;
    unsigned sent = 0;
    PacketSize_t replySize = 0;
    bool serialize = false;
    std::exception_ptr failure;
};

enum class TopologyStage { Worlds, Groups, Metadata };

class SelectionTopology : public WorldSelectionTopology {
public:
    explicit SelectionTopology(SelectionPlayer& player) : player(player) {}

    std::vector<WorldID_t> worldIDs() override {
        record(TopologyStage::Worlds);
        return worlds;
    }
    std::vector<ServerGroupID_t> serverGroupIDs(WorldID_t world) override {
        record(TopologyStage::Groups);
        groupWorld = world;
        return groups;
    }
    ServerGroupRow serverGroup(ServerGroupID_t group, WorldID_t world) override {
        record(TopologyStage::Metadata);
        rowWorld = world;
        rowGroup = group;
        return {group, "Group", groupStatus};
    }
    WorldStatus worldStatus(WorldID_t) override {
        std::abort();
    }
    UserNum_t serverGroupUserNum(ServerGroupID_t, WorldID_t) override {
        std::abort();
    }
    void record(TopologyStage stage) {
        observed[static_cast<unsigned>(stage)] = state(player);
        ++reads;
        if (stage == failureStage && failure)
            std::rethrow_exception(failure);
    }

    SelectionPlayer& player;
    std::vector<WorldID_t> worlds = {7, 1};
    std::vector<ServerGroupID_t> groups = {9, 1};
    BYTE groupStatus = SERVER_FREE;
    WorldID_t groupWorld = 0;
    WorldID_t rowWorld = 0;
    ServerGroupID_t rowGroup = 0;
    std::array<SessionState, 3> observed{};
    unsigned reads = 0;
    TopologyStage failureStage = TopologyStage::Worlds;
    std::exception_ptr failure;
};

class SelectionCharacters : public FakeLoginCharacterRepository {
public:
    explicit SelectionCharacters(SelectionPlayer& player) : player(player) {
        LoginSlayerListRow row{};
        row.race = "SLAYER";
        row.name = std::string(40, 's');
        row.slot = "SLOT1";
        row.sex = "MALE";
        rows.push_back(row);
    }

    std::vector<LoginSlayerListRow> loadSlayerList(WorldID_t world, const std::string& account) override {
        observed = state(player);
        queryWorld = world;
        correctAccount = account == expectedAccount;
        ++reads;
        if (failure)
            std::rethrow_exception(failure);
        return rows;
    }
    void setCharacterServerGroup(WorldID_t, int, const std::string&) override {
        std::abort();
    }

    SelectionPlayer& player;
    std::string expectedAccount = std::string(64, 'a');
    bool correctAccount = false;
    std::vector<LoginSlayerListRow> rows;
    WorldID_t queryWorld = 0;
    SessionState observed{};
    unsigned reads = 0;
    std::exception_ptr failure;
};

TEST(LoginServerSelection, AcceptedSelectionPublishesLocationBeforeQueryAndStatusAfterSend) {
    SelectionPlayer player;
    player.setWorldID(7);
    SelectionTopology topology(player);
    SelectionCharacters characters(player);
    const auto before = state(player);

    de::selectLoginServer(player, 9, topology, characters);

    for (const auto& observed : topology.observed)
        EXPECT_EQ(observed, before);
    EXPECT_EQ(characters.queryWorld, 7);
    EXPECT_TRUE(characters.correctAccount);
    EXPECT_EQ(characters.observed, kSelected);
    EXPECT_EQ(player.observed, kSelected);
    EXPECT_EQ(player.sent, 1u);
    EXPECT_GT(player.replySize, SLOT_MAX);
    EXPECT_EQ(state(player), (SessionState{7, 9, LPS_PC_MANAGEMENT}));
}

TEST(LoginServerSelection, MissingWorldAndGroupUseTheNormalizedLocationForTheQueryAndSession) {
    SelectionPlayer player;
    SelectionTopology topology(player);
    SelectionCharacters characters(player);

    de::selectLoginServer(player, 4, topology, characters);

    EXPECT_EQ(topology.groupWorld, 7);
    EXPECT_EQ(topology.rowWorld, 7);
    EXPECT_EQ(topology.rowGroup, 9);
    for (const auto& observed : topology.observed)
        EXPECT_EQ(observed, kInitial);
    EXPECT_EQ(characters.queryWorld, 7);
    EXPECT_EQ(characters.observed, kSelected);
    EXPECT_EQ(player.observed, kSelected);
    EXPECT_EQ(state(player), (SessionState{7, 9, LPS_PC_MANAGEMENT}));
}

TEST(LoginServerSelection, RefusedSelectionsKeepSessionAndSkipCharacterQueriesAndSending) {
    for (const auto reason :
         {SelectServerReason::NoWorlds, SelectServerReason::NoServerGroups, SelectServerReason::ServerClosed}) {
        SelectionPlayer player;
        SelectionTopology topology(player);
        SelectionCharacters characters(player);
        const char* message;
        if (reason == SelectServerReason::NoWorlds) {
            topology.worlds.clear();
            message = "NoWorlds";
        } else if (reason == SelectServerReason::NoServerGroups) {
            topology.groups.clear();
            message = "NoServerGroups";
        } else {
            topology.groupStatus = SERVER_DOWN;
            message = "ServerClosed";
        }

        try {
            de::selectLoginServer(player, 4, topology, characters);
            FAIL() << "refused selection returned";
        } catch (const DisconnectException& error) {
            EXPECT_EQ(error.getMessage(), message);
        }

        EXPECT_EQ(state(player), kInitial);
        EXPECT_EQ(characters.reads, 0u);
        EXPECT_EQ(player.attempts, 0u);
    }
}

TEST(LoginServerSelection, QueryFailureKeepsSelectedLocationWithoutAdvancingStatusOrSending) {
    SelectionPlayer player;
    SelectionTopology topology(player);
    SelectionCharacters characters(player);
    characters.failure = std::make_exception_ptr(std::runtime_error("query failed"));
    std::exception_ptr caught;

    try {
        de::selectLoginServer(player, 4, topology, characters);
    } catch (...) {
        caught = std::current_exception();
    }

    EXPECT_EQ(caught, characters.failure);
    EXPECT_EQ(characters.queryWorld, 7);
    EXPECT_EQ(characters.observed, kSelected);
    EXPECT_EQ(state(player), kSelected);
    EXPECT_EQ(player.attempts, 0u);
}

TEST(LoginServerSelection, SendFailureKeepsSelectedLocationAndDoesNotAdvanceStatus) {
    SelectionPlayer player;
    SelectionTopology topology(player);
    SelectionCharacters characters(player);
    player.failure = std::make_exception_ptr(std::runtime_error("send failed"));
    std::exception_ptr caught;

    try {
        de::selectLoginServer(player, 4, topology, characters);
    } catch (...) {
        caught = std::current_exception();
    }

    EXPECT_EQ(caught, player.failure);
    EXPECT_EQ(player.observed, kSelected);
    EXPECT_EQ(state(player), kSelected);
    EXPECT_EQ(player.attempts, 1u);
    EXPECT_EQ(player.sent, 0u);
}

TEST(LoginServerSelection, ExistingZeroAndMaximumLocationsReachEveryObserverWithoutNarrowing) {
    for (const WorldID_t world : {0, 255}) {
        for (const ServerGroupID_t group : {0, 255}) {
            SelectionPlayer player;
            player.setWorldID(world);
            SelectionTopology topology(player);
            topology.worlds = {255, 0};
            topology.groups = {255, 0};
            SelectionCharacters characters(player);

            de::selectLoginServer(player, group, topology, characters);

            const SessionState selected{world, group, kInitial.status};
            EXPECT_EQ(characters.queryWorld, world);
            EXPECT_EQ(characters.observed, selected);
            EXPECT_EQ(player.observed, selected);
            EXPECT_EQ(state(player), (SessionState{world, group, LPS_PC_MANAGEMENT}));
        }
    }
}

TEST(LoginServerSelection, EmptyCharacterListsStillCompleteSelectionAndSendAReply) {
    SelectionPlayer player;
    SelectionTopology topology(player);
    SelectionCharacters characters(player);
    characters.rows.clear();

    de::selectLoginServer(player, 4, topology, characters);

    EXPECT_EQ(characters.queryWorld, 7);
    EXPECT_EQ(player.replySize, SLOT_MAX);
    EXPECT_EQ(player.sent, 1u);
    EXPECT_EQ(state(player), (SessionState{7, 9, LPS_PC_MANAGEMENT}));
}

TEST(LoginServerSelection, SelectionLeavesOtherSessionDataAndCharacterPersistenceAlone) {
    SelectionPlayer player;
    player.setGroupID(44);
    player.setWorldGroupID(true);
    player.setLastSlot(3);
    player.setLastCharacterName("previous character");
    player.setFailureCount(2);
    player.setGameServerIP("192.0.2.10");
    const auto account = player.getID();
    SelectionTopology topology(player);
    SelectionCharacters characters(player);

    de::selectLoginServer(player, 4, topology, characters);

    EXPECT_EQ(player.getID(), account);
    EXPECT_EQ(player.getGroupID(), 44);
    EXPECT_TRUE(player.isSetWorldGroupID());
    EXPECT_EQ(player.getLastSlot(), 3u);
    EXPECT_EQ(player.getLastCharacterName(), "previous character");
    EXPECT_EQ(player.getFailureCount(), 2u);
    EXPECT_EQ(player.getGameServerIP(), "192.0.2.10");
    EXPECT_TRUE(characters.insertedSlayers.empty());
    EXPECT_TRUE(characters.insertedVampires.empty());
    EXPECT_TRUE(characters.insertedOusters.empty());
}

TEST(LoginServerSelection, TopologyFailuresRetainTheirIdentityAndLeaveTheSessionUntouched) {
    const std::array failures{std::make_exception_ptr(std::runtime_error("topology failed")),
                              std::make_exception_ptr(NoSuchElementException("topology failed")),
                              std::make_exception_ptr(DatabaseError("topology failed"))};
    for (const auto stage : {TopologyStage::Worlds, TopologyStage::Groups, TopologyStage::Metadata}) {
        for (const auto& failure : failures) {
            SelectionPlayer player;
            SelectionTopology topology(player);
            SelectionCharacters characters(player);
            topology.failureStage = stage;
            topology.failure = failure;
            std::exception_ptr caught;
            try {
                de::selectLoginServer(player, 4, topology, characters);
            } catch (...) {
                caught = std::current_exception();
            }
            EXPECT_EQ(caught, failure);
            EXPECT_EQ(state(player), kInitial);
            EXPECT_EQ(characters.reads, 0u);
            EXPECT_EQ(player.attempts, 0u);
        }
    }
}

TEST(LoginServerSelection, CharacterDatabaseFailureKeepsTheEstablishedDisconnectDiagnostic) {
    SelectionPlayer player;
    SelectionTopology topology(player);
    SelectionCharacters characters(player);
    characters.failure = std::make_exception_ptr(DatabaseError("query failed"));
    try {
        de::selectLoginServer(player, 4, topology, characters);
        FAIL() << "database failure returned";
    } catch (const DisconnectException& error) {
        EXPECT_EQ(error.getMessage(), "LoginPlayer::makePCList : query failed");
    }
    EXPECT_EQ(characters.observed, kSelected);
    EXPECT_EQ(state(player), kSelected);
    EXPECT_EQ(player.attempts, 0u);
}

TEST(LoginServerSelection, InvalidCharacterDataDoesNotSendOrAdvanceStatusAndCanRetry) {
    SelectionPlayer player;
    SelectionTopology topology(player);
    SelectionCharacters characters(player);
    characters.rows[0].slot = "SLOT0";
    EXPECT_THROW(de::selectLoginServer(player, 4, topology, characters), InvalidProtocolException);
    EXPECT_EQ(state(player), kSelected);
    EXPECT_EQ(player.attempts, 0u);
    characters.rows[0].slot = "SLOT1";
    de::selectLoginServer(player, 4, topology, characters);
    EXPECT_EQ(player.sent, 1u);
    EXPECT_EQ(state(player), (SessionState{7, 9, LPS_PC_MANAGEMENT}));
}

int checkSendFailure(const std::exception_ptr& failure) {
    SelectionPlayer player;
    SelectionTopology topology(player);
    SelectionCharacters characters(player);
    player.failure = failure;
    AllocationProbe probe;
    bool correct = false;
    try {
        de::selectLoginServer(player, 4, topology, characters);
    } catch (...) {
        correct = std::current_exception() == failure;
    }
    if (!correct || state(player) != kSelected || player.observed != kSelected || player.sent != 0 ||
        player.attempts != 1 || probe.outstanding() != 0)
        return 1;
    player.failure = nullptr;
    de::selectLoginServer(player, 4, topology, characters);
    return player.sent == 1 && player.attempts == 2 && player.getPlayerStatus() == LPS_PC_MANAGEMENT &&
                   probe.outstanding() == 0
               ? 0
               : 2;
}

TEST(LoginServerSelection, SendFailuresKeepTheirIdentityReleaseTheReplyAndPermitRetry) {
    const std::array failures{std::make_exception_ptr(std::runtime_error("send failed")),
                              std::make_exception_ptr(DatabaseError("send failed")),
                              std::make_exception_ptr(Error("send failed")), std::make_exception_ptr(std::bad_alloc{})};
    for (const auto& failure : failures)
        ASSERT_EXIT(std::_Exit(checkSendFailure(failure)), ::testing::ExitedWithCode(0), "");
}

int checkAllocationFailure(std::size_t failAt) {
    SelectionPlayer player;
    SelectionTopology topology(player);
    SelectionCharacters characters(player);
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        de::selectLoginServer(player, 4, topology, characters);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || probe.outstanding() != 0 || (failAt == 64 && failed))
        return 1;
    if (failed) {
        if ((state(player) != kInitial && state(player) != kSelected) || player.attempts != 0 ||
            (characters.reads != 0 && (characters.observed != kSelected || characters.queryWorld != 7)))
            return 2;
    } else if (state(player) != SessionState{7, 9, LPS_PC_MANAGEMENT} || player.sent != 1) {
        return 3;
    }
    const auto previousSent = player.sent;
    de::selectLoginServer(player, 4, topology, characters);
    return player.sent == previousSent + 1 && state(player) == SessionState{7, 9, LPS_PC_MANAGEMENT} &&
                   probe.outstanding() == 0
               ? 0
               : 4;
}

TEST(LoginServerSelection, AllocationFailuresKeepCoherentLocationAndReleaseTheReplyBeforeRetry) {
    for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(std::_Exit(checkAllocationFailure(failAt)), ::testing::ExitedWithCode(0), "");
    }
}

TEST(LoginServerSelection, ProductionPlayerSendingBuffersACompleteCharacterListFrame) {
    SelectionPlayer player;
    player.serialize = true;
    SelectionTopology topology(player);
    SelectionCharacters characters(player);

    de::selectLoginServer(player, 4, topology, characters);

    const auto bytes = player.bufferedBytes();
    ASSERT_EQ(bytes.size(), szPacketHeader + player.replySize);
    PacketID_t packetID;
    PacketSize_t bodySize;
    std::memcpy(&packetID, bytes.data(), szPacketID);
    std::memcpy(&bodySize, bytes.data() + szPacketID, szPacketSize);
    EXPECT_EQ(packetID, Packet::PACKET_LC_PC_LIST);
    EXPECT_EQ(bodySize, player.replySize);
    EXPECT_EQ(bytes.substr(szPacketHeader, SLOT_MAX), "S00");
    EXPECT_EQ(player.observed, kSelected);
    EXPECT_EQ(state(player), (SessionState{7, 9, LPS_PC_MANAGEMENT}));
}

TEST(LoginServerSelection, ProductionSerializationFailureKeepsStatusAndAllowsACompleteRetry) {
    SelectionPlayer player;
    player.serialize = true;
    SelectionTopology topology(player);
    SelectionCharacters characters(player);
    characters.rows[0].name.clear();

    EXPECT_THROW(de::selectLoginServer(player, 4, topology, characters), InvalidProtocolException);

    EXPECT_EQ(state(player), kSelected);
    EXPECT_EQ(player.sent, 0u);
    EXPECT_TRUE(player.bufferedBytes().empty());
    characters.rows[0].name = "Retry";
    de::selectLoginServer(player, 4, topology, characters);
    EXPECT_EQ(player.sent, 1u);
    EXPECT_EQ(state(player), (SessionState{7, 9, LPS_PC_MANAGEMENT}));
    EXPECT_EQ(player.bufferedBytes().size(), szPacketHeader + player.replySize);
}

class CatalogueRepository : public ServerInfoRepository, public LoginConfigRepository {
public:
    std::vector<ServerInfoWorldRow> loadWorlds() override {
        return worlds;
    }
    bool loadMaxGameServerGroupWorldID(int& maximum) override {
        maximum = 255;
        return true;
    }
    std::vector<LoginGameServerGroupRow> loadGameServerGroups() override {
        return groups;
    }
    std::vector<LoginGameServerGroupIDRow> loadGameServerGroupIDs() override {
        std::abort();
    }
    bool loadMaxServerGroupID(int&) override {
        std::abort();
    }
    bool loadMaxWorldID(int&) override {
        std::abort();
    }
    std::vector<ServerInfoRow> loadServers() override {
        std::abort();
    }
    std::vector<ServerInfoNonPKRow> loadNonPKServers() override {
        std::abort();
    }
    std::vector<ServerInfoCastleStatRow> loadCastleStats() override {
        std::abort();
    }
    std::vector<LoginZoneGroupRow> loadZoneGroups() override {
        std::abort();
    }
    std::vector<LoginZoneRow> loadZones() override {
        std::abort();
    }
    bool loadClientVersion(int&) override {
        std::abort();
    }

    std::vector<ServerInfoWorldRow> worlds = {{7, "Sparse", WORLD_OPEN}, {1, "First", WORLD_OPEN}};
    std::vector<LoginGameServerGroupRow> groups = {{7, 9, "Sparse group", SERVER_FREE},
                                                   {1, 1, "First group", SERVER_FREE}};
};

TEST(LoginServerSelection, RealCataloguesSupplyNormalizedWorldsAndObserveQuiescentReloads) {
    CatalogueRepository repository;
    GameWorldInfoManager worlds;
    GameServerGroupInfoManager groups;
    UserInfoManager unusedPopulations;
    worlds.load(repository);
    groups.load(repository);
    LoginWorldTopology topology(worlds, groups, unusedPopulations);
    SelectionPlayer player;
    SelectionCharacters characters(player);

    de::selectLoginServer(player, 4, topology, characters);
    EXPECT_EQ(characters.queryWorld, 7);
    EXPECT_EQ(characters.observed, kSelected);

    repository.worlds = {{255, "Last", WORLD_OPEN}, {0, "Zero", WORLD_OPEN}};
    repository.groups = {{255, 255, "Last group", SERVER_FREE}, {0, 0, "Zero group", SERVER_FREE}};
    worlds.load(repository);
    groups.load(repository);
    de::selectLoginServer(player, 4, topology, characters);
    EXPECT_EQ(characters.queryWorld, 255);
    EXPECT_EQ(characters.observed, (SessionState{255, 255, LPS_PC_MANAGEMENT}));
    EXPECT_EQ(player.observed, (SessionState{255, 255, LPS_PC_MANAGEMENT}));
    EXPECT_EQ(state(player), (SessionState{255, 255, LPS_PC_MANAGEMENT}));
}

TEST(LoginServerSelection, RealCatalogueGroupDownPolicyRemainsSeparateFromWorldClosureAndPopulation) {
    CatalogueRepository repository;
    repository.worlds[0].stat = WORLD_CLOSE;
    GameWorldInfoManager worlds;
    GameServerGroupInfoManager groups;
    UserInfoManager unusedPopulations;
    worlds.load(repository);
    groups.load(repository);
    LoginWorldTopology topology(worlds, groups, unusedPopulations);
    SelectionPlayer player;
    SelectionCharacters characters(player);

    de::selectLoginServer(player, 4, topology, characters);
    EXPECT_EQ(characters.queryWorld, 7);
    EXPECT_EQ(player.sent, 1u);
    const auto before = state(player);
    repository.groups[0].stat = SERVER_DOWN;
    groups.load(repository);
    EXPECT_THROW(de::selectLoginServer(player, 4, topology, characters), DisconnectException);
    EXPECT_EQ(state(player), before);
    EXPECT_EQ(characters.reads, 1u);
    EXPECT_EQ(player.attempts, 1u);
}

} // namespace
