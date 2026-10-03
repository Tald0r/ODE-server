#include <unistd.h>

#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <memory>
#include <new>
#include <stdexcept>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "CLCreatePC.h"
#include "CharacterCreation.h"
#include "DatabaseError.h"
#include "LCCreatePCError.h"
#include "LCCreatePCOK.h"
#include "LoginCharacterCreation.h"
#include "LoginPlayer.h"
#include "Socket.h"
#include "SocketOutputStream.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginCharacterRepository.h"

namespace {

enum class Stage { Name, Slot, Balance, Slayer, Vampire, Ousters, Flags, Refusal, Success };

class Repository : public FakeLoginCharacterRepository {
public:
    void visit(Stage stage, WorldID_t world) {
        worlds.push_back(world);
        if (before)
            before(stage);
    }
    bool slayerNameExists(WorldID_t world, const std::string& name) override {
        visit(Stage::Name, world);
        return FakeLoginCharacterRepository::slayerNameExists(world, name);
    }
    bool slotOccupied(WorldID_t world, const std::string& account, const std::string& slot) override {
        visit(Stage::Slot, world);
        return FakeLoginCharacterRepository::slotOccupied(world, account, slot);
    }
    bool loadRankGoalExp(WorldID_t world, int rank, int& value) override {
        visit(Stage::Balance, world);
        return FakeLoginCharacterRepository::loadRankGoalExp(world, rank, value);
    }
    void insertSlayer(WorldID_t world, const LoginNewSlayer& row) override {
        visit(Stage::Slayer, world);
        FakeLoginCharacterRepository::insertSlayer(world, row);
    }
    void insertVampire(WorldID_t world, const LoginNewVampire& row) override {
        visit(Stage::Vampire, world);
        FakeLoginCharacterRepository::insertVampire(world, row);
    }
    void insertOusters(WorldID_t world, const LoginNewOusters& row) override {
        visit(Stage::Ousters, world);
        FakeLoginCharacterRepository::insertOusters(world, row);
    }
    void insertFlagSet(WorldID_t world, const std::string& name, LoginFlagSetPreset preset) override {
        visit(Stage::Flags, world);
        FakeLoginCharacterRepository::insertFlagSet(world, name, preset);
    }
    std::function<void(Stage)> before;
    std::vector<int> worlds;
};

class CreationPlayer : public LoginPlayer {
public:
    CreationPlayer() : LoginPlayer(new Socket("198.51.100.7", 1234)) {
        setID("account");
        setWorldID(7);
        setServerGroupID(9);
        setPlayerStatus(LPS_PC_MANAGEMENT);
        loginAccountOwnership().acquire("account", [] { return true; });
    }
    ~CreationPlayer() override {
        setPlayerStatus(LPS_END_SESSION);
    }
    void sendPacket(Packet* packet) override {
        if (observe)
            observe(*packet);
        LoginPlayer::sendPacket(packet);
    }
    std::string bufferedBytes() const {
        return {m_pOutputStream->getBuffer(), m_pOutputStream->length()};
    }
    std::function<void(Packet&)> observe;
};

struct Session {
    explicit Session(Race_t race = RACE_SLAYER) {
        packet.setName("Rowan");
        packet.setSlot(SLOT2);
        packet.setSex(MALE);
        packet.setHairStyle(HAIR_STYLE3);
        packet.setHairColor(7);
        packet.setSkinColor(9);
        packet.setRace(race);
        const Attr_t attribute = race == RACE_VAMPIRE ? 20 : race == RACE_OUSTERS ? 15 : 10;
        packet.setSTR(attribute);
        packet.setDEX(attribute);
        packet.setINT(attribute);
        repository.rankGoalExp = {{0, 1000}, {1, 1100}, {2, 1200}};
        repository.vampireGoalExp[1] = 2000;
        repository.oustersGoalExp[1] = 3000;
    }
    bool create() {
        return de::createLoginCharacter(player, packet, repository, balance, actions);
    }
    CreationPlayer player;
    CLCreatePC packet;
    Repository repository;
    CreatePCBalanceCache balance;
    unsigned successes = 0;
    std::vector<BYTE> refusals;
    de::LoginCharacterCreationActions actions{
        [&](LoginPlayer&, LCCreatePCError& reply) { refusals.push_back(reply.getErrorID()); },
        [&](LoginPlayer&, LCCreatePCOK&) { ++successes; },
        {+[] { return 0u; }, {}}};
};

void expectOwned(const Session& session, PlayerStatus status = LPS_PC_MANAGEMENT) {
    EXPECT_EQ(session.player.getPlayerStatus(), status);
    EXPECT_EQ(session.player.getID(), "account");
    EXPECT_EQ(session.player.getWorldID(), 7);
    EXPECT_EQ(session.player.getServerGroupID(), 9);
    EXPECT_TRUE(session.player.loginAccountOwnership().owns("account"));
}

void expectNoWrites(const Session& session) {
    EXPECT_TRUE(session.repository.insertedSlayers.empty());
    EXPECT_TRUE(session.repository.insertedVampires.empty());
    EXPECT_TRUE(session.repository.insertedOusters.empty());
    EXPECT_TRUE(session.repository.insertedFlagSets.empty());
    EXPECT_EQ(session.successes, 0u);
}

TEST(LoginCharacterCreation, EveryRaceAndSlotKeepsTheWriteOrderAndPublishesAfterTheReply) {
    for (const Race_t race : {RACE_SLAYER, RACE_VAMPIRE, RACE_OUSTERS}) {
        for (int slot = SLOT1; slot < SLOT_MAX; ++slot) {
            SCOPED_TRACE(::testing::Message() << int(race) << "/" << slot);
            Session session(race);
            session.packet.setSlot(static_cast<Slot>(slot));
            std::vector<Stage> effects;
            session.repository.before = [&](Stage stage) {
                if (stage >= Stage::Slayer) {
                    effects.push_back(stage);
                    expectOwned(session);
                }
            };
            session.actions.sendSuccess = [&](LoginPlayer& player, LCCreatePCOK& reply) {
                EXPECT_EQ(&player, &session.player);
                EXPECT_EQ(reply.getPacketID(), Packet::PACKET_LC_CREATE_PC_OK);
                SocketOutputStream output(nullptr, 64);
                reply.write(output);
                EXPECT_EQ(output.length(), 0u);
                EXPECT_EQ(session.repository.insertedFlagSets.size(), 1u);
                effects.push_back(Stage::Success);
                expectOwned(session);
                ++session.successes;
            };
            EXPECT_TRUE(session.create());
            EXPECT_EQ(effects,
                      (std::vector<Stage>{Stage::Slayer, race == RACE_OUSTERS ? Stage::Ousters : Stage::Vampire,
                                          Stage::Flags, Stage::Success}));
            ASSERT_EQ(session.repository.insertedSlayers.size(), 1u);
            const auto& slayer = session.repository.insertedSlayers.front();
            EXPECT_EQ(slayer.name, "Rowan");
            EXPECT_EQ(slayer.playerID, "account");
            EXPECT_EQ(slayer.serverGroupID, 9);
            EXPECT_EQ(slayer.slot, Slot2String[slot]);
            EXPECT_EQ(slayer.sex, "MALE");
            EXPECT_EQ(slayer.hairStyle, "HAIR_STYLE3");
            EXPECT_EQ(slayer.hairColor, 7);
            EXPECT_EQ(slayer.skinColor, 9);
            EXPECT_EQ(slayer.str, session.packet.getSTR());
            EXPECT_EQ(slayer.dex, session.packet.getDEX());
            EXPECT_EQ(slayer.inte, session.packet.getINT());
            if (race == RACE_VAMPIRE) {
                EXPECT_GE(slayer.str, 5);
                EXPECT_LE(slayer.str, 20);
                EXPECT_GE(slayer.dex, 5);
                EXPECT_GE(slayer.inte, 5);
                EXPECT_EQ(slayer.str + slayer.dex + slayer.inte, 30);
            }
            if (race == RACE_OUSTERS) {
                ASSERT_EQ(session.repository.insertedOusters.size(), 1u);
                EXPECT_EQ(session.repository.insertedOusters.front().name, "Rowan");
                EXPECT_EQ(session.repository.insertedOusters.front().slot, Slot2String[slot]);
                EXPECT_TRUE(session.repository.insertedVampires.empty());
            } else {
                ASSERT_EQ(session.repository.insertedVampires.size(), 1u);
                EXPECT_EQ(session.repository.insertedVampires.front().name, "Rowan");
                EXPECT_EQ(session.repository.insertedVampires.front().slot, Slot2String[slot]);
                EXPECT_TRUE(session.repository.insertedOusters.empty());
            }
            EXPECT_EQ(session.repository.insertedFlagSets,
                      (std::vector<std::pair<std::string, LoginFlagSetPreset>>{
                          {"Rowan", race == RACE_SLAYER ? LOGIN_FLAGSET_SLAYER : LOGIN_FLAGSET_OTHER}}));
            for (const auto world : session.repository.worlds)
                EXPECT_EQ(world, 7);
            EXPECT_EQ(session.successes, 1u);
            EXPECT_TRUE(session.refusals.empty());
            expectOwned(session, LPS_WAITING_FOR_CL_GET_PC_LIST);
        }
    }
}

TEST(LoginCharacterCreation, OrdinaryRefusalsKeepTheirLiteralByteAndNeverWrite) {
    for (int rejection = 0; rejection < 4; ++rejection) {
        Session session;
        if (rejection == 0)
            session.packet.setName("NONE");
        else if (rejection == 1)
            session.repository.existingNames.insert("Rowan");
        else if (rejection == 2)
            session.repository.occupiedSlots.insert({"account", "SLOT2"});
        else
            session.packet.setRace(255);
        unsigned replies = 0;
        session.actions.sendRefusal = [&](LoginPlayer& player, LCCreatePCError& reply) {
            EXPECT_EQ(&player, &session.player);
            SocketOutputStream output(nullptr, 64);
            reply.write(output);
            ASSERT_EQ(output.length(), 1u);
            EXPECT_EQ(static_cast<BYTE>(output.getBuffer()[0]), rejection == 3 ? 15 : 2);
            expectOwned(session);
            expectNoWrites(session);
            ++replies;
        };
        EXPECT_FALSE(session.create());
        EXPECT_EQ(replies, 1u);
        EXPECT_EQ(session.packet.getSTR(), 10);
        EXPECT_EQ(session.packet.getDEX(), 10);
        EXPECT_EQ(session.packet.getINT(), 10);
        if (rejection == 0)
            EXPECT_TRUE(session.repository.worlds.empty());
        expectOwned(session);
        expectNoWrites(session);
    }
}

TEST(LoginCharacterCreation, InvalidAttributesSlotAndHairStyleKeepTheirFatalMessages) {
    const char* const messages[] = {"CLCreatePCHandler::too large character attribute",
                                    "CLCreatePCHandler::slot out of range",
                                    "CLCreatePCHandler::hair style out of range"};
    for (int rejection = 0; rejection < 3; ++rejection) {
        Session session;
        if (rejection == 0)
            session.packet.setSTR(21);
        else if (rejection == 1)
            session.packet.setSlot(SLOT_MAX);
        else
            session.packet.setHairStyle(static_cast<HairStyle>(3));
        try {
            (void)session.create();
            FAIL() << "invalid creation was accepted";
        } catch (const InvalidProtocolException& error) {
            EXPECT_EQ(error.getMessage(), messages[rejection]);
        }
        EXPECT_TRUE(session.refusals.empty());
        expectOwned(session);
        expectNoWrites(session);
    }
}

TEST(LoginCharacterCreation, SnapshotFieldsRemainStableAcrossRepositoryCallbacks) {
    Session session;
    session.repository.before = [&](Stage stage) {
        if (stage == Stage::Name) {
            session.packet.setName("different");
            session.packet.setRace(RACE_OUSTERS);
            session.packet.setSlot(SLOT3);
            session.packet.setSTR(99);
            session.packet.setHairColor(88);
        }
    };
    EXPECT_TRUE(session.create());
    ASSERT_EQ(session.repository.insertedSlayers.size(), 1u);
    const auto& slayer = session.repository.insertedSlayers.front();
    EXPECT_EQ(slayer.name, "Rowan");
    EXPECT_EQ(slayer.race, "SLAYER");
    EXPECT_EQ(slayer.slot, "SLOT2");
    EXPECT_EQ(slayer.str, 10);
    EXPECT_EQ(slayer.hairColor, 7);
    EXPECT_EQ(session.packet.getSTR(), 10);
    EXPECT_EQ(session.packet.getName(), "different");
    EXPECT_EQ(session.repository.insertedVampires.size(), 1u);
    EXPECT_TRUE(session.repository.insertedOusters.empty());
    EXPECT_EQ(session.repository.insertedFlagSets.front().first, "Rowan");
}

constexpr Stage failureStages[] = {Stage::Name,    Stage::Slot,  Stage::Balance, Stage::Slayer, Stage::Vampire,
                                   Stage::Ousters, Stage::Flags, Stage::Success, Stage::Refusal};

void installFailure(Session& session, Stage stage, std::exception_ptr failure) {
    if (stage == Stage::Success)
        session.actions.sendSuccess = [failure](LoginPlayer&, LCCreatePCOK&) { std::rethrow_exception(failure); };
    else if (stage == Stage::Refusal) {
        session.packet.setName("NONE");
        session.actions.sendRefusal = [failure](LoginPlayer&, LCCreatePCError&) { std::rethrow_exception(failure); };
    } else
        session.repository.before = [stage, failure](Stage current) {
            if (stage == current)
                std::rethrow_exception(failure);
        };
}

void expectPartialWrites(const Session& session, Stage stage) {
    const bool hasSlayer =
        stage == Stage::Vampire || stage == Stage::Ousters || stage == Stage::Flags || stage == Stage::Success;
    const bool hasRace = stage == Stage::Flags || stage == Stage::Success;
    EXPECT_EQ(session.repository.insertedSlayers.size(), hasSlayer ? 1u : 0u);
    EXPECT_EQ(session.repository.insertedVampires.size() + session.repository.insertedOusters.size(),
              hasRace ? 1u : 0u);
    EXPECT_EQ(session.repository.insertedFlagSets.size(), stage == Stage::Success ? 1u : 0u);
    EXPECT_EQ(session.successes, 0u);
    expectOwned(session);
}

TEST(LoginCharacterCreation, DatabaseFailuresSendTheLegacyRefusalAndRetainEarlierWrites) {
    for (const auto stage : failureStages) {
        if (stage == Stage::Refusal)
            continue;
        SCOPED_TRACE(static_cast<int>(stage));
        Session session(stage == Stage::Ousters ? RACE_OUSTERS : RACE_SLAYER);
        installFailure(session, stage, std::make_exception_ptr(DatabaseError("query failed")));
        EXPECT_FALSE(session.create());
        EXPECT_EQ(session.refusals, (std::vector<BYTE>{15}));
        expectPartialWrites(session, stage);
    }
}

TEST(LoginCharacterCreation, OtherFailuresKeepTheirIdentityAndEarlierWrites) {
    const std::exception_ptr failures[] = {std::make_exception_ptr(NoSuchElementException("balance missing")),
                                           std::make_exception_ptr(Error("creation failed")),
                                           std::make_exception_ptr(std::runtime_error("callback failed")),
                                           std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        for (const auto stage : failureStages) {
            SCOPED_TRACE(static_cast<int>(stage));
            Session session(stage == Stage::Ousters ? RACE_OUSTERS : RACE_SLAYER);
            installFailure(session, stage, failure);
            try {
                (void)session.create();
                FAIL() << "failure was swallowed";
            } catch (...) {
                EXPECT_EQ(std::current_exception(), failure);
            }
            EXPECT_TRUE(session.refusals.empty());
            expectPartialWrites(session, stage);
        }
    }
}

TEST(LoginCharacterCreation, VampireAttributesArePublishedBeforeWritingAndRemainAfterFailure) {
    for (const auto stage : {Stage::Slayer, Stage::Vampire, Stage::Flags, Stage::Success}) {
        Session session(RACE_VAMPIRE);
        installFailure(session, stage, std::make_exception_ptr(DatabaseError("insert failed")));
        EXPECT_FALSE(session.create());
        EXPECT_EQ(session.packet.getSTR() + session.packet.getDEX() + session.packet.getINT(), 30);
        EXPECT_GE(session.packet.getSTR(), 5);
        EXPECT_GE(session.packet.getDEX(), 5);
        EXPECT_GE(session.packet.getINT(), 5);
        expectPartialWrites(session, stage);
        if (!session.repository.insertedSlayers.empty()) {
            EXPECT_EQ(session.repository.insertedSlayers.front().str, session.packet.getSTR());
            EXPECT_EQ(session.repository.insertedSlayers.front().dex, session.packet.getDEX());
            EXPECT_EQ(session.repository.insertedSlayers.front().inte, session.packet.getINT());
        }
    }
}

TEST(LoginCharacterCreation, ADatabaseFailureSendingARefusalTriggersOneGenericRefusalAttempt) {
    Session session;
    session.packet.setName("NONE");
    session.actions.sendRefusal = [&](LoginPlayer&, LCCreatePCError& reply) {
        session.refusals.push_back(reply.getErrorID());
        if (session.refusals.size() == 1)
            throw DatabaseError("first refusal failed");
    };
    EXPECT_FALSE(session.create());
    EXPECT_EQ(session.refusals, (std::vector<BYTE>{2, 15}));
    expectOwned(session);
    expectNoWrites(session);
}

TEST(LoginCharacterCreation, AFailedDatabaseErrorReplyPropagatesWithoutAnotherAttempt) {
    const std::exception_ptr failures[] = {std::make_exception_ptr(DatabaseError("error reply failed")),
                                           std::make_exception_ptr(DisconnectException("peer lost")),
                                           std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        Session session;
        installFailure(session, Stage::Flags, std::make_exception_ptr(DatabaseError("flags failed")));
        unsigned replies = 0;
        session.actions.sendRefusal = [&](LoginPlayer&, LCCreatePCError& reply) {
            EXPECT_EQ(reply.getErrorID(), 15);
            ++replies;
            std::rethrow_exception(failure);
        };
        try {
            (void)session.create();
            FAIL() << "error reply failure was swallowed";
        } catch (...) {
            EXPECT_EQ(std::current_exception(), failure);
        }
        EXPECT_EQ(replies, 1u);
        expectPartialWrites(session, Stage::Flags);
    }
}

TEST(LoginCharacterCreation, AFailedReadCanBeRetriedButPersistedNamesAreNotRecreated) {
    for (bool persisted : {false, true}) {
        Session session;
        installFailure(session, persisted ? Stage::Flags : Stage::Name,
                       std::make_exception_ptr(DatabaseError("retry boundary")));
        EXPECT_FALSE(session.create());
        session.repository.before = {};
        EXPECT_EQ(session.create(), !persisted);
        EXPECT_EQ(session.repository.insertedSlayers.size(), 1u);
        EXPECT_EQ(session.repository.insertedVampires.size(), 1u);
        if (persisted) {
            EXPECT_EQ(session.refusals, (std::vector<BYTE>{15, 2}));
            EXPECT_TRUE(session.repository.insertedFlagSets.empty());
            expectOwned(session);
        } else {
            EXPECT_EQ(session.successes, 1u);
            expectOwned(session, LPS_WAITING_FOR_CL_GET_PC_LIST);
        }
    }
}

TEST(LoginCharacterCreation, ExplicitCachesCanBeSharedOrScopedToIndependentCallers) {
    Session first;
    EXPECT_TRUE(first.create());
    Session second;
    second.repository.rankGoalExp[0] = 9999;
    EXPECT_TRUE(
        de::createLoginCharacter(second.player, second.packet, second.repository, first.balance, second.actions));
    EXPECT_EQ(second.repository.rankGoalExpCalls, 0);
    EXPECT_EQ(second.repository.insertedSlayers.front().rankGoalExp, 1000);
    Session independent;
    independent.repository.rankGoalExp[0] = 9999;
    EXPECT_TRUE(independent.create());
    EXPECT_EQ(independent.repository.rankGoalExpCalls, 3);
    EXPECT_EQ(independent.repository.insertedSlayers.front().rankGoalExp, 9999);
}

TEST(LoginCharacterCreation, DefaultActionsUseTheRealPlayerOutputForBothReplyKinds) {
    for (bool refused : {false, true}) {
        Session session;
        session.actions = de::defaultLoginCharacterCreationActions();
        if (refused)
            session.packet.setName("NONE");
        unsigned replies = 0;
        session.player.observe = [&](Packet& reply) {
            EXPECT_EQ(reply.getPacketID(),
                      refused ? Packet::PACKET_LC_CREATE_PC_ERROR : Packet::PACKET_LC_CREATE_PC_OK);
            expectOwned(session);
            ++replies;
        };
        EXPECT_EQ(session.create(), !refused);
        EXPECT_EQ(replies, 1u);
        const auto bytes = session.player.bufferedBytes();
        ASSERT_EQ(bytes.size(), szPacketHeader + (refused ? 1 : 0));
        if (refused)
            EXPECT_EQ(static_cast<BYTE>(bytes[szPacketHeader]), 2);
    }
}

TEST(LoginCharacterCreation, TheProductionHandlerCanRefuseWithoutDatabaseOrProcessContexts) {
    Session session;
    session.packet.setName("NONE");
    CLCreatePCHandler::execute(&session.packet, &session.player);
    const auto bytes = session.player.bufferedBytes();
    ASSERT_EQ(bytes.size(), szPacketHeader + 1);
    EXPECT_EQ(static_cast<BYTE>(bytes[szPacketHeader]), 2);
    expectOwned(session);
    expectNoWrites(session);
}

int checkAllocationFailure(std::size_t position, int mode) {
    auto session = std::make_unique<Session>(mode == 4 ? RACE_VAMPIRE : mode == 5 ? RACE_OUSTERS : RACE_SLAYER);
    session->player.setID(std::string(64, 'a'));
    session->packet.setName(std::string(64, 'n'));
    session->actions.sendSuccess = [&](LoginPlayer&, LCCreatePCOK& reply) {
        SocketOutputStream output(nullptr, 64);
        reply.write(output);
        ++session->successes;
    };
    session->actions.sendRefusal = [&](LoginPlayer&, LCCreatePCError& reply) {
        SocketOutputStream output(nullptr, 64);
        reply.write(output);
        session->refusals.push_back(reply.getErrorID());
    };
    if (mode == 1)
        session->packet.setName(std::string(64, 'n') + "NONE");
    if (mode == 2)
        installFailure(*session, Stage::Name, std::make_exception_ptr(DatabaseError("lookup failed")));
    if (mode == 3)
        installFailure(*session, Stage::Flags, std::make_exception_ptr(DatabaseError("flags failed")));
    bool ignoredReportingFailure = false;
    if (mode == 4)
        session->actions.decision.random = [] {
            const std::string storage(96, 'r');
            return static_cast<unsigned>(storage.front());
        };
    if (mode == 5) {
        session->packet.setSTR(5);
        session->packet.setDEX(20);
        session->packet.setINT(20);
        session->actions.decision.reportLowOusters = [&](const CreatePCRequest& request) {
            try {
                const std::string text = request.playerID + ":" + request.name;
                if (text.empty())
                    std::_Exit(8);
            } catch (...) {
                ignoredReportingFailure = true;
                throw;
            }
        };
    }
    const bool shouldAccept = mode == 0 || mode >= 4;
    AllocationProbe probe(position);
    bool failed = false;
    bool accepted = false;
    try {
        accepted = session->create();
    } catch (const std::bad_alloc&) {
        failed = true;
    } catch (...) {
        return 1;
    }
    probe.stopFailing();
    if ((failed || ignoredReportingFailure) != probe.rejected() || (position == 96 && failed) ||
        (!failed && accepted != shouldAccept))
        return 2;
    if (!session->player.loginAccountOwnership().owns("account"))
        return 3;
    if (session->successes) {
        if (!shouldAccept || failed || session->player.getPlayerStatus() != LPS_WAITING_FOR_CL_GET_PC_LIST ||
            session->repository.insertedFlagSets.size() != 1)
            return 4;
    } else if (session->player.getPlayerStatus() != LPS_PC_MANAGEMENT) {
        return 5;
    }
    if (failed && session->repository.insertedSlayers.empty() && shouldAccept &&
        (mode != 4 || session->packet.getSTR() + session->packet.getDEX() + session->packet.getINT() == 60)) {
        if (!session->create() || session->successes != 1)
            return 6;
    }
    session.reset();
    return probe.outstanding() == 0 ? 0 : 7;
}

TEST(LoginCharacterCreation, AllocationFailuresRetainTheCallerAndReleasePreparedRowsAndReplies) {
    for (int mode = 0; mode < 6; ++mode) {
        for (std::size_t position = 1; position <= 96; ++position) {
            SCOPED_TRACE(::testing::Message() << mode << "/" << position);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure(position, mode)), ::testing::ExitedWithCode(0), "");
        }
    }
}

