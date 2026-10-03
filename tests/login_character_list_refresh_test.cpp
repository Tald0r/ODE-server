#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "LCPCList.h"
#include "LoginCharacterListRefresh.h"
#include "LoginPlayer.h"
#include "Socket.h"
#include "SocketOutputStream.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginCharacterRepository.h"

namespace {

enum class Stage { Slayer, Vampire, Ousters };
using Query = std::tuple<Stage, int, std::string, std::string>;

class Characters : public FakeLoginCharacterRepository {
public:
    Characters() {
        LoginSlayerListRow slayer{};
        slayer.race = "SLAYER";
        slayer.name = "Slayer";
        slayer.slot = "SLOT1";
        slayer.sex = "MALE";
        auto vampireIndex = slayer;
        vampireIndex.race = "VAMPIRE";
        vampireIndex.name = "vampire-index";
        auto oustersIndex = slayer;
        oustersIndex.race = "OUSTERS";
        oustersIndex.name = "ousters-index";
        rows = {slayer, vampireIndex, oustersIndex};
        vampire.name = "Vampire";
        vampire.slot = "SLOT2";
        vampire.sex = "FEMALE";
        ousters.name = "Ousters";
        ousters.slot = "SLOT3";
        ousters.sex = "MALE";
    }
    std::vector<LoginSlayerListRow> loadSlayerList(WorldID_t world, const std::string& account) override {
        visit(Stage::Slayer, world, account, "");
        return rows;
    }
    bool loadVampireListRow(WorldID_t world, const std::string& account, const std::string& name,
                            LoginVampireListRow& row) override {
        visit(Stage::Vampire, world, account, name);
        row = vampire;
        return vampireFound;
    }
    bool loadOustersListRow(WorldID_t world, const std::string& account, const std::string& name,
                            LoginOustersListRow& row) override {
        visit(Stage::Ousters, world, account, name);
        row = ousters;
        return oustersFound;
    }
    void visit(Stage stage, WorldID_t world, const std::string& account, const std::string& name) {
        queries.emplace_back(stage, world, account, name);
        if (before)
            before(stage);
        if (stage == failureStage && failure)
            std::rethrow_exception(failure);
    }
    std::vector<LoginSlayerListRow> rows;
    LoginVampireListRow vampire{};
    LoginOustersListRow ousters{};
    bool vampireFound = true;
    bool oustersFound = true;
    std::vector<Query> queries;
    std::function<void(Stage)> before;
    Stage failureStage = Stage::Slayer;
    std::exception_ptr failure;
};

class ListPlayer : public LoginPlayer {
public:
    ListPlayer() : LoginPlayer(new Socket("198.51.100.7", 1234)) {
        setID("account");
        setWorldID(7);
        setServerGroupID(9);
        setPlayerStatus(LPS_WAITING_FOR_CL_GET_PC_LIST);
        loginAccountOwnership().acquire("account", [] { return true; });
    }
    ~ListPlayer() override {
        setPlayerStatus(LPS_END_SESSION);
    }
    void sendPacket(Packet* packet) override {
        if (observe)
            observe(*packet);
        LoginPlayer::sendPacket(packet);
        ++sent;
    }
    std::string bufferedBytes() const {
        return {m_pOutputStream->getBuffer(), m_pOutputStream->length()};
    }
    unsigned sent = 0;
    std::function<void(Packet&)> observe;
};

struct Session {
    void refresh() {
        de::refreshLoginCharacterList(player, characters, actions);
    }
    Characters characters;
    ListPlayer player;
    unsigned sent = 0;
    de::LoginCharacterListRefreshActions actions{[&](LoginPlayer&, LCPCList&) { ++sent; }};
};

TEST(LoginCharacterListRefresh, TheOriginalPhaseRemainsVisibleThroughEveryLookupAndSending) {
    Session session;
    session.characters.before = [&](Stage) {
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
    };
    session.actions.send = [&](LoginPlayer& player, LCPCList&) {
        EXPECT_EQ(&player, &session.player);
        EXPECT_EQ(player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
        EXPECT_EQ(session.characters.queries.size(), 3u);
        ++session.sent;
    };
    session.refresh();
    EXPECT_EQ(session.sent, 1u);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_PC_MANAGEMENT);
}

TEST(LoginCharacterListRefresh, QueryFailuresDoNotPublishCharacterManagement) {
    for (const auto stage : {Stage::Slayer, Stage::Vampire, Stage::Ousters}) {
        Session session;
        session.characters.failureStage = stage;
        session.characters.failure = std::make_exception_ptr(std::runtime_error("lookup failed"));
        EXPECT_THROW(session.refresh(), std::runtime_error);
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
        EXPECT_EQ(session.sent, 0u);
    }
}

TEST(LoginCharacterListRefresh, SendFailuresDoNotPublishCharacterManagement) {
    Session session;
    session.actions.send = [](LoginPlayer&, LCPCList&) { throw DisconnectException("send failed"); };
    EXPECT_THROW(session.refresh(), DisconnectException);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
}

void expectIdentity(const Session& session) {
    EXPECT_EQ(session.player.getID(), "account");
    EXPECT_EQ(session.player.getWorldID(), 7);
    EXPECT_EQ(session.player.getServerGroupID(), 9);
    EXPECT_TRUE(session.player.loginAccountOwnership().owns("account"));
    EXPECT_TRUE(session.characters.insertedSlayers.empty());
    EXPECT_TRUE(session.characters.insertedVampires.empty());
    EXPECT_TRUE(session.characters.insertedOusters.empty());
    EXPECT_TRUE(session.characters.insertedFlagSets.empty());
}

void expectCompleteReply(const LCPCList& reply) {
    EXPECT_EQ(reply.getPacketID(), Packet::PACKET_LC_PC_LIST);
    EXPECT_EQ(reply.getPCInfo(SLOT1)->getPCType(), PC_SLAYER);
    EXPECT_EQ(dynamic_cast<const PCSlayerInfo&>(*reply.getPCInfo(SLOT1)).getName(), "Slayer");
    EXPECT_EQ(reply.getPCInfo(SLOT2)->getPCType(), PC_VAMPIRE);
    EXPECT_EQ(dynamic_cast<const PCVampireInfo&>(*reply.getPCInfo(SLOT2)).getName(), "Vampire");
    EXPECT_EQ(reply.getPCInfo(SLOT3)->getPCType(), PC_OUSTERS);
    EXPECT_EQ(dynamic_cast<const PCOustersInfo&>(*reply.getPCInfo(SLOT3)).getName(), "Ousters");
}

TEST(LoginCharacterListRefresh, CompleteRepliesUseSelectedInputsAndRetainIdentityAndOwnership) {
    Session session;
    const auto* owner = session.player.loginAccountOwnership().account();
    session.actions.send = [&](LoginPlayer& player, LCPCList& reply) {
        EXPECT_EQ(&player, &session.player);
        expectCompleteReply(reply);
        expectIdentity(session);
        EXPECT_EQ(player.loginAccountOwnership().account(), owner);
        ++session.sent;
    };
    session.refresh();
    EXPECT_EQ(session.characters.queries, (std::vector<Query>{{Stage::Slayer, 7, "account", ""},
                                                              {Stage::Vampire, 7, "account", "vampire-index"},
                                                              {Stage::Ousters, 7, "account", "ousters-index"}}));
    EXPECT_EQ(session.sent, 1u);
    expectIdentity(session);
}

TEST(LoginCharacterListRefresh, EmptyAndRepeatedRefreshesUseCurrentSelectionWithoutAcquiringAnAccount) {
    Session session;
    session.player.loginAccountOwnership().release([](const std::string&) {});
    session.characters.rows.clear();
    session.actions.send = [&](LoginPlayer& player, LCPCList& reply) {
        EXPECT_EQ(reply.getPacketSize(), SLOT_MAX);
        for (const auto slot : {SLOT1, SLOT2, SLOT3})
            EXPECT_THROW(reply.getPCInfo(slot), NoSuchElementException);
        EXPECT_EQ(player.loginAccountOwnership().account(), nullptr);
        ++session.sent;
    };
    session.refresh();
    for (const int world : {0, 255}) {
        session.player.setWorldID(world);
        session.player.setID("new-account");
        session.refresh();
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_PC_MANAGEMENT);
    }
    EXPECT_EQ(session.characters.queries, (std::vector<Query>{{Stage::Slayer, 7, "account", ""},
                                                              {Stage::Slayer, 0, "new-account", ""},
                                                              {Stage::Slayer, 255, "new-account", ""}}));
    EXPECT_EQ(session.sent, 3u);
}

