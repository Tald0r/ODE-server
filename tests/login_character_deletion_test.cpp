#include <unistd.h>

#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <vector>

#include <gtest/gtest.h>

#include "CLDeletePC.h"
#include "DatabaseError.h"
#include "LCDeletePCError.h"
#include "LCDeletePCOK.h"
#include "LoginCharacterDeletion.h"
#include "LoginPlayer.h"
#include "Socket.h"
#include "SocketOutputStream.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginCharacterPurgeRepository.h"

namespace {

enum class Stage { Owner, Retire, Record, Purge, Refusal, Success };

class Repository : public FakeLoginCharacterPurgeRepository {
public:
    void visit(Stage stage) {
        calls.push_back(stage);
        if (before)
            before(stage);
    }
    bool loadActiveSlayerOwner(WorldID_t world, const std::string& name, std::string& owner) override {
        visit(Stage::Owner);
        return FakeLoginCharacterPurgeRepository::loadActiveSlayerOwner(world, name, owner);
    }
    bool retireSlayer(WorldID_t world, const std::string& name, Slot slot) override {
        visit(Stage::Retire);
        const bool retired = FakeLoginCharacterPurgeRepository::retireSlayer(world, name, slot);
        if (after)
            after(Stage::Retire);
        return retired;
    }
    void recordDeletion(const std::string& account, WorldID_t world, const std::string& name) override {
        visit(Stage::Record);
        FakeLoginCharacterPurgeRepository::recordDeletion(account, world, name);
        if (after)
            after(Stage::Record);
    }
    void purgeCharacterRows(WorldID_t world, const std::string& name, Slot slot) override {
        visit(Stage::Purge);
        FakeLoginCharacterPurgeRepository::purgeCharacterRows(world, name, slot);
        if (after)
            after(Stage::Purge);
    }
    std::vector<Stage> calls;
    std::function<void(Stage)> before;
    std::function<void(Stage)> after;
};

class DeletionPlayer : public LoginPlayer {
public:
    DeletionPlayer() : LoginPlayer(new Socket("198.51.100.7", 1234)) {
        setID("account");
        setWorldID(7);
        setServerGroupID(9);
        setPlayerStatus(LPS_PC_MANAGEMENT);
        loginAccountOwnership().acquire("account", [] { return true; });
    }
    ~DeletionPlayer() override {
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
    Session() {
        packet.setName("Rowan");
        packet.setSlot(SLOT2);
        packet.setSSN("test-ssn");
        repository.addActiveSlayer("Rowan", "account", SLOT2);
    }
    bool erase() {
        return de::deleteLoginCharacter(player, packet, repository, actions);
    }
    DeletionPlayer player;
    CLDeletePC packet;
    Repository repository;
    unsigned successes = 0;
    std::vector<BYTE> refusals;
    de::LoginCharacterDeletionActions actions{
        [&](LoginPlayer&, LCDeletePCError& reply) { refusals.push_back(reply.getErrorID()); },
        [&](LoginPlayer&, LCDeletePCOK&) { ++successes; },
        {}};
};

TEST(LoginCharacterDeletion, RequestReportingFailureCannotSkipRetirementAndDispatch) {
    Session session;
    session.actions.diagnostics.request = [](const CLDeletePC&) { throw std::runtime_error("report failed"); };
    EXPECT_NO_THROW(EXPECT_TRUE(session.erase()));
    EXPECT_EQ(session.successes, 1u);
    EXPECT_EQ(session.repository.purges.size(), 1u);
}

TEST(LoginCharacterDeletion, WrongOwnerAuditFailureCannotSkipEitherTheConsoleReportOrRefusal) {
    Session session;
    session.repository.addActiveSlayer("Rowan", "someone-else", SLOT2);
    session.actions.diagnostics.wrongOwner = [](const DeletePCRequest&) { throw std::bad_alloc(); };
    unsigned reports = 0;
    session.actions.diagnostics.refusal = [&](DeletePCRejection) { ++reports; };
    EXPECT_NO_THROW(EXPECT_FALSE(session.erase()));
    EXPECT_EQ(session.refusals, (std::vector<BYTE>{0}));
    EXPECT_EQ(reports, 1u);
    EXPECT_EQ(session.repository.retireSlayerCalls, 0);
}

TEST(LoginCharacterDeletion, RefusalReportingFailureCannotSkipTheMissingCharacterReply) {
    Session session;
    session.repository.activeSlayers.clear();
    session.actions.diagnostics.refusal = [](DeletePCRejection) { throw std::runtime_error("report failed"); };
    EXPECT_NO_THROW(EXPECT_FALSE(session.erase()));
    EXPECT_EQ(session.refusals, (std::vector<BYTE>{11}));
}

TEST(LoginCharacterDeletion, DatabaseReportingFailureCannotHideTheOriginalFailureReply) {
    Session session;
    session.repository.before = [](Stage) { throw DatabaseError("lookup failed"); };
    session.actions.diagnostics.databaseFailure = [](const std::string&) { throw std::runtime_error("report failed"); };
    EXPECT_NO_THROW(EXPECT_FALSE(session.erase()));
    EXPECT_EQ(session.refusals, (std::vector<BYTE>{0}));
}

class RejectedOutput : public std::streambuf {
    int_type overflow(int_type) override {
        return traits_type::eof();
    }
    std::streamsize xsputn(const char*, std::streamsize) override {
        return 0;
    }
};

class ConsoleBuffer {
public:
    explicit ConsoleBuffer(std::streambuf& buffer, bool throwing = false)
        : exceptions(std::cout.exceptions()), previous(std::cout.rdbuf(&buffer)) {
        if (throwing)
            std::cout.exceptions(std::ios::badbit | std::ios::failbit);
    }
    ~ConsoleBuffer() {
        std::cout.exceptions(std::ios::goodbit);
        std::cout.rdbuf(previous);
        std::cout.clear();
        std::cout.exceptions(exceptions);
    }

private:
    std::ios::iostate exceptions;
    std::streambuf* previous;
};

int checkBrokenOutput() {
    Session session;
    session.actions.diagnostics = de::defaultLoginCharacterDeletionActions().diagnostics;
    RejectedOutput output;
    ConsoleBuffer console(output, true);
    try {
        if (!session.erase())
            return 1;
    } catch (...) {
        return 2;
    }
    return session.successes == 1 && session.player.getPlayerStatus() == LPS_WAITING_FOR_CL_GET_PC_LIST ? 0 : 3;
}

TEST(LoginCharacterDeletion, BrokenProductionOutputCannotSkipDeletion) {
    ASSERT_EXIT(std::_Exit(checkBrokenOutput()), ::testing::ExitedWithCode(0), "");
}

void expectOwned(const Session& session, PlayerStatus status = LPS_PC_MANAGEMENT) {
    EXPECT_EQ(session.player.getPlayerStatus(), status);
    EXPECT_EQ(session.player.getID(), "account");
    EXPECT_EQ(session.player.getServerGroupID(), 9);
    EXPECT_TRUE(session.player.loginAccountOwnership().owns("account"));
}

TEST(LoginCharacterDeletion, EveryRejectionKeepsItsLiteralByteAndSkipsRecordAndPurge) {
    for (int reason = 0; reason < 3; ++reason) {
        Session session;
        if (reason == 0)
            session.repository.activeSlayers.clear();
        if (reason == 1)
            session.repository.addActiveSlayer("Rowan", "accountant", SLOT2);
        if (reason == 2)
            session.repository.addActiveSlayer("Rowan", "account", SLOT3);
        unsigned replies = 0;
        session.actions.sendRefusal = [&](LoginPlayer& player, LCDeletePCError& reply) {
            EXPECT_EQ(&player, &session.player);
            EXPECT_EQ(reply.getPacketID(), Packet::PACKET_LC_DELETE_PC_ERROR);
            SocketOutputStream output(nullptr, 64);
            reply.write(output);
            ASSERT_EQ(output.length(), 1u);
            EXPECT_EQ(static_cast<BYTE>(output.getBuffer()[0]), reason == 0 ? 11 : reason == 1 ? 0 : 12);
            expectOwned(session);
            ++replies;
        };
        EXPECT_FALSE(session.erase());
        EXPECT_EQ(replies, 1u);
        EXPECT_EQ(session.repository.loadActiveSlayerOwnerCalls, 1);
        EXPECT_EQ(session.repository.retireSlayerCalls, reason == 2 ? 1 : 0);
        EXPECT_EQ(session.repository.activeSlayers.size(), reason == 0 ? 0u : 1u);
        EXPECT_TRUE(session.repository.recordedDeletions.empty());
        EXPECT_TRUE(session.repository.purges.empty());
        EXPECT_TRUE(session.repository.destroyedItemOwners.empty());
        EXPECT_EQ(session.successes, 0u);
        expectOwned(session);
    }
}

TEST(LoginCharacterDeletion, EverySlotUsesTheOwnedIdentityAndWorldAndPublishesAfterTheSuccessReply) {
    for (WorldID_t world : {WorldID_t(7), WorldID_t(255)}) {
        for (int slot = SLOT1; slot < SLOT_MAX; ++slot) {
            Session session;
            session.player.setWorldID(world);
            session.packet.setSlot(static_cast<Slot>(slot));
            session.repository.addActiveSlayer("Rowan", "account", static_cast<Slot>(slot));
            session.repository.before = [&](Stage) { expectOwned(session); };
            session.actions.sendSuccess = [&](LoginPlayer& player, LCDeletePCOK& reply) {
                EXPECT_EQ(&player, &session.player);
                EXPECT_EQ(reply.getPacketID(), Packet::PACKET_LC_DELETE_PC_OK);
                SocketOutputStream output(nullptr, 64);
                reply.write(output);
                EXPECT_EQ(output.length(), 0u);
                EXPECT_EQ(session.repository.purges.size(), 1u);
                expectOwned(session);
                ++session.successes;
            };
            EXPECT_TRUE(session.erase());
            EXPECT_EQ(session.repository.calls,
                      (std::vector<Stage>{Stage::Owner, Stage::Retire, Stage::Record, Stage::Purge}));
            EXPECT_TRUE(session.repository.activeSlayers.empty());
            EXPECT_EQ(session.repository.ownerLookupWorldIDs, (std::vector<WorldID_t>{world}));
            ASSERT_EQ(session.repository.retirements.size(), 1u);
            EXPECT_EQ(session.repository.retirements.front().worldID, world);
            EXPECT_EQ(session.repository.retirements.front().name, "Rowan");
            EXPECT_EQ(session.repository.retirements.front().slot, slot);
            ASSERT_EQ(session.repository.recordedDeletions.size(), 1u);
            EXPECT_EQ(session.repository.recordedDeletions.front().worldID, world);
            EXPECT_EQ(session.repository.recordedDeletions.front().playerID, "account");
            EXPECT_EQ(session.repository.recordedDeletions.front().name, "Rowan");
            ASSERT_EQ(session.repository.purges.size(), 1u);
            EXPECT_EQ(session.repository.purges.front().worldID, world);
            EXPECT_EQ(session.repository.purges.front().name, "Rowan");
            EXPECT_EQ(session.repository.purges.front().slot, slot);
            EXPECT_TRUE(session.repository.destroyedItemOwners.empty());
            EXPECT_EQ(session.successes, 1u);
            EXPECT_TRUE(session.refusals.empty());
            EXPECT_EQ(session.player.getWorldID(), world);
            expectOwned(session, LPS_WAITING_FOR_CL_GET_PC_LIST);
        }
    }
}

TEST(LoginCharacterDeletion, RequestSnapshotPrecedesDiagnosticsAndRepositoryCallbacks) {
    for (bool atReport : {false, true}) {
        Session session;
        const auto mutate = [&] {
            session.packet.setName("replacement");
            session.packet.setSlot(SLOT3);
            session.packet.setSSN("replaced");
        };
        if (atReport)
            session.actions.diagnostics.request = [&](const CLDeletePC&) { mutate(); };
        else
            session.repository.before = [&](Stage stage) {
                if (stage == Stage::Owner)
                    mutate();
            };
        EXPECT_TRUE(session.erase());
        ASSERT_EQ(session.repository.retirements.size(), 1u);
        EXPECT_EQ(session.repository.retirements.front().name, "Rowan");
        EXPECT_EQ(session.repository.retirements.front().slot, SLOT2);
        EXPECT_EQ(session.repository.recordedDeletions.front().name, "Rowan");
        EXPECT_EQ(session.repository.purges.front().name, "Rowan");
        EXPECT_EQ(session.repository.purges.front().slot, SLOT2);
        EXPECT_EQ(session.packet.getName(), "replacement");
    }
}

constexpr Stage stages[] = {Stage::Owner, Stage::Retire, Stage::Record, Stage::Purge, Stage::Refusal, Stage::Success};

void installFailure(Session& session, Stage stage, std::exception_ptr failure) {
    if (stage == Stage::Refusal) {
        session.repository.activeSlayers.clear();
        session.actions.sendRefusal = [failure](LoginPlayer&, LCDeletePCError&) { std::rethrow_exception(failure); };
    } else if (stage == Stage::Success) {
        session.actions.sendSuccess = [failure](LoginPlayer&, LCDeletePCOK&) { std::rethrow_exception(failure); };
    } else {
        session.repository.before = [stage, failure](Stage current) {
            if (stage == current)
                std::rethrow_exception(failure);
        };
    }
}

void expectPartialEffects(const Session& session, Stage stage) {
    const bool retired = stage == Stage::Record || stage == Stage::Purge || stage == Stage::Success;
    EXPECT_EQ(session.repository.activeSlayers.size(), retired || stage == Stage::Refusal ? 0u : 1u);
    EXPECT_EQ(session.repository.recordedDeletions.size(), stage == Stage::Purge || stage == Stage::Success ? 1u : 0u);
    EXPECT_EQ(session.repository.purges.size(), stage == Stage::Success ? 1u : 0u);
    EXPECT_EQ(session.successes, 0u);
    expectOwned(session);
}

TEST(LoginCharacterDeletion, DatabaseFailuresRetainPartialEffectsAndSendTheDefaultCode) {
    for (auto stage : stages) {
        if (stage == Stage::Refusal)
            continue;
        SCOPED_TRACE(static_cast<int>(stage));
        Session session;
        installFailure(session, stage, std::make_exception_ptr(DatabaseError("deletion query failed")));
        std::vector<std::string> reports;
        session.actions.diagnostics.databaseFailure = [&](const std::string& message) { reports.push_back(message); };
        EXPECT_FALSE(session.erase());
        EXPECT_EQ(session.refusals, (std::vector<BYTE>{0}));
        EXPECT_EQ(reports, (std::vector<std::string>{"deletion query failed"}));
        expectPartialEffects(session, stage);
    }
}

TEST(LoginCharacterDeletion, OtherFailuresKeepTheirIdentityAndPartialEffectsAtEveryStage) {
    const std::exception_ptr failures[] = {
        std::make_exception_ptr(NoSuchElementException("row missing")),
        std::make_exception_ptr(Error("deletion failed")), std::make_exception_ptr(DisconnectException("peer left")),
        std::make_exception_ptr(std::runtime_error("callback failed")), std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        for (auto stage : stages) {
            SCOPED_TRACE(static_cast<int>(stage));
            Session session;
            installFailure(session, stage, failure);
            try {
                (void)session.erase();
                FAIL() << "operation failure was swallowed";
            } catch (...) {
                EXPECT_EQ(std::current_exception(), failure);
            }
            EXPECT_TRUE(session.refusals.empty());
            expectPartialEffects(session, stage);
        }
    }
}

TEST(LoginCharacterDeletion, ADatabaseFailureSendingARefusalRetriesTheSelectedByteOnce) {
    for (int reason = 0; reason < 3; ++reason) {
        Session session;
        if (reason == 0)
            session.repository.activeSlayers.clear();
        if (reason == 1)
            session.repository.addActiveSlayer("Rowan", "other", SLOT2);
        if (reason == 2)
            session.repository.addActiveSlayer("Rowan", "account", SLOT3);
        const BYTE expected = reason == 0 ? 11 : reason == 1 ? 0 : 12;
        session.actions.sendRefusal = [&](LoginPlayer&, LCDeletePCError& reply) {
            session.refusals.push_back(reply.getErrorID());
            if (session.refusals.size() == 1)
                throw DatabaseError("first reply failed");
        };
        EXPECT_FALSE(session.erase());
        EXPECT_EQ(session.refusals, (std::vector<BYTE>{expected, expected}));
        EXPECT_TRUE(session.repository.recordedDeletions.empty());
        EXPECT_TRUE(session.repository.purges.empty());
        expectOwned(session);
    }
}

TEST(LoginCharacterDeletion, FailureSendingTheDatabaseErrorReplyPropagatesWithoutAnotherAttempt) {
    const std::exception_ptr failures[] = {std::make_exception_ptr(DatabaseError("reply failed")),
                                           std::make_exception_ptr(DisconnectException("peer left")),
                                           std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        Session session;
        installFailure(session, Stage::Purge, std::make_exception_ptr(DatabaseError("purge failed")));
        unsigned replies = 0;
        session.actions.sendRefusal = [&](LoginPlayer&, LCDeletePCError& reply) {
            EXPECT_EQ(reply.getErrorID(), 0);
            ++replies;
            std::rethrow_exception(failure);
        };
        try {
            (void)session.erase();
            FAIL() << "reply failure was swallowed";
        } catch (...) {
            EXPECT_EQ(std::current_exception(), failure);
        }
        EXPECT_EQ(replies, 1u);
        expectPartialEffects(session, Stage::Purge);
    }
}

TEST(LoginCharacterDeletion, AFailureInsidePersistenceDoesNotUndoItsCompletedEffects) {
    for (auto stage : {Stage::Retire, Stage::Record, Stage::Purge}) {
        Session session;
        session.repository.after = [stage](Stage current) {
            if (stage == current)
                throw DatabaseError("write partly applied");
        };
        EXPECT_FALSE(session.erase());
        EXPECT_TRUE(session.repository.activeSlayers.empty());
        EXPECT_EQ(session.repository.recordedDeletions.size(), stage == Stage::Retire ? 0u : 1u);
        EXPECT_EQ(session.repository.purges.size(), stage == Stage::Purge ? 1u : 0u);
        EXPECT_EQ(session.refusals, (std::vector<BYTE>{0}));
        EXPECT_EQ(session.successes, 0u);
        expectOwned(session);
    }
}

TEST(LoginCharacterDeletion, RetryCanRepeatAFailedLookupButCannotResumeAnAlreadyRetiredCharacter) {
    for (auto stage : {Stage::Owner, Stage::Record, Stage::Purge, Stage::Success}) {
        Session session;
        installFailure(session, stage, std::make_exception_ptr(DatabaseError("failed step")));
        EXPECT_FALSE(session.erase());
        session.repository.before = {};
        session.actions.sendSuccess = [&](LoginPlayer&, LCDeletePCOK&) { ++session.successes; };
        EXPECT_EQ(session.erase(), stage == Stage::Owner);
        if (stage == Stage::Owner) {
            EXPECT_EQ(session.successes, 1u);
            EXPECT_EQ(session.repository.purges.size(), 1u);
            expectOwned(session, LPS_WAITING_FOR_CL_GET_PC_LIST);
        } else {
            EXPECT_EQ(session.refusals, (std::vector<BYTE>{0, 11}));
            expectPartialEffects(session, stage);
        }
    }
}

TEST(LoginCharacterDeletion, DiagnosticFailuresAtEverySiteCannotReplaceTheOperationOutcome) {
    const std::exception_ptr failures[] = {std::make_exception_ptr(DatabaseError("report database failure")),
                                           std::make_exception_ptr(NoSuchElementException("report target missing")),
                                           std::make_exception_ptr(Error("report failed")),
                                           std::make_exception_ptr(std::runtime_error("output failed")),
                                           std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        for (int site = 0; site < 4; ++site) {
            Session session;
            unsigned reports = 0;
            const auto fail = [&] {
                ++reports;
                std::rethrow_exception(failure);
            };
            if (site == 0)
                session.actions.diagnostics.request = [&](const CLDeletePC&) { fail(); };
            if (site == 1) {
                session.repository.addActiveSlayer("Rowan", "other", SLOT2);
                session.actions.diagnostics.wrongOwner = [&](const DeletePCRequest&) { fail(); };
            }
            if (site == 2) {
                session.repository.activeSlayers.clear();
                session.actions.diagnostics.refusal = [&](DeletePCRejection) { fail(); };
            }
            if (site == 3) {
                installFailure(session, Stage::Owner, std::make_exception_ptr(DatabaseError("actual lookup failure")));
                session.actions.diagnostics.databaseFailure = [&](const std::string& message) {
                    EXPECT_EQ(message, "actual lookup failure");
                    fail();
                };
            }
            EXPECT_EQ(session.erase(), site == 0);
            EXPECT_EQ(reports, 1u);
            if (site == 0) {
                EXPECT_EQ(session.successes, 1u);
                expectOwned(session, LPS_WAITING_FOR_CL_GET_PC_LIST);
            } else {
                EXPECT_EQ(session.refusals, (std::vector<BYTE>{BYTE(site == 2 ? 11 : 0)}));
                expectOwned(session);
            }
        }
    }
}

TEST(LoginCharacterDeletion, DiagnosticsReceiveOwnedInputsAtTheRelevantStages) {
    Session session;
    session.repository.addActiveSlayer("Rowan", "other", SLOT2);
    std::vector<std::string> effects;
    session.actions.diagnostics.request = [&](const CLDeletePC& packet) {
        EXPECT_EQ(packet.getName(), "Rowan");
        EXPECT_TRUE(session.repository.calls.empty());
        effects.push_back("request");
        session.packet.setName("replacement");
    };
    session.repository.before = [&](Stage stage) {
        EXPECT_EQ(stage, Stage::Owner);
        effects.push_back("owner");
    };
    session.actions.diagnostics.wrongOwner = [&](const DeletePCRequest& request) {
        EXPECT_EQ(request.worldID, 7);
        EXPECT_EQ(request.playerID, "account");
        EXPECT_EQ(request.name, "Rowan");
        EXPECT_EQ(request.slot, SLOT2);
        effects.push_back("audit");
    };
    session.actions.diagnostics.refusal = [&](DeletePCRejection rejection) {
        EXPECT_EQ(rejection, DeletePCRejection::NotTheOwner);
        effects.push_back("refusal report");
    };
    session.actions.sendRefusal = [&](LoginPlayer&, LCDeletePCError& reply) {
        EXPECT_EQ(reply.getErrorID(), 0);
        effects.push_back("send");
    };
    EXPECT_FALSE(session.erase());
    EXPECT_EQ(effects, (std::vector<std::string>{"request", "owner", "audit", "refusal report", "send"}));
}

class DeletionLogs {
public:
    DeletionLogs() {
        if (!::mkdtemp(directory) || ::chdir(directory) != 0)
            std::_Exit(80);
    }
    ~DeletionLogs() {
        ::unlink("DeletePC.log");
        if (::chdir("/") != 0 || ::rmdir(directory) != 0)
            std::_Exit(81);
    }

private:
    char directory[40] = "/tmp/darkeden-deletion-XXXXXX";
};

int checkProductionActions(int result) {
    DeletionLogs logs;
    Session session;
    if (result == 1)
        session.repository.activeSlayers.clear();
    if (result == 2)
        session.repository.addActiveSlayer("Rowan", "other", SLOT2);
    if (result == 3)
        session.repository.addActiveSlayer("Rowan", "account", SLOT3);
    if (result == 4)
        installFailure(session, Stage::Owner, std::make_exception_ptr(DatabaseError("synthetic database failure")));
    session.actions = de::defaultLoginCharacterDeletionActions();
    std::ostringstream output;
    ConsoleBuffer console(*output.rdbuf());
    std::string reported;
    session.player.observe = [&](Packet&) { reported = output.str(); };
    if (session.erase() != (result == 0) || session.player.sent != 1)
        return 1;
    std::string expected = "CLDeletePC(Name:Rowan,Slot:1,SSN:test-ssn)\n";
    if (result == 1 || result == 3)
        expected += "Fail to deletePC : no such slayer exist.\n";
    if (result == 2)
        expected += "Fail to deletePC : illegal pc delete\n";
    if (result == 4)
        expected += "Fail to deletePC : synthetic database failure\n";
    if (reported != expected) {
        std::cerr << "Actual diagnostics: [" << reported << "] expected: [" << expected << "]" << std::endl;
        return 2;
    }
    const auto bytes = session.player.bufferedBytes();
    if (bytes.size() != szPacketHeader + (result == 0 ? 0 : 1))
        return 3;
    if (result != 0 && static_cast<BYTE>(bytes[szPacketHeader]) != (result == 1 ? 11 : result == 3 ? 12 : 0))
        return 4;
    if (session.player.getPlayerStatus() != (result == 0 ? LPS_WAITING_FOR_CL_GET_PC_LIST : LPS_PC_MANAGEMENT) ||
        !session.player.loginAccountOwnership().owns("account"))
        return 5;
    if (result == 2) {
        std::ifstream log("DeletePC.log");
        std::string line;
        if (!std::getline(log, line) || !line.ends_with(" : Illegal PC Delete : [account:Rowan]") ||
            std::getline(log, line))
            return 6;
    } else if (::access("DeletePC.log", F_OK) == 0) {
        return 7;
    }
    return 0;
}

TEST(LoginCharacterDeletion, ProductionActionsPreserveReplyBytesConsoleTextAndTheWrongOwnerAudit) {
    for (int result = 0; result < 5; ++result) {
        SCOPED_TRACE(result);
        ASSERT_EXIT(std::_Exit(checkProductionActions(result)), ::testing::ExitedWithCode(0), "");
    }
}

int checkAuditAllocation(std::size_t position) {
    DeletionLogs logs;
    Session session;
    session.repository.addActiveSlayer("Rowan", "other", SLOT2);
    const auto& production = de::defaultLoginCharacterDeletionActions();
    session.actions.diagnostics = production.diagnostics;
    bool injected = false;
    session.actions.diagnostics.wrongOwner = [&](const DeletePCRequest& request) {
        AllocationProbe probe(position);
        try {
            production.diagnostics.wrongOwner(request);
        } catch (...) {
            injected = probe.rejected();
            throw;
        }
        injected = probe.rejected();
    };
    std::ostringstream output;
    ConsoleBuffer console(*output.rdbuf());
    if (session.erase() || session.refusals != std::vector<BYTE>{0} || session.repository.retireSlayerCalls != 0 ||
        !output.str().ends_with("Fail to deletePC : illegal pc delete\n"))
        return 1;
    return position == 1 && !injected ? 2 : 0;
}

TEST(LoginCharacterDeletion, ProductionAuditAllocationFailureCannotSkipTheConsoleReportOrRefusal) {
    for (std::size_t position = 1; position <= 16; ++position) {
        SCOPED_TRACE(position);
        ASSERT_EXIT(std::_Exit(checkAuditAllocation(position)), ::testing::ExitedWithCode(0), "");
    }
}

int checkAllocationFailure(std::size_t position, int mode) {
    auto session = std::make_unique<Session>();
    const std::string account(64, 'a');
    const std::string name(20, 'n');
    session->player.setID(account);
    session->packet.setName(name);
    session->repository.activeSlayers.clear();
    session->repository.addActiveSlayer(name, account, SLOT2);
    if (mode == 1)
        session->repository.activeSlayers.clear();
    if (mode == 2)
        session->repository.addActiveSlayer(name, "other", SLOT2);
    if (mode == 3)
        session->repository.addActiveSlayer(name, account, SLOT3);
    if (mode == 4)
        installFailure(*session, Stage::Owner, std::make_exception_ptr(DatabaseError("lookup failed")));
    if (mode == 5)
        installFailure(*session, Stage::Record, std::make_exception_ptr(DatabaseError("record failed")));
    if (mode == 6)
        session->repository.after = [](Stage stage) {
            if (stage == Stage::Purge)
                throw DatabaseError("partial purge");
        };
    session->actions.sendSuccess = [&](LoginPlayer&, LCDeletePCOK& reply) {
        SocketOutputStream output(nullptr, 64);
        reply.write(output);
        ++session->successes;
    };
    session->actions.sendRefusal = [&](LoginPlayer&, LCDeletePCError& reply) {
        SocketOutputStream output(nullptr, 64);
        reply.write(output);
        session->refusals.push_back(reply.getErrorID());
    };
    AllocationProbe probe(position);
    bool failed = false;
    bool deleted = false;
    try {
        deleted = session->erase();
    } catch (const std::bad_alloc&) {
        failed = true;
    } catch (...) {
        return 1;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (position == 64 && failed) || (!failed && deleted != (mode == 0)))
        return 2;
    if (!session->player.loginAccountOwnership().owns("account"))
        return 3;
    if (session->successes) {
        if (failed || mode != 0 || session->player.getPlayerStatus() != LPS_WAITING_FOR_CL_GET_PC_LIST ||
            session->repository.purges.size() != 1 || !session->repository.activeSlayers.empty())
            return 4;
    } else if (session->player.getPlayerStatus() != LPS_PC_MANAGEMENT) {
        return 5;
    }
    if (failed && mode == 0) {
        const bool canRetry = !session->repository.activeSlayers.empty();
        if (session->erase() != canRetry || (canRetry && session->successes != 1) ||
            (!canRetry && (session->refusals.empty() || session->refusals.back() != 11)))
            return 6;
    }
    session.reset();
    return probe.outstanding() == 0 ? 0 : 7;
}

TEST(LoginCharacterDeletion, AllocationFailuresRetainOwnershipAndPartialEffectsWithoutLeaking) {
    for (int mode = 0; mode < 7; ++mode) {
        for (std::size_t position = 1; position <= 64; ++position) {
            SCOPED_TRACE(::testing::Message() << mode << "/" << position);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure(position, mode)), ::testing::ExitedWithCode(0), "");
        }
    }
}

} // namespace