TEST(LoginCharacterCreation, AReporterDatabaseErrorCannotTurnAValidCreationIntoAGenericRefusal) {
    Session session(RACE_OUSTERS);
    session.packet.setSTR(5);
    session.packet.setDEX(20);
    session.packet.setINT(20);
    session.actions.decision.reportLowOusters = [](const CreatePCRequest&) { throw DatabaseError("report failed"); };
    EXPECT_TRUE(session.create());
    EXPECT_EQ(session.successes, 1u);
    EXPECT_TRUE(session.refusals.empty());
    EXPECT_EQ(session.repository.insertedSlayers.size(), 1u);
    EXPECT_EQ(session.repository.insertedOusters.size(), 1u);
}

class CreationLogs {
public:
    CreationLogs() {
        if (!::mkdtemp(directory) || ::chdir(directory) != 0)
            std::_Exit(80);
    }
    ~CreationLogs() {
        ::unlink("CreatePC.log");
        if (::chdir("/") != 0 || ::rmdir(directory) != 0)
            std::_Exit(81);
    }

private:
    char directory[40] = "/tmp/darkeden-creation-XXXXXX";
};

int checkProductionLoggingFailure(std::size_t position) {
    CreationLogs logs;
    Session session(RACE_OUSTERS);
    session.packet.setSTR(5);
    session.packet.setDEX(20);
    session.packet.setINT(20);
    const auto& production = de::defaultLoginCharacterCreationActions().decision;
    bool injected = false;
    session.actions.decision.reportLowOusters = [&](const CreatePCRequest& request) {
        AllocationProbe probe(position);
        try {
            production.reportLowOusters(request);
        } catch (...) {
            injected = probe.rejected();
            throw;
        }
        injected = probe.rejected();
    };
    try {
        if (!session.create())
            return 1;
    } catch (...) {
        return 2;
    }
    if (position == 1 && !injected)
        return 3;
    return session.successes == 1 && session.repository.insertedOusters.size() == 1 ? 0 : 4;
}