TEST(LoginCharacterListRefresh, QueryInputsRemainOwnedWhenARepositoryCallbackChangesTheLiveSelection) {
    Session session;
    session.characters.before = [&](Stage stage) {
        if (stage == Stage::Slayer) {
            session.player.setID(std::string(64, 'x'));
            session.player.setWorldID(255);
        }
    };
    session.actions.send = [&](LoginPlayer&, LCPCList& reply) { expectCompleteReply(reply); };
    session.refresh();
    EXPECT_EQ(session.characters.queries, (std::vector<Query>{{Stage::Slayer, 7, "account", ""},
                                                              {Stage::Vampire, 7, "account", "vampire-index"},
                                                              {Stage::Ousters, 7, "account", "ousters-index"}}));
    EXPECT_EQ(session.player.getID(), std::string(64, 'x'));
    EXPECT_EQ(session.player.getWorldID(), 255);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_PC_MANAGEMENT);
    EXPECT_TRUE(session.player.loginAccountOwnership().owns("account"));
}

TEST(LoginCharacterListRefresh, DatabaseTranslationBelongsToAssemblyAndDoesNotCoverSending) {
    for (int stage = 0; stage < 4; ++stage) {
        Session session;
        const auto failure = std::make_exception_ptr(DatabaseError("query failed"));
        if (stage < 3) {
            session.characters.failureStage = static_cast<Stage>(stage);
            session.characters.failure = failure;
        } else {
            session.actions.send = [&](LoginPlayer&, LCPCList&) { std::rethrow_exception(failure); };
        }
        try {
            session.refresh();
            FAIL() << "database failure was swallowed";
        } catch (const DisconnectException& error) {
            EXPECT_LT(stage, 3);
            EXPECT_EQ(error.getMessage(), "LoginPlayer::makePCList : query failed");
        } catch (...) {
            EXPECT_EQ(stage, 3);
            EXPECT_EQ(std::current_exception(), failure);
        }
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
        EXPECT_EQ(session.sent, 0u);
        expectIdentity(session);
    }
}

