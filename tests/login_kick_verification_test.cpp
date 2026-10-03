#include <array>
#include <cstdlib>
#include <exception>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "GLKickVerify.h"
#include "LoginConnection.h"
#include "LoginContext.h"
#include "LoginKickVerification.h"
#include "LoginPlayerManager.h"
#include "Socket.h"
#include "support/AllocationProbe.h"

namespace {

struct Session {
    Session() {
        auto incoming = de::makeLoginConnection(std::make_unique<Socket>());
        player = incoming.get();
        player->setID("account");
        player->cacheLoginKickTarget({7, 9, 3, "Rowan"});
        player->setPlayerStatus(LPS_WAITING_FOR_GL_KICK_VERIFY);
        packet.setID(player->getSocket()->getSOCKET());
        packet.setPCName("Rowan");
        packet.setKicked();
        players.addPlayer(player);
        incoming.release();
    }
    de::LoginContext context;
    LoginPlayerManager players{context};
    LoginPlayer* player = nullptr;
    GLKickVerify packet;
};

TEST(LoginKickVerification, EveryNonWaitingStateRefusesAMatchingCharacter) {
    for (int value = 0; value < PLAYER_STATUS_MAX; ++value) {
        const auto status = static_cast<PlayerStatus>(value);
        if (status == LPS_WAITING_FOR_GL_KICK_VERIFY)
            continue;
        Session session;
        session.player->setPlayerStatus(status);
        unsigned completions = 0;

        EXPECT_FALSE(de::verifyLoginKick(session.players, session.packet, [&](LoginPlayer&) { ++completions; }));
        EXPECT_EQ(completions, 0u);
        EXPECT_EQ(session.player->getPlayerStatus(), status);
    }
}

int checkStandardFailureUnlock() {
    Session session;
    const auto failure = std::make_exception_ptr(std::runtime_error("complete failed"));
    std::exception_ptr caught;
    try {
        (void)de::verifyLoginKick(session.players, session.packet,
                                  [&](LoginPlayer&) { std::rethrow_exception(failure); });
    } catch (...) {
        caught = std::current_exception();
    }
    if (caught != failure)
        return 1;
    try {
        session.players.lock();
        session.players.unlock();
    } catch (...) {
        return 2;
    }
    return 0;
}

TEST(LoginKickVerification, AStandardCompletionExceptionCannotLeaveTheManagerLocked) {
    ASSERT_EXIT(std::_Exit(checkStandardFailureUnlock()), ::testing::ExitedWithCode(0), "");
}

int checkFailedLockOwnership() {
    Session session;
    session.players.lock();
    bool failed = false;
    try {
        (void)de::verifyLoginKick(session.players, session.packet, [](LoginPlayer&) {});
    } catch (const Error&) {
        failed = true;
    }
    bool stillHeld = false;
    try {
        session.players.lock();
    } catch (const Error&) {
        stillHeld = true;
    }
    session.players.unlock();
    return failed && stillHeld ? 0 : 1;
}

TEST(LoginKickVerification, FailedLockAcquisitionDoesNotReleaseTheCallersExistingLock) {
    ASSERT_EXIT(std::_Exit(checkFailedLockOwnership()), ::testing::ExitedWithCode(0), "");
}

TEST(LoginKickVerification, CompletionFailuresRetainTheirIdentityForTheBoundaryHandler) {
    Session session;
    const auto failure = std::make_exception_ptr(Error("complete failed"));
    std::exception_ptr caught;
    try {
        (void)de::verifyLoginKick(session.players, session.packet,
                                  [&](LoginPlayer&) { std::rethrow_exception(failure); });
    } catch (...) {
        caught = std::current_exception();
    }
    EXPECT_EQ(caught, failure);
}

TEST(LoginKickVerification, AWaitingMatchCompletesTheRegisteredPlayerUnderItsManagerLock) {
    Session session;
    unsigned completions = 0;
    const auto* target = session.player->getLoginKickTarget();
    EXPECT_TRUE(de::verifyLoginKick(session.players, session.packet, [&](LoginPlayer& player) {
        EXPECT_EQ(&player, session.player);
        EXPECT_EQ(player.getID(), "account");
        EXPECT_EQ(player.getLoginKickTarget(), target);
        EXPECT_EQ(player.getPlayerStatus(), LPS_WAITING_FOR_GL_KICK_VERIFY);
        // Native error-checking mutexes reject same-thread relocking.
        EXPECT_THROW(session.players.lock(), Error);
        player.setPlayerStatus(LPS_WAITING_FOR_CL_GET_PC_LIST);
        ++completions;
    }));
    EXPECT_EQ(completions, 1u);
    EXPECT_EQ(session.player->getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
    EXPECT_NO_THROW({
        session.players.lock();
        session.players.unlock();
    });
}

TEST(LoginKickVerification, AnAlreadyAbsentCharacterAndACompletedKickAreBothValidReplies) {
    for (const bool kicked : {false, true}) {
        Session session;
        session.packet.setKicked(kicked);
        unsigned completions = 0;
        EXPECT_TRUE(de::verifyLoginKick(session.players, session.packet, [&](LoginPlayer&) { ++completions; }));
        EXPECT_EQ(completions, 1u);
    }
}

TEST(LoginKickVerification, DuplicateRepliesAfterCompletionCannotAdvanceTheSessionAgain) {
    Session session;
    unsigned completions = 0;
    const de::LoginKickCompletion complete = [&](LoginPlayer& player) {
        ++completions;
        player.setPlayerStatus(LPS_WAITING_FOR_CL_GET_PC_LIST);
    };
    EXPECT_TRUE(de::verifyLoginKick(session.players, session.packet, complete));
    EXPECT_FALSE(de::verifyLoginKick(session.players, session.packet, complete));
    EXPECT_FALSE(de::verifyLoginKick(session.players, session.packet, complete));
    EXPECT_EQ(completions, 1u);
}

TEST(LoginKickVerification, CharacterNamesMustMatchEveryByte) {
    for (const std::string& name : {std::string(""), std::string("rowan"), std::string("Rowan "),
                                    std::string("Rowan\0extra", 11), std::string(96, 'r')}) {
        Session session;
        session.packet.setPCName(name);
        EXPECT_FALSE(de::verifyLoginKick(session.players, session.packet, {}));
        EXPECT_EQ(session.player->getPlayerStatus(), LPS_WAITING_FOR_GL_KICK_VERIFY);
        EXPECT_EQ(session.player->getID(), "account");
    }
}

TEST(LoginKickVerification, AChangedAccountCannotVerifyThePreviousAccountsTarget) {
    Session session;
    static_cast<Player&>(*session.player).setID("another account");
    EXPECT_FALSE(de::verifyLoginKick(session.players, session.packet, {}));
    EXPECT_EQ(session.player->getID(), "another account");
    EXPECT_EQ(session.player->getPlayerStatus(), LPS_WAITING_FOR_GL_KICK_VERIFY);
}

TEST(LoginKickVerification, MissingOrUnnamedTargetsNeverReachCompletion) {
    for (const bool missing : {false, true}) {
        Session session;
        if (missing)
            session.player->clearLoginKickTarget();
        else
            session.player->cacheLoginKickTarget({7, 9, 3, {}});
        EXPECT_FALSE(de::verifyLoginKick(session.players, session.packet, {}));
    }
}

TEST(LoginKickVerification, EmptyAndSuppressedAccountIdentitiesCannotComplete) {
    for (const std::string id : {"", "NONE"}) {
        Session session;
        session.player->setID(id);
        session.player->cacheLoginKickTarget({7, 9, 3, "Rowan"});
        EXPECT_FALSE(de::verifyLoginKick(session.players, session.packet, {}));
        EXPECT_EQ(session.player->getID(), id);
    }
}

TEST(LoginKickVerification, MissingAndOutOfRangeDescriptorsAreIgnoredWithoutNarrowing) {
    de::LoginContext context;
    LoginPlayerManager players(context);
    GLKickVerify packet;
    packet.setPCName("Rowan");
    for (const uint descriptor :
         {0u, PlayerManager::nMaxPlayers - 1, PlayerManager::nMaxPlayers,
          static_cast<uint>(std::numeric_limits<int>::max()), std::numeric_limits<uint>::max()}) {
        packet.setID(descriptor);
        EXPECT_FALSE(de::verifyLoginKick(players, packet, {}));
        EXPECT_NO_THROW({
            players.lock();
            players.unlock();
        });
    }
}

TEST(LoginKickVerification, OnlyTheSuppliedManagerCanResolveTheDescriptor) {
    Session first;
    Session second;
    EXPECT_NE(first.packet.getID(), second.packet.getID());
    EXPECT_FALSE(de::verifyLoginKick(first.players, second.packet, {}));
    EXPECT_FALSE(de::verifyLoginKick(second.players, first.packet, {}));
}

TEST(LoginKickVerification, EveryCompletionExceptionReleasesTheLockAndAllowsRetry) {
    const std::array failures{
        std::make_exception_ptr(std::runtime_error("complete failed")),
        std::make_exception_ptr(DatabaseError("complete failed")), std::make_exception_ptr(Error("complete failed")),
        std::make_exception_ptr(NoSuchElementException("complete failed")), std::make_exception_ptr(std::bad_alloc{})};
    for (const auto& failure : failures) {
        Session session;
        const auto* target = session.player->getLoginKickTarget();
        std::exception_ptr caught;
        try {
            (void)de::verifyLoginKick(session.players, session.packet,
                                      [&](LoginPlayer&) { std::rethrow_exception(failure); });
        } catch (...) {
            caught = std::current_exception();
        }
        EXPECT_EQ(caught, failure);
        EXPECT_EQ(session.player->getID(), "account");
        EXPECT_EQ(session.player->getLoginKickTarget(), target);
        EXPECT_EQ(session.player->getPlayerStatus(), LPS_WAITING_FOR_GL_KICK_VERIFY);
        unsigned completions = 0;
        EXPECT_TRUE(de::verifyLoginKick(session.players, session.packet, [&](LoginPlayer&) { ++completions; }));
        EXPECT_EQ(completions, 1u);
    }
}

TEST(LoginKickVerification, CompletionOwnsItsChangesEvenWhenItThrows) {
    Session session;
    const auto failure = std::make_exception_ptr(std::runtime_error("after publication"));
    EXPECT_THROW((void)de::verifyLoginKick(session.players, session.packet,
                                           [&](LoginPlayer& player) {
                                               player.setPlayerStatus(LPS_PC_MANAGEMENT);
                                               std::rethrow_exception(failure);
                                           }),
                 std::runtime_error);
    EXPECT_EQ(session.player->getPlayerStatus(), LPS_PC_MANAGEMENT);
    EXPECT_FALSE(de::verifyLoginKick(session.players, session.packet, {}));
}

int checkAllocationFailure(std::size_t failAt, int mode) {
    Session session;
    const std::string name(96, 'c');
    session.player->cacheLoginKickTarget({7, 9, 3, name});
    session.packet.setPCName(name);
    if (mode == 1)
        session.packet.setPCName(std::string(96, 'd'));
    if (mode == 2)
        session.packet.setID(session.packet.getID() == 0 ? 1 : 0);
    unsigned completions = 0;
    const de::LoginKickCompletion complete = [&](LoginPlayer&) {
        const auto storage = std::make_unique<char[]>(96);
        storage[0] = 'x';
        ++completions;
    };
    const auto* previous = session.player->getLoginKickTarget();
    AllocationProbe probe(failAt);
    bool failed = false;
    bool verified = false;
    try {
        verified = de::verifyLoginKick(session.players, session.packet, complete);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 64 && failed) || probe.outstanding() != 0)
        return 1;
    if (verified != (!failed && mode == 0) || completions != (verified ? 1u : 0u) ||
        session.player->getLoginKickTarget() != previous || session.player->getID() != "account" ||
        session.player->getPlayerStatus() != LPS_WAITING_FOR_GL_KICK_VERIFY)
        return 2;
    try {
        session.players.lock();
        session.players.unlock();
    } catch (...) {
        return 3;
    }
    session.packet.setID(session.player->getSocket()->getSOCKET());
    session.packet.setPCName(name);
    completions = 0;
    if (!de::verifyLoginKick(session.players, session.packet, complete) || completions != 1)
        return 4;
    return probe.outstanding() == 0 ? 0 : 5;
}

TEST(LoginKickVerification, AllocationFailuresReleaseTheLockAndTemporaryStorageBeforeRetry) {
    for (int mode = 0; mode < 3; ++mode) {
        for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
            SCOPED_TRACE(::testing::Message() << mode << "/" << failAt);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure(failAt, mode)), ::testing::ExitedWithCode(0), "");
        }
    }
}

} // namespace