TEST(LoginCharacterCreation, ProductionLoggingAllocationFailureCannotInterruptCreation) {
    for (std::size_t position = 1; position <= 16; ++position) {
        SCOPED_TRACE(position);
        ASSERT_EXIT(std::_Exit(checkProductionLoggingFailure(position)), ::testing::ExitedWithCode(0), "");
    }
}

TEST(LoginCharacterCreation, ExplicitDrawsReachThePacketRowsAndSessionWithoutProcessRandomState) {
    Session session(RACE_VAMPIRE);
    unsigned draws = 0;
    session.actions.decision.random = [&] { return draws++ == 0 ? 7u : 5u; };
    EXPECT_TRUE(session.create());
    EXPECT_EQ(draws, 2u);
    EXPECT_EQ(session.packet.getSTR(), 12);
    EXPECT_EQ(session.packet.getDEX(), 10);
    EXPECT_EQ(session.packet.getINT(), 8);
    ASSERT_EQ(session.repository.insertedSlayers.size(), 1u);
    const auto& row = session.repository.insertedSlayers.front();
    EXPECT_EQ(row.str, 12);
    EXPECT_EQ(row.dex, 10);
    EXPECT_EQ(row.inte, 8);
    EXPECT_EQ(row.hp, 24);
    EXPECT_EQ(row.mp, 16);
    EXPECT_EQ(session.repository.insertedVampires.size(), 1u);
    expectOwned(session, LPS_WAITING_FOR_CL_GET_PC_LIST);
}