TEST(LoginCharacterListRefresh, OtherLookupAndSenderFailuresKeepTheirIdentityAndTheOriginalPhase) {
    const std::exception_ptr failures[] = {
        std::make_exception_ptr(NoSuchElementException("missing result")),
        std::make_exception_ptr(Error("operation failed")), std::make_exception_ptr(DisconnectException("send failed")),
        std::make_exception_ptr(std::runtime_error("standard failure")), std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        for (int stage = 0; stage < 5; ++stage) {
            for (const auto phase : {LPS_WAITING_FOR_CL_GET_PC_LIST, LPS_PC_MANAGEMENT}) {
                Session session;
                session.player.setPlayerStatus(phase);
                if (stage < 3) {
                    session.characters.failureStage = static_cast<Stage>(stage);
                    session.characters.failure = failure;
                } else if (stage == 3) {
                    session.actions.send = [&](LoginPlayer&, LCPCList&) { std::rethrow_exception(failure); };
                } else {
                    session.actions = de::defaultLoginCharacterListRefreshActions();
                    session.player.observe = [&](Packet&) { std::rethrow_exception(failure); };
                }
                try {
                    session.refresh();
                    FAIL() << "failure was swallowed";
                } catch (...) {
                    EXPECT_EQ(std::current_exception(), failure);
                }
                EXPECT_EQ(session.player.getPlayerStatus(), phase);
                EXPECT_EQ(session.sent, 0u);
                EXPECT_EQ(session.player.sent, 0u);
                EXPECT_TRUE(session.player.bufferedBytes().empty());
                expectIdentity(session);
            }
        }
    }
}

