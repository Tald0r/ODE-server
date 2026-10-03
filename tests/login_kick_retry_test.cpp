#include <array>
#include <cstdlib>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "GameServerInfoManager.h"
#include "LoginKickDispatch.h"
#include "LoginKickPreparation.h"
#include "LoginKickRetry.h"
#include "LoginPlayer.h"
#include "ServerContext.h"
#include "Socket.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginAccountRepository.h"
#include "support/FakeLoginCharacterRepository.h"

namespace {

class RetryPlayer : public LoginPlayer {
public:
    RetryPlayer() : LoginPlayer(new Socket()) {
        setID("account");
        cacheLoginKickTarget({7, 9, 3, "Rowan"});
        setPlayerStatus(LPS_BEGIN_SESSION);
    }
    ~RetryPlayer() override {
        setPlayerStatus(LPS_END_SESSION);
    }
};

struct Attempt {
    Attempt() {
        de::beginLoginKick(player, send);
    }
    bool tick() {
        now = player.getExpireTimeForKickCharacter();
        return de::retryLoginKick(player, now, send, complete);
    }
    RetryPlayer player;
    Timeval now{100, 123456};
    unsigned sends = 0;
    unsigned completions = 0;
    de::LoginKickRequest send = [&](LoginPlayer& current) {
        ++sends;
        current.setExpireTimeForKickCharacter(now);
        current.setPlayerStatus(LPS_WAITING_FOR_GL_KICK_VERIFY);
        return true;
    };
    std::function<void(LoginPlayer&)> complete = [&](LoginPlayer&) { ++completions; };
};

TEST(LoginKickRetry, ARefusedResendCannotAdvanceTheCountOrCompleteLogin) {
    Attempt attempt;
    ASSERT_TRUE(attempt.tick());
    ASSERT_TRUE(attempt.tick());
    EXPECT_FALSE(de::retryLoginKick(
        attempt.player, attempt.player.getExpireTimeForKickCharacter(), [](LoginPlayer&) { return false; },
        attempt.complete));
    EXPECT_EQ(attempt.player.getKickCharacterCount(), 2);
    EXPECT_EQ(attempt.completions, 0u);
}

TEST(LoginKickRetry, MissingDestinationsCannotCompleteLoginWithASuppressedIdentity) {
    Attempt attempt;
    ASSERT_TRUE(attempt.tick());
    ASSERT_TRUE(attempt.tick());
    GameServerInfoManager servers;
    EXPECT_FALSE(de::retryLoginKick(
        attempt.player, attempt.player.getExpireTimeForKickCharacter(),
        [&](LoginPlayer& player) { return de::dispatchLoginKick(player, *player.getLoginKickTarget(), servers, {}); },
        attempt.complete));
    EXPECT_EQ(attempt.player.getID(), "NONE");
    EXPECT_EQ(attempt.player.getKickCharacterCount(), 2);
    EXPECT_EQ(attempt.completions, 0u);
}

TEST(LoginKickRetry, AFreshAttemptOnTheSameSocketStartsWithZeroRetries) {
    Attempt attempt;
    ASSERT_TRUE(attempt.tick());
    ASSERT_TRUE(attempt.tick());
    attempt.player.setPlayerStatus(LPS_BEGIN_SESSION);
    ASSERT_TRUE(de::beginLoginKick(attempt.player, attempt.send));
    EXPECT_EQ(attempt.player.getKickCharacterCount(), 0);
    ASSERT_TRUE(attempt.tick());
    EXPECT_EQ(attempt.player.getKickCharacterCount(), 1);
    EXPECT_EQ(attempt.completions, 0u);
}

TEST(LoginKickRetry, ASuppressedWaitingSessionDoesNotCallEitherAction) {
    Attempt attempt;
    attempt.player.setID("NONE");
    EXPECT_FALSE(attempt.tick());
    EXPECT_EQ(attempt.sends, 1u);
    EXPECT_EQ(attempt.player.getKickCharacterCount(), 0);
}

TEST(LoginKickRetry, RepeatedCompletionFailuresKeepTheRetryCountAtItsLimit) {
    Attempt attempt;
    ASSERT_TRUE(attempt.tick());
    ASSERT_TRUE(attempt.tick());
    attempt.complete = [&](LoginPlayer&) { throw std::runtime_error("completion failed"); };
    for (unsigned i = 0; i < 5; ++i) {
        EXPECT_THROW(attempt.tick(), std::runtime_error);
        EXPECT_EQ(attempt.player.getKickCharacterCount(), 3);
    }
}

TEST(LoginKickRetry, TheDeadlineIsInclusiveAndPreservesMicroseconds) {
    for (const Timeval due : {Timeval{103, 123456}, Timeval{103, 123457}, Timeval{999, 0}}) {
        Attempt attempt;
        EXPECT_EQ(attempt.player.getExpireTimeForKickCharacter(), (Timeval{103, 123456}));
        for (const Timeval early : {Timeval{0, 0}, Timeval{102, 999999}, Timeval{103, 123455}}) {
            EXPECT_FALSE(de::retryLoginKick(attempt.player, early, {}, {}));
            EXPECT_EQ(attempt.player.getKickCharacterCount(), 0);
        }
        attempt.now = due;
        EXPECT_TRUE(de::retryLoginKick(attempt.player, due, attempt.send, attempt.complete));
        EXPECT_EQ(attempt.player.getExpireTimeForKickCharacter(), (Timeval{due.tv_sec + 3, due.tv_usec}));
        EXPECT_EQ(attempt.sends, 2u);
        EXPECT_EQ(attempt.player.getKickCharacterCount(), 1);
        EXPECT_EQ(attempt.completions, 0u);
    }
}

TEST(LoginKickRetry, EveryNonWaitingPhaseSkipsActionsEvenAfterTheDeadline) {
    for (int value = 0; value < PLAYER_STATUS_MAX; ++value) {
        const auto status = static_cast<PlayerStatus>(value);
        if (status == LPS_WAITING_FOR_GL_KICK_VERIFY)
            continue;
        Attempt attempt;
        attempt.player.setPlayerStatus(status);
        EXPECT_FALSE(de::retryLoginKick(attempt.player, Timeval{999, 0}, {}, {}));
        EXPECT_EQ(attempt.player.getPlayerStatus(), status);
        EXPECT_EQ(attempt.player.getKickCharacterCount(), 0);
        EXPECT_EQ(attempt.player.getExpireTimeForKickCharacter(), (Timeval{103, 123456}));
    }
}

TEST(LoginKickRetry, MissingOrStaleAccountTargetsSkipBothActions) {
    for (int mode = 0; mode < 5; ++mode) {
        SCOPED_TRACE(mode);
        Attempt attempt;
        if (mode < 3)
            static_cast<Player&>(attempt.player).setID(std::array{"", "NONE", "another account"}[mode]);
        else if (mode == 3)
            attempt.player.clearLoginKickTarget();
        else
            attempt.player.cacheLoginKickTarget({7, 9, 3, ""});
        EXPECT_FALSE(de::retryLoginKick(attempt.player, Timeval{999, 0}, {}, {}));
        EXPECT_EQ(attempt.player.getKickCharacterCount(), 0);
    }
}

TEST(LoginKickRetry, CompletionFollowsTheThirdResendAndDuplicatesStopAfterItChangesPhase) {
    Attempt attempt;
    attempt.complete = [&](LoginPlayer& player) {
        EXPECT_EQ(attempt.sends, 4u);
        EXPECT_EQ(player.getKickCharacterCount(), 3);
        EXPECT_EQ(player.getExpireTimeForKickCharacter(), (Timeval{112, 123456}));
        EXPECT_EQ(player.getID(), "account");
        EXPECT_EQ(player.getLoginKickTarget()->characterName, "Rowan");
        EXPECT_EQ(player.getPlayerStatus(), LPS_WAITING_FOR_GL_KICK_VERIFY);
        player.setPlayerStatus(LPS_WAITING_FOR_CL_GET_PC_LIST);
        ++attempt.completions;
    };
    for (int count = 1; count <= 3; ++count) {
        ASSERT_TRUE(attempt.tick());
        EXPECT_EQ(attempt.player.getKickCharacterCount(), count);
        EXPECT_EQ(attempt.completions, count == 3 ? 1u : 0u);
    }
    EXPECT_FALSE(de::retryLoginKick(attempt.player, Timeval{999, 0}, {}, {}));
}

TEST(LoginKickRetry, SuccessfulResendsCannotCountForAChangedPhaseIdentityOrTarget) {
    for (int mode = 0; mode < 8; ++mode) {
        SCOPED_TRACE(mode);
        Attempt attempt;
        ASSERT_TRUE(attempt.tick());
        ASSERT_TRUE(attempt.tick());
        auto target = *attempt.player.getLoginKickTarget();
        EXPECT_FALSE(de::retryLoginKick(
            attempt.player, attempt.player.getExpireTimeForKickCharacter(),
            [&](LoginPlayer& player) {
                switch (mode) {
                case 0:
                    player.setPlayerStatus(LPS_BEGIN_SESSION);
                    break;
                case 1:
                    player.setID("NONE");
                    break;
                case 2:
                    static_cast<Player&>(player).setID("another account");
                    player.cacheLoginKickTarget(target);
                    break;
                case 3:
                    player.clearLoginKickTarget();
                    break;
                case 4:
                    target.characterName = "Willow";
                    player.cacheLoginKickTarget(target);
                    break;
                case 5:
                    ++target.worldID;
                    player.cacheLoginKickTarget(target);
                    break;
                case 6:
                    ++target.groupID;
                    player.cacheLoginKickTarget(target);
                    break;
                case 7:
                    --target.lastSlot;
                    player.cacheLoginKickTarget(target);
                    break;
                }
                return true;
            },
            attempt.complete));
        EXPECT_EQ(attempt.player.getKickCharacterCount(), 2);
        EXPECT_EQ(attempt.completions, 0u);
    }
}

TEST(LoginKickRetry, AResendCannotCountForAFreshAttemptWithTheSameIdentityAndTarget) {
    Attempt attempt;
    ASSERT_TRUE(attempt.tick());
    ASSERT_TRUE(attempt.tick());
    EXPECT_FALSE(de::retryLoginKick(
        attempt.player, attempt.player.getExpireTimeForKickCharacter(),
        [&](LoginPlayer& player) { return de::beginLoginKick(player, attempt.send); }, attempt.complete));
    EXPECT_EQ(attempt.player.getKickCharacterCount(), 0);
    EXPECT_EQ(attempt.completions, 0u);
    for (int count = 1; count <= 3; ++count) {
        ASSERT_TRUE(attempt.tick());
        EXPECT_EQ(attempt.player.getKickCharacterCount(), count);
    }
    EXPECT_EQ(attempt.completions, 1u);
}

TEST(LoginKickRetry, EquivalentCacheReplacementAndLiveSelectionChangesPreserveTheAttempt) {
    Attempt attempt;
    const auto target = *attempt.player.getLoginKickTarget();
    EXPECT_TRUE(de::retryLoginKick(
        attempt.player, attempt.player.getExpireTimeForKickCharacter(),
        [&](LoginPlayer& player) {
            player.cacheLoginKickTarget(target);
            player.setWorldID(255);
            player.setServerGroupID(255);
            return attempt.send(player);
        },
        attempt.complete));
    EXPECT_EQ(attempt.player.getKickCharacterCount(), 1);
    EXPECT_EQ(attempt.player.getWorldID(), 255);
    EXPECT_EQ(attempt.player.getServerGroupID(), 255);
    EXPECT_EQ(*attempt.player.getLoginKickTarget(), target);
}

TEST(LoginKickRetry, FreshAttemptsResetBeforeSendingEvenWhenTheRequestRefusesOrThrows) {
    for (const bool throws : {false, true}) {
        Attempt attempt;
        ASSERT_TRUE(attempt.tick());
        ASSERT_TRUE(attempt.tick());
        const auto* target = attempt.player.getLoginKickTarget();
        const auto deadline = attempt.player.getExpireTimeForKickCharacter();
        attempt.player.setPlayerStatus(LPS_BEGIN_SESSION);
        const auto failure = std::make_exception_ptr(std::runtime_error("first request failed"));
        std::exception_ptr caught;
        bool accepted = false;
        try {
            accepted = de::beginLoginKick(attempt.player, [&](LoginPlayer& player) {
                EXPECT_EQ(player.getKickCharacterCount(), 0);
                EXPECT_EQ(player.getLoginKickTarget(), target);
                EXPECT_EQ(player.getExpireTimeForKickCharacter(), deadline);
                if (throws)
                    std::rethrow_exception(failure);
                return false;
            });
        } catch (...) {
            caught = std::current_exception();
        }
        EXPECT_FALSE(accepted);
        EXPECT_EQ(caught, throws ? failure : nullptr);
        EXPECT_EQ(attempt.player.getKickCharacterCount(), 0);
        EXPECT_EQ(attempt.player.getPlayerStatus(), LPS_BEGIN_SESSION);
    }
}

const auto failures = std::array{
    std::make_exception_ptr(std::runtime_error("request failed")), std::make_exception_ptr(Error("request failed")),
    std::make_exception_ptr(NoSuchElementException("request failed")),
    std::make_exception_ptr(DatabaseError("request failed")), std::make_exception_ptr(std::bad_alloc{})};

TEST(LoginKickRetry, EverySenderExceptionPreservesTheCountAndItsExactCause) {
    for (const auto& failure : failures) {
        Attempt attempt;
        ASSERT_TRUE(attempt.tick());
        ASSERT_TRUE(attempt.tick());
        const auto deadline = attempt.player.getExpireTimeForKickCharacter();
        std::exception_ptr caught;
        try {
            de::retryLoginKick(
                attempt.player, deadline, [&](LoginPlayer&) -> bool { std::rethrow_exception(failure); },
                attempt.complete);
        } catch (...) {
            caught = std::current_exception();
        }
        EXPECT_EQ(caught, failure);
        EXPECT_EQ(attempt.player.getKickCharacterCount(), 2);
        EXPECT_EQ(attempt.player.getExpireTimeForKickCharacter(), deadline);
        EXPECT_EQ(attempt.completions, 0u);
        ASSERT_TRUE(attempt.tick());
        EXPECT_EQ(attempt.player.getKickCharacterCount(), 3);
        EXPECT_EQ(attempt.completions, 1u);
    }
}

TEST(LoginKickRetry, CompletionFailuresKeepTheSuccessfulResendAndWaitForTheNextDeadline) {
    for (const auto& failure : failures) {
        Attempt attempt;
        ASSERT_TRUE(attempt.tick());
        ASSERT_TRUE(attempt.tick());
        attempt.now = attempt.player.getExpireTimeForKickCharacter();
        std::exception_ptr caught;
        try {
            de::retryLoginKick(attempt.player, attempt.now, attempt.send,
                               [&](LoginPlayer&) { std::rethrow_exception(failure); });
        } catch (...) {
            caught = std::current_exception();
        }
        EXPECT_EQ(caught, failure);
        EXPECT_EQ(attempt.player.getKickCharacterCount(), 3);
        EXPECT_EQ(attempt.player.getExpireTimeForKickCharacter(), (Timeval{112, 123456}));
        EXPECT_FALSE(de::retryLoginKick(attempt.player, attempt.now, {}, {}));
        ASSERT_TRUE(attempt.tick());
        EXPECT_EQ(attempt.player.getKickCharacterCount(), 3);
        EXPECT_EQ(attempt.completions, 1u);
    }
}

TEST(LoginKickRetry, ActionsRetainTheirSessionChangesWhenTheyThrow) {
    for (const bool completing : {false, true}) {
        Attempt attempt;
        ASSERT_TRUE(attempt.tick());
        ASSERT_TRUE(attempt.tick());
        const auto mutate = [](LoginPlayer& player) -> bool {
            player.setID("NONE");
            player.setPlayerStatus(LPS_BEGIN_SESSION);
            throw std::runtime_error("failed after mutation");
        };
        EXPECT_THROW(de::retryLoginKick(attempt.player, attempt.player.getExpireTimeForKickCharacter(),
                                        completing ? attempt.send : de::LoginKickRequest(mutate),
                                        completing ? std::function<void(LoginPlayer&)>(mutate) : attempt.complete),
                     std::runtime_error);
        EXPECT_EQ(attempt.player.getPlayerStatus(), LPS_BEGIN_SESSION);
        EXPECT_EQ(attempt.player.getID(), "NONE");
        EXPECT_EQ(attempt.player.getKickCharacterCount(), completing ? 3 : 2);
        EXPECT_FALSE(de::retryLoginKick(attempt.player, Timeval{999, 0}, {}, {}));
    }
}

TEST(LoginKickRetry, PreparationRefusalOwnsItsReplyAndCannotReachCompletion) {
    Attempt attempt;
    ASSERT_TRUE(attempt.tick());
    ASSERT_TRUE(attempt.tick());
    FakeLoginAccountRepository accounts;
    FakeLoginCharacterRepository characters;
    EXPECT_FALSE(de::retryLoginKick(
        attempt.player, attempt.player.getExpireTimeForKickCharacter(),
        [&](LoginPlayer& player) {
            player.clearLoginKickTarget();
            return de::prepareLoginKick(player, accounts, characters).has_value();
        },
        attempt.complete));
    EXPECT_EQ(attempt.player.getID(), "NONE");
    EXPECT_EQ(attempt.player.getPlayerStatus(), LPS_BEGIN_SESSION);
    EXPECT_EQ(attempt.player.getLoginKickTarget(), nullptr);
    EXPECT_EQ(attempt.player.getKickCharacterCount(), 2);
    EXPECT_EQ(attempt.completions, 0u);
}

int checkProductionRefusal(bool fresh) {
    Attempt attempt;
    if (!attempt.tick() || !attempt.tick())
        return 1;
    GameServerInfoManager servers;
    de::serverContext().setGameServerInfoManager(&servers);
    if (fresh) {
        attempt.player.setPlayerStatus(LPS_BEGIN_SESSION);
        attempt.player.sendLGKickCharacter();
    } else {
        attempt.player.processCommand();
    }
    if (attempt.player.getID() != "NONE" || attempt.player.getPlayerStatus() != LPS_BEGIN_SESSION ||
        attempt.player.getKickCharacterCount() != (fresh ? 0 : 2))
        return 2;
    attempt.player.disconnect(DISCONNECTED);
    de::serverContext().setGameServerInfoManager(nullptr);
    return attempt.player.getPlayerStatus() == LPS_END_SESSION ? 0 : 3;
}

TEST(LoginKickRetry, ProductionTimeoutRefusalEndsTheWaitWithoutCompletingOrLoggingOffAnAccount) {
    ASSERT_EXIT(std::_Exit(checkProductionRefusal(false)), ::testing::ExitedWithCode(0), "");
}

TEST(LoginKickRetry, ProductionFreshAttemptsResetTheCountBeforeDestinationRefusal) {
    ASSERT_EXIT(std::_Exit(checkProductionRefusal(true)), ::testing::ExitedWithCode(0), "");
}

TEST(LoginKickRetry, ProductionPollingBeforeTheDeadlineNeedsNoDatabaseOrServerManagers) {
    Attempt attempt;
    Timeval now;
    getCurrentTime(now);
    now.tv_sec += 100;
    attempt.player.setExpireTimeForKickCharacter(now);
    EXPECT_NO_THROW(attempt.player.processCommand());
    EXPECT_EQ(attempt.player.getKickCharacterCount(), 0);
    EXPECT_EQ(attempt.player.getPlayerStatus(), LPS_WAITING_FOR_GL_KICK_VERIFY);
}

int checkAllocationFailure(std::size_t failAt, bool refuse) {
    Attempt attempt;
    const std::string account(96, 'a');
    attempt.player.setID(account);
    attempt.player.cacheLoginKickTarget({7, 9, 3, std::string(96, 'c')});
    if (!attempt.tick() || !attempt.tick())
        return 1;
    const auto* previous = attempt.player.getLoginKickTarget();
    bool sent = false;
    bool completed = false;
    const Timeval now = attempt.player.getExpireTimeForKickCharacter();
    const de::LoginKickRequest send = [&](LoginPlayer& player) {
        const auto storage = std::make_unique<char[]>(96);
        storage[0] = 'x';
        if (refuse)
            return false;
        player.setExpireTimeForKickCharacter(now);
        sent = true;
        return true;
    };
    const std::function<void(LoginPlayer&)> complete = [&](LoginPlayer& player) {
        const auto storage = std::make_unique<char[]>(96);
        storage[0] = 'x';
        player.setPlayerStatus(LPS_WAITING_FOR_CL_GET_PC_LIST);
        completed = true;
    };
    AllocationProbe probe(failAt);
    bool failed = false;
    bool advanced = false;
    try {
        advanced = de::retryLoginKick(attempt.player, now, send, complete);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 64 && failed) || probe.outstanding() != 0)
        return 2;
    if (advanced != completed || advanced != (!failed && !refuse) ||
        attempt.player.getKickCharacterCount() != (sent ? 3 : 2) ||
        attempt.player.getExpireTimeForKickCharacter() != (sent ? Timeval{112, 123456} : now) ||
        attempt.player.getPlayerStatus() !=
            (completed ? LPS_WAITING_FOR_CL_GET_PC_LIST : LPS_WAITING_FOR_GL_KICK_VERIFY) ||
        attempt.player.getID() != account || attempt.player.getLoginKickTarget() != previous)
        return 3;
    refuse = false;
    completed = false;
    attempt.player.setPlayerStatus(LPS_WAITING_FOR_GL_KICK_VERIFY);
    const auto next = attempt.player.getExpireTimeForKickCharacter();
    if (!de::retryLoginKick(attempt.player, next, send, complete) || !completed ||
        attempt.player.getKickCharacterCount() != 3)
        return 4;
    return probe.outstanding() == 0 ? 0 : 5;
}

TEST(LoginKickRetry, AllocationFailuresReleaseSnapshotsAndActionStorageBeforeRetry) {
    for (const bool refuse : {false, true}) {
        for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
            SCOPED_TRACE(::testing::Message() << refuse << "/" << failAt);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure(failAt, refuse)), ::testing::ExitedWithCode(0), "");
        }
    }
}

} // namespace