TEST(LoginCharacterCreation, RandomFailuresPreserveTheRequestAndFollowTheExistingExceptionBoundary) {
    const std::exception_ptr failures[] = {std::make_exception_ptr(DatabaseError("random source database failure")),
                                           std::make_exception_ptr(Error("random unavailable")),
                                           std::make_exception_ptr(std::runtime_error("draw failed")),
                                           std::make_exception_ptr(std::bad_alloc())};
    for (int kind = 0; kind < 4; ++kind) {
        for (unsigned failAt : {1u, 2u}) {
            Session session(RACE_VAMPIRE);
            unsigned draws = 0;
            session.actions.decision.random = [&] {
                if (++draws == failAt)
                    std::rethrow_exception(failures[kind]);
                return 15u;
            };
            if (kind == 0) {
                EXPECT_FALSE(session.create());
                EXPECT_EQ(session.refusals, (std::vector<BYTE>{15}));
            } else {
                try {
                    (void)session.create();
                    FAIL() << "random failure was swallowed";
                } catch (...) {
                    EXPECT_EQ(std::current_exception(), failures[kind]);
                }
                EXPECT_TRUE(session.refusals.empty());
            }
            EXPECT_EQ(draws, failAt);
            EXPECT_EQ(session.packet.getSTR(), 20);
            EXPECT_EQ(session.packet.getDEX(), 20);
            EXPECT_EQ(session.packet.getINT(), 20);
            expectNoWrites(session);
            expectOwned(session);
            EXPECT_TRUE(session.create());
            EXPECT_EQ(draws, failAt + 2);
            EXPECT_EQ(session.repository.rankGoalExpCalls, 3);
            EXPECT_EQ(session.packet.getSTR(), 20);
            EXPECT_EQ(session.packet.getDEX(), 5);
            EXPECT_EQ(session.packet.getINT(), 5);
            expectOwned(session, LPS_WAITING_FOR_CL_GET_PC_LIST);
        }
    }
}