TEST(LoginCharacterListRefresh, MissingRaceRowsRetainTheirDisconnectReasonsAndCanRetry) {
    for (bool vampire : {false, true}) {
        Session session;
        session.characters.vampireFound = !vampire;
        session.characters.oustersFound = vampire;
        try {
            session.refresh();
            FAIL() << "missing race row was accepted";
        } catch (const DisconnectException& error) {
            EXPECT_EQ(error.getMessage(), vampire ? "No Vampire" : "No Ousters");
        }
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
        EXPECT_EQ(session.sent, 0u);
        expectIdentity(session);
        session.characters.vampireFound = session.characters.oustersFound = true;
        session.refresh();
        EXPECT_EQ(session.sent, 1u);
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_PC_MANAGEMENT);
    }
}

TEST(LoginCharacterListRefresh, DuplicateRecordsDoNotSendOrAdvanceThePhaseAndPermitRetry) {
    Session session;
    session.characters.ousters.slot = "SLOT1";
    EXPECT_THROW(session.refresh(), DuplicatedException);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
    EXPECT_EQ(session.sent, 0u);
    expectIdentity(session);
    session.characters.ousters.slot = "SLOT3";
    session.refresh();
    EXPECT_EQ(session.sent, 1u);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_PC_MANAGEMENT);
}

TEST(LoginCharacterListRefresh, DefaultSendingBuffersCompleteAndEmptyFramesBeforePublication) {
    for (bool empty : {false, true}) {
        Session session;
        if (empty)
            session.characters.rows.clear();
        session.actions = de::defaultLoginCharacterListRefreshActions();
        PacketSize_t expectedSize = 0;
        session.player.observe = [&](Packet& packet) {
            EXPECT_EQ(packet.getPacketID(), Packet::PACKET_LC_PC_LIST);
            EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
            expectedSize = packet.getPacketSize();
            if (!empty)
                expectCompleteReply(dynamic_cast<LCPCList&>(packet));
            expectIdentity(session);
        };
        session.refresh();
        const auto bytes = session.player.bufferedBytes();
        ASSERT_EQ(bytes.size(), szPacketHeader + expectedSize);
        PacketID_t packetID;
        PacketSize_t bodySize;
        std::memcpy(&packetID, bytes.data(), szPacketID);
        std::memcpy(&bodySize, bytes.data() + szPacketID, szPacketSize);
        EXPECT_EQ(packetID, Packet::PACKET_LC_PC_LIST);
        EXPECT_EQ(bodySize, expectedSize);
        EXPECT_EQ(bytes.substr(szPacketHeader, SLOT_MAX), empty ? "000" : "SVO");
        if (empty)
            EXPECT_EQ(bytes.substr(szPacketHeader), "000");
        EXPECT_EQ(session.player.sent, 1u);
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_PC_MANAGEMENT);
    }
}

TEST(LoginCharacterListRefresh, ProductionSerializationFailureDoesNotPublishAndCanRetry) {
    Session session;
    session.actions = de::defaultLoginCharacterListRefreshActions();
    session.characters.rows.front().name.clear();
    EXPECT_THROW(session.refresh(), InvalidProtocolException);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
    EXPECT_EQ(session.player.sent, 0u);
    EXPECT_TRUE(session.player.bufferedBytes().empty());
    expectIdentity(session);
    session.characters.rows.front().name = "Slayer";
    session.refresh();
    EXPECT_EQ(session.player.sent, 1u);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_PC_MANAGEMENT);
}