TEST(LoginCharacterCreation, ReporterExceptionsCannotReplaceEitherAcceptanceOrFatalAttributeRejection) {
    const std::exception_ptr failures[] = {std::make_exception_ptr(DatabaseError("report database failure")),
                                           std::make_exception_ptr(NoSuchElementException("report target missing")),
                                           std::make_exception_ptr(Error("report failed")),
                                           std::make_exception_ptr(std::runtime_error("output failed")),
                                           std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        for (bool accepted : {false, true}) {
            Session session(RACE_OUSTERS);
            session.packet.setSTR(5);
            session.packet.setDEX(accepted ? 20 : 15);
            session.packet.setINT(accepted ? 20 : 15);
            unsigned reports = 0;
            session.actions.decision.reportLowOusters = [&](const CreatePCRequest& request) {
                EXPECT_EQ(request.playerID, "account");
                EXPECT_EQ(request.name, "Rowan");
                EXPECT_EQ(request.str, 5);
                EXPECT_EQ(request.dex, accepted ? 20 : 15);
                EXPECT_EQ(request.inte, accepted ? 20 : 15);
                ++reports;
                std::rethrow_exception(failure);
            };
            if (accepted) {
                EXPECT_TRUE(session.create());
                EXPECT_EQ(session.successes, 1u);
                expectOwned(session, LPS_WAITING_FOR_CL_GET_PC_LIST);
            } else {
                EXPECT_THROW((void)session.create(), InvalidProtocolException);
                expectNoWrites(session);
                expectOwned(session);
            }
            EXPECT_EQ(reports, 1u);
            EXPECT_TRUE(session.refusals.empty());
        }
    }
}

int checkProductionRandom(unsigned seed) {
    std::srand(seed);
    const unsigned first = std::rand();
    const unsigned second = std::rand();
    const int next = std::rand();
    const unsigned str = 5 + first % 16;
    const unsigned dex = 5 + second % (21 - str);
    std::srand(seed);
    Session session(RACE_VAMPIRE);
    session.actions.decision = de::defaultLoginCharacterCreationActions().decision;
    if (!session.create() || session.packet.getSTR() != str || session.packet.getDEX() != dex ||
        session.packet.getINT() != 30 - str - dex || std::rand() != next)
        return 1;
    return session.successes == 1 ? 0 : 2;
}

TEST(LoginCharacterCreation, TheProductionAdapterPreservesTheRandomSequenceAndModuloRules) {
    for (unsigned seed : {0u, 1u, 17u, 0xdeadbeefu}) {
        ASSERT_EXIT(std::_Exit(checkProductionRandom(seed)), ::testing::ExitedWithCode(0), "");
    }
}

int checkExplicitIsolation() {
    CreationLogs logs;
    std::srand(0xcafe);
    const int next = std::rand();
    std::srand(0xcafe);
    Session vampire(RACE_VAMPIRE);
    if (!vampire.create())
        return 1;
    Session ousters(RACE_OUSTERS);
    ousters.packet.setSTR(5);
    ousters.packet.setDEX(20);
    ousters.packet.setINT(20);
    unsigned reports = 0;
    ousters.actions.decision.reportLowOusters = [&](const CreatePCRequest&) { ++reports; };
    if (!ousters.create() || reports != 1 || std::rand() != next || ::access("CreatePC.log", F_OK) == 0)
        return 2;
    return 0;
}