TEST(LoginCharacterListRefresh, FailureAfterBufferingKeepsTheOutputWithoutPublishingThePhase) {
    Session session;
    session.characters.rows.clear();
    session.actions.send = [&](LoginPlayer& player, LCPCList& reply) {
        player.sendPacket(&reply);
        throw DisconnectException("failure after buffering");
    };
    EXPECT_THROW(session.refresh(), DisconnectException);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
    EXPECT_EQ(session.player.bufferedBytes().substr(szPacketHeader), "000");
    expectIdentity(session);
    session.actions = de::defaultLoginCharacterListRefreshActions();
    session.refresh();
    EXPECT_EQ(session.player.bufferedBytes().size(), 2 * (szPacketHeader + SLOT_MAX));
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_PC_MANAGEMENT);
}

int checkAllocationFailure(std::size_t position, int mode) {
    auto session = std::make_unique<Session>();
    const std::string account(64, 'a');
    session->player.setID(account);
    session->characters.rows[0].name = std::string(20, 's');
    session->characters.vampire.name = std::string(20, 'v');
    session->characters.ousters.name = std::string(20, 'o');
    if (mode == 1)
        session->characters.rows.clear();
    if (mode == 2)
        session->characters.oustersFound = false;
    if (mode == 3) {
        session->characters.failureStage = Stage::Ousters;
        session->characters.failure = std::make_exception_ptr(DatabaseError("query failed"));
    }
    const auto sendFailure = std::make_exception_ptr(std::runtime_error("send failed"));
    session->actions.send = [&](LoginPlayer&, LCPCList& reply) {
        SocketOutputStream output(nullptr, 1024);
        reply.write(output);
        if (mode == 4)
            std::rethrow_exception(sendFailure);
        ++session->sent;
    };
    AllocationProbe probe(position);
    bool failed = false;
    bool disconnected = false;
    bool sendFailed = false;
    try {
        session->refresh();
    } catch (const std::bad_alloc&) {
        failed = true;
    } catch (const DisconnectException& error) {
        disconnected = (mode == 2 && error.getMessage() == "No Ousters") ||
                       (mode == 3 && error.getMessage() == "LoginPlayer::makePCList : query failed");
        if (!disconnected)
            return 1;
    } catch (...) {
        sendFailed = mode == 4 && std::current_exception() == sendFailure;
        if (!sendFailed)
            return 2;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (position == 96 && failed) ||
        (!failed && ((mode == 2 || mode == 3) != disconnected || (mode == 4) != sendFailed)))
        return 3;
    if (!session->player.loginAccountOwnership().owns("account") ||
        session->player.getPlayerStatus() != (session->sent ? LPS_PC_MANAGEMENT : LPS_WAITING_FOR_CL_GET_PC_LIST))
        return 4;
    if (!session->sent) {
        session->characters.oustersFound = true;
        session->characters.failure = nullptr;
        mode = 0;
        session->refresh();
    }
    if (session->sent != 1 || session->player.getPlayerStatus() != LPS_PC_MANAGEMENT)
        return 5;
    session.reset();
    return probe.outstanding() == 0 ? 0 : 6;
}

TEST(LoginCharacterListRefresh, AllocationFailuresReleasePartialAndCompleteRepliesAndPermitRetry) {
    for (int mode = 0; mode < 5; ++mode) {
        for (std::size_t position = 1; position <= 96; ++position) {
            SCOPED_TRACE(::testing::Message() << mode << "/" << position);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure(position, mode)), ::testing::ExitedWithCode(0), "");
        }
    }
}

} // namespace