TEST(LoginCharacterCreation, SuppliedActionsLeaveTheProcessRandomSequenceAndLogDirectoryAlone) {
    ASSERT_EXIT(std::_Exit(checkExplicitIsolation()), ::testing::ExitedWithCode(0), "");
}

int checkProductionDiagnostic() {
    CreationLogs logs;
    Session accepted(RACE_OUSTERS);
    accepted.packet.setSTR(5);
    accepted.packet.setDEX(20);
    accepted.packet.setINT(20);
    accepted.actions.decision = de::defaultLoginCharacterCreationActions().decision;
    if (!accepted.create())
        return 1;
    Session refused(RACE_OUSTERS);
    refused.packet.setSTR(5);
    refused.actions.decision = de::defaultLoginCharacterCreationActions().decision;
    try {
        (void)refused.create();
        return 2;
    } catch (const InvalidProtocolException&) {
    }
    std::ifstream log("CreatePC.log");
    std::string line;
    if (!std::getline(log, line) || !line.ends_with(" : Illegal PC Create [account:Rowan] : 5/20/20"))
        return 3;
    if (!std::getline(log, line) || !line.ends_with(" : Illegal PC Create [account:Rowan] : 5/15/15"))
        return 4;
    return std::getline(log, line) ? 5 : 0;
}

TEST(LoginCharacterCreation, TheProductionDiagnosticRetainsItsFileTextAndPreRejectionTiming) {
    ASSERT_EXIT(std::_Exit(checkProductionDiagnostic()), ::testing::ExitedWithCode(0), "");
}

} // namespace
