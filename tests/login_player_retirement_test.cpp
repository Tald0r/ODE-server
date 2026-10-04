#include <fcntl.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "GLIncomingConnectionError.h"
#include "GLIncomingConnectionOK.h"
#include "LoginAccountSession.h"
#include "LoginConnection.h"
#include "LoginContext.h"
#include "LoginPlayerManager.h"
#include "Socket.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginAccountRepository.h"
#include "support/LoopbackListener.h"

namespace {

using namespace std::chrono_literals;

struct Session {
    Session() {
        auto incoming = de::makeLoginConnection(std::make_unique<Socket>());
        player = incoming.get();
        descriptor = player->getSocket()->getSOCKET();
        players.addPlayer(player);
        incoming.release();
    }
    de::LoginContext context;
    LoginPlayerManager players{context};
    LoginPlayer* player = nullptr;
    SOCKET descriptor = INVALID_SOCKET;
    de::LoginRetirementTime now{};
};

TEST(LoginPlayerRetirement, ACleanupFailureCannotKeepAClosedPlayerRegistered) {
    Session session;
    EXPECT_NO_THROW(session.players.retirePlayer(session.descriptor, false, session.now,
                                                 {[](LoginPlayer& player, bool) {
                                                      player.getSocket()->close();
                                                      player.setPlayerStatus(LPS_END_SESSION);
                                                      throw std::runtime_error("cleanup failed");
                                                  },
                                                  {}}));
    EXPECT_EQ(session.players.size(), 0u);
    EXPECT_THROW(session.players.getPlayer(session.descriptor), NoSuchElementException);
}

TEST(LoginPlayerRetirement, RemovingTheOnlyPlayerFromAManagerWithoutAListenerSucceeds) {
    Session session;
    EXPECT_TRUE(session.players.retirePlayer(session.descriptor, false, session.now,
                                             {[](LoginPlayer& player, bool) { player.disconnect(DISCONNECTED); }, {}}));
    EXPECT_EQ(session.players.size(), 0u);
    EXPECT_FALSE(session.players.retirePlayer(session.descriptor, false, session.now, {}));
}

TEST(LoginPlayerRetirement, FailedLogoutMovesItsOwnerOutOfTheDescriptorTable) {
    Session session;
    session.player->loginAccountOwnership().acquire("account", [] { return true; });
    EXPECT_NO_THROW(session.players.retirePlayer(session.descriptor, false, session.now,
                                                 {[](LoginPlayer& player, bool) {
                                                      player.setPlayerStatus(LPS_END_SESSION);
                                                      player.getSocket()->close();
                                                  },
                                                  {}}));
    EXPECT_EQ(session.players.size(), 0u);
    EXPECT_EQ(session.players.pendingRetirements(), 1u);
}

TEST(LoginPlayerRetirement, DetachmentPrecedesReportingAndCleanupUnderTheManagerLock) {
    Session session;
    const auto reason = std::make_exception_ptr(ConnectException("peer closed"));
    unsigned reports = 0;
    unsigned cleanups = 0;
    const auto checkDetached = [&] {
        EXPECT_EQ(session.players.size(), 0u);
        EXPECT_THROW(session.players.getPlayer(session.descriptor), NoSuchElementException);
        EXPECT_THROW(session.players.lock(), Error);
    };
    EXPECT_TRUE(session.players.retirePlayer(session.descriptor, true, session.now,
                                             {[&](LoginPlayer& player, bool flush) {
                                                  checkDetached();
                                                  EXPECT_EQ(&player, session.player);
                                                  EXPECT_TRUE(flush);
                                                  ++cleanups;
                                              },
                                              [&](const std::exception_ptr& error) {
                                                  checkDetached();
                                                  EXPECT_EQ(error, reason);
                                                  ++reports;
                                                  throw std::runtime_error("report failed");
                                              }},
                                             reason));
    EXPECT_EQ(cleanups, 1u);
    EXPECT_EQ(reports, 1u);
    EXPECT_EQ(session.players.pendingRetirements(), 0u);
    EXPECT_EQ(::fcntl(session.descriptor, F_GETFD), -1);
    EXPECT_NO_THROW({
        session.players.lock();
        session.players.unlock();
    });
}

TEST(LoginPlayerRetirement, EveryCleanupFailureStillClosesAndReportsTheOriginalCause) {
    const auto failures = std::array{
        std::make_exception_ptr(Error("cleanup failed")), std::make_exception_ptr(DatabaseError("cleanup failed")),
        std::make_exception_ptr(std::runtime_error("cleanup failed")), std::make_exception_ptr(std::bad_alloc{})};
    for (const auto& failure : failures) {
        Session session;
        unsigned reports = 0;
        EXPECT_TRUE(session.players.retirePlayer(session.descriptor, false, session.now,
                                                 {[&](LoginPlayer&, bool) { std::rethrow_exception(failure); },
                                                  [&](const std::exception_ptr& error) {
                                                      EXPECT_EQ(error, failure);
                                                      EXPECT_EQ(session.player->getPlayerStatus(), LPS_END_SESSION);
                                                      EXPECT_EQ(::fcntl(session.descriptor, F_GETFD), -1);
                                                      ++reports;
                                                      throw std::bad_alloc{};
                                                  }}));
        EXPECT_EQ(reports, 1u);
        EXPECT_EQ(session.players.size(), 0u);
        EXPECT_EQ(session.players.pendingRetirements(), 0u);
    }
}

class Accounts : public FakeLoginAccountRepository {
public:
    void markLoggedOff(const std::string& account) override {
        attempts.push_back(account);
        if (failure)
            std::rethrow_exception(failure);
    }
    std::exception_ptr failure;
    std::vector<std::string> attempts;
};

TEST(LoginPlayerRetirement, FailedLogoutRetainsItsIdentityUntilTheInclusiveRetryDeadline) {
    Session session;
    Accounts accounts;
    accounts.failure = std::make_exception_ptr(DatabaseError("logout failed"));
    const std::string identity(96, 'a');
    session.player->loginAccountOwnership().acquire(identity, [] { return true; });
    const auto* borrowed = session.player->loginAccountOwnership().account();
    session.player->setID("replacement");
    unsigned flushes = 0;
    unsigned reports = 0;
    const de::LoginRetirementActions actions{[&](LoginPlayer& player, bool flush) {
                                                 EXPECT_THROW(session.players.lock(), Error);
                                                 de::disconnectLoginPlayer(
                                                     player, accounts, flush,
                                                     {[&] { ++flushes; }, [&] { player.getSocket()->close(); }});
                                             },
                                             [&](const std::exception_ptr& error) {
                                                 EXPECT_EQ(error, accounts.failure);
                                                 ++reports;
                                             }};
    EXPECT_TRUE(session.players.retirePlayer(session.descriptor, true, session.now, actions));
    EXPECT_EQ(session.player->loginAccountOwnership().account(), borrowed);
    EXPECT_EQ(*borrowed, identity);
    EXPECT_EQ(session.player->getID(), "NONE");
    EXPECT_EQ(session.player->getPlayerStatus(), LPS_END_SESSION);
    EXPECT_EQ(session.players.pendingRetirements(), 1u);
    session.players.retryRetiredPlayers(session.now + 5s - 1ns, actions);
    EXPECT_EQ(accounts.attempts.size(), 1u);
    session.players.retryRetiredPlayers(session.now + 5s, actions);
    EXPECT_EQ(accounts.attempts.size(), 2u);
    session.players.retryRetiredPlayers(session.now + 5s, actions);
    session.players.retryRetiredPlayers(session.now + 10s - 1ns, actions);
    EXPECT_EQ(accounts.attempts.size(), 2u);
    accounts.failure = {};
    session.players.retryRetiredPlayers(session.now + 10s, actions);
    session.players.retryRetiredPlayers(session.now + 100s, actions);
    EXPECT_EQ(accounts.attempts, (std::vector<std::string>{identity, identity, identity}));
    EXPECT_EQ(flushes, 1u);
    EXPECT_EQ(reports, 2u);
    EXPECT_EQ(session.players.pendingRetirements(), 0u);
    EXPECT_NO_THROW({
        session.players.lock();
        session.players.unlock();
    });
}

TEST(LoginPlayerRetirement, SuccessfulLogoutIsFinalEvenWhenTheFlushFailed) {
    Session session;
    Accounts accounts;
    session.player->loginAccountOwnership().acquire("account", [] { return true; });
    const auto failure = std::make_exception_ptr(ConnectException("flush failed"));
    unsigned reports = 0;
    const de::LoginRetirementActions actions{
        [&](LoginPlayer& player, bool flush) {
            de::disconnectLoginPlayer(player, accounts, flush,
                                      {[&] { std::rethrow_exception(failure); }, [&] { player.getSocket()->close(); }});
        },
        [&](const std::exception_ptr& error) {
            EXPECT_EQ(error, failure);
            ++reports;
        }};
    EXPECT_TRUE(session.players.retirePlayer(session.descriptor, true, session.now, actions));
    EXPECT_EQ(session.players.pendingRetirements(), 0u);
    session.players.retryRetiredPlayers(session.now + 10s, actions);
    EXPECT_EQ(accounts.attempts, (std::vector<std::string>{"account"}));
    EXPECT_EQ(reports, 1u);
}

const de::LoginRetirementActions releaseAccount{
    [](LoginPlayer& player, bool) { player.loginAccountOwnership().release([](const std::string&) {}); }, {}};

TEST(LoginPlayerRetirement, DueOwnersCanFinishAroundAnOwnerThatStillNeedsRetry) {
    de::LoginPlayerRetirement retirement;
    const de::LoginRetirementTime now{};
    std::array<LoginPlayer*, 4> players{};
    std::array<unsigned, 4> attempts{};
    for (unsigned i = 0; i < players.size(); ++i) {
        auto player = de::makeLoginConnection(std::make_unique<Socket>());
        players[i] = player.get();
        player->loginAccountOwnership().acquire("account", [] { return true; });
        retirement.retire(std::move(player), false, now + std::chrono::seconds(i), {[](LoginPlayer&, bool) {}, {}});
    }
    const de::LoginRetirementActions actions{[&](LoginPlayer& player, bool flush) {
                                                 EXPECT_FALSE(flush);
                                                 for (unsigned i = 0; i < players.size(); ++i) {
                                                     if (&player != players[i])
                                                         continue;
                                                     ++attempts[i];
                                                     if (i == 1 && attempts[i] == 1)
                                                         throw std::runtime_error("still unavailable");
                                                     releaseAccount.disconnect(player, flush);
                                                 }
                                             },
                                             {}};
    retirement.retry(now + 7s, actions);
    EXPECT_EQ(attempts, (std::array<unsigned, 4>{1, 1, 1, 0}));
    EXPECT_EQ(retirement.size(), 2u);
    retirement.retry(now + 8s, actions);
    EXPECT_EQ(attempts, (std::array<unsigned, 4>{1, 1, 1, 1}));
    EXPECT_EQ(retirement.size(), 1u);
    retirement.retry(now + 12s, actions);
    EXPECT_EQ(attempts, (std::array<unsigned, 4>{1, 2, 1, 1}));
    EXPECT_EQ(retirement.size(), 0u);
}

TEST(LoginPlayerRetirement, RepeatedMissingAndOutOfRangeRetirementDoesNoWork) {
    Session session;
    unsigned calls = 0;
    const de::LoginRetirementActions actions{[&](LoginPlayer&, bool) { ++calls; }, {}};
    for (const SOCKET descriptor : {INVALID_SOCKET, static_cast<SOCKET>(2000), static_cast<SOCKET>(3000)})
        EXPECT_FALSE(session.players.retirePlayer(descriptor, false, session.now, actions));
    EXPECT_EQ(session.players.size(), 1u);
    EXPECT_TRUE(session.players.retirePlayer(session.descriptor, false, session.now, actions));
    EXPECT_FALSE(session.players.retirePlayer(session.descriptor, true, session.now, actions));
    EXPECT_EQ(calls, 1u);
}

TEST(LoginPlayerRetirement, FailedLockAcquisitionPreservesTheCallersLockAndPlayer) {
    ASSERT_EXIT(
        {
            Session session;
            session.players.lock();
            bool refused = false;
            try {
                session.players.retirePlayer(session.descriptor, false, session.now, {});
            } catch (const Error&) {
                refused = true;
            }
            bool stillHeld = false;
            try {
                session.players.lock();
            } catch (const Error&) {
                stillHeld = true;
            }
            const bool intact = session.players.getPlayer(session.descriptor) == session.player;
            session.players.unlock();
            std::_Exit(refused && stillHeld && intact ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

struct Visits {
    unsigned destroyed = 0;
    unsigned inputs = 0;
    unsigned commands = 0;
    unsigned outputs = 0;
    std::vector<bool> flushes;
    std::exception_ptr inputFailure;
    std::exception_ptr commandFailure;
    std::exception_ptr outputFailure;
    std::exception_ptr disconnectFailure;
    std::exception_ptr descriptionFailure;
    LoginPlayerManager* manager = nullptr;
    bool cleanupLocked = false;
};

class TrackedPlayer : public LoginPlayer {
public:
    TrackedPlayer(Socket* socket, Visits& visits) : LoginPlayer(socket), visits(visits) {
        setPlayerStatus(LPS_BEGIN_SESSION);
    }
    ~TrackedPlayer() override {
        ++visits.destroyed;
    }
    void processInput() override {
        ++visits.inputs;
        fail(visits.inputFailure);
    }
    void processCommand(bool = true) override {
        ++visits.commands;
        fail(visits.commandFailure);
    }
    void processOutput() override {
        ++visits.outputs;
        fail(visits.outputFailure);
    }
    void disconnect(bool flush) override {
        if (visits.manager) {
            try {
                visits.manager->lock();
                visits.manager->unlock();
            } catch (const Error&) {
                visits.cleanupLocked = true;
            }
        }
        visits.flushes.push_back(flush);
        fail(visits.disconnectFailure);
        LoginPlayer::disconnect(flush);
    }
    std::string toString() const override {
        fail(visits.descriptionFailure);
        return "tracked player";
    }

private:
    static void fail(const std::exception_ptr& failure) {
        if (failure)
            std::rethrow_exception(failure);
    }
    Visits& visits;
};

TrackedPlayer* addTracked(LoginPlayerManager& players, Visits& visits, std::unique_ptr<Socket> socket) {
    de::LoginConnection player(new TrackedPlayer(socket.release(), visits));
    players.addPlayer(player.get());
    return static_cast<TrackedPlayer*>(player.release());
}

struct NetworkSession {
    NetworkSession() {
        auto socket = std::make_unique<Socket>("127.0.0.1", listener.port());
        socket->connect();
        peer = listener.accept();
        player = addTracked(players, visits, std::move(socket));
        descriptor = player->getSocket()->getSOCKET();
        player->loginAccountOwnership().acquire("account", [] { return true; });
        visits.disconnectFailure = std::make_exception_ptr(DatabaseError("logout failed"));
    }
    ~NetworkSession() {
        ::close(peer);
    }
    void waitFor(short event) {
        pollfd ready{descriptor, event, 0};
        ASSERT_EQ(::poll(&ready, 1, 2000), 1);
        ASSERT_NE(ready.revents & event, 0);
        players.pollSockets();
    }
    void checkRetired(bool flushed) {
        EXPECT_EQ(players.size(), 0u);
        EXPECT_EQ(players.pendingRetirements(), 1u);
        EXPECT_EQ(player->getPlayerStatus(), LPS_END_SESSION);
        EXPECT_EQ(::fcntl(descriptor, F_GETFD), -1);
        EXPECT_EQ(visits.flushes, (std::vector<bool>{flushed}));
        EXPECT_NO_THROW({
            players.processInputs();
            players.processCommands();
            players.processOutputs();
        });
        EXPECT_NO_THROW({
            players.lock();
            players.unlock();
        });
        players.retryRetiredPlayers(std::chrono::steady_clock::now() + 10s, releaseAccount);
        EXPECT_EQ(players.pendingRetirements(), 0u);
        EXPECT_EQ(visits.destroyed, 1u);
    }
    de::LoginContext context;
    Visits visits;
    LoginPlayerManager players{context};
    LoopbackListener listener;
    int peer = -1;
    TrackedPlayer* player = nullptr;
    SOCKET descriptor = INVALID_SOCKET;
};

TEST(LoginPlayerRetirement, CommandFailuresKeepTheirFlushPolicyAndRetireDespiteFailedLogout) {
    for (const bool protocol : {true, false}) {
        NetworkSession session;
        session.visits.commandFailure = protocol ? std::make_exception_ptr(InvalidProtocolException("bad command"))
                                                 : std::make_exception_ptr(ConnectException("lost connection"));
        session.waitFor(POLLOUT);
        EXPECT_NO_THROW(session.players.processCommands());
        session.checkRetired(protocol);
        EXPECT_EQ(session.visits.commands, 1u);
        EXPECT_EQ(session.visits.outputs, 0u);
    }
}

TEST(LoginPlayerRetirement, InputFailureRetiresBeforeLaterCommandsOrOutput) {
    NetworkSession session;
    session.visits.inputFailure = std::make_exception_ptr(ConnectException("read failed"));
    ASSERT_EQ(::send(session.peer, "x", 1, 0), 1);
    session.waitFor(POLLIN);
    EXPECT_NO_THROW(session.players.processInputs());
    session.checkRetired(false);
    EXPECT_EQ(session.visits.inputs, 1u);
    EXPECT_EQ(session.visits.commands, 0u);
    EXPECT_EQ(session.visits.outputs, 0u);
}

TEST(LoginPlayerRetirement, RepeatedUnauthenticatedPeerClosesRetireWithoutErrorOutput) {
    de::LoginContext context;
    LoginPlayerManager players(context);
    LoopbackListener listener;
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        auto socket = std::make_unique<Socket>("127.0.0.1", listener.port());
        socket->connect();
        const int peer = listener.accept();
        auto player = de::makeLoginConnection(std::move(socket));
        const auto descriptor = player->getSocket()->getSOCKET();
        players.addPlayer(player.get());
        player.release();
        ASSERT_EQ(::close(peer), 0);
        pollfd ready{descriptor, POLLIN, 0};
        ASSERT_EQ(::poll(&ready, 1, 2000), 1);
        players.pollSockets();

        testing::internal::CaptureStdout();
        testing::internal::CaptureStderr();
        EXPECT_NO_THROW(players.processInputs());
        const auto output = testing::internal::GetCapturedStdout();
        const auto errors = testing::internal::GetCapturedStderr();
        EXPECT_TRUE(output.empty()) << output;
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_EQ(players.size(), 0u);
        EXPECT_EQ(players.pendingRetirements(), 0u);
        EXPECT_EQ(::fcntl(descriptor, F_GETFD), -1);
    }
}

TEST(LoginPlayerRetirement, RealInputAndCleanupFailuresRemainVisible) {
    NetworkSession session;
    session.visits.inputFailure = std::make_exception_ptr(ConnectException("read failed"));
    ASSERT_EQ(::send(session.peer, "x", 1, 0), 1);
    session.waitFor(POLLIN);
    testing::internal::CaptureStdout();
    EXPECT_NO_THROW(session.players.processInputs());
    const auto output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("read failed"), std::string::npos);
    EXPECT_NE(output.find("logout failed"), std::string::npos);
    session.checkRetired(false);
}

TEST(LoginPlayerRetirement, QuietPeerCloseStillReportsFailedCleanupAndRetainsItsOwner) {
    NetworkSession session;
    session.visits.inputFailure = std::make_exception_ptr(PeerClosedException());
    ASSERT_EQ(::send(session.peer, "x", 1, 0), 1);
    session.waitFor(POLLIN);
    testing::internal::CaptureStdout();
    EXPECT_NO_THROW(session.players.processInputs());
    const auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output.find("connect closed"), std::string::npos);
    EXPECT_NE(output.find("logout failed"), std::string::npos);
    session.checkRetired(false);
}

TEST(LoginPlayerRetirement, ADescriptorWithASocketErrorRetiresWithoutReadingOrFlushing) {
    NetworkSession session;
    session.player->getSocket()->close();
    session.players.pollSockets();
    EXPECT_NO_THROW(session.players.processInputs());
    session.checkRetired(false);
    EXPECT_EQ(session.visits.inputs, 0u);
}

TEST(LoginPlayerRetirement, OutputFailuresNeverFlushAndCannotKeepStaleReadiness) {
    for (const auto& failure : {std::make_exception_ptr(ConnectException("write failed")),
                                std::make_exception_ptr(ProtocolException("bad output"))}) {
        NetworkSession session;
        session.visits.outputFailure = failure;
        session.waitFor(POLLOUT);
        EXPECT_NO_THROW(session.players.processOutputs());
        session.checkRetired(false);
        EXPECT_EQ(session.visits.outputs, 1u);
    }
}

TEST(LoginPlayerRetirement, UrgentInputRetiresEvenWhenItsDescriptionAndCleanupThrow) {
#ifndef __linux__
    GTEST_SKIP() << "Requires Linux TCP urgent-data readiness from poll";
#else
    NetworkSession session;
    session.visits.descriptionFailure = std::make_exception_ptr(std::runtime_error("description failed"));
    ASSERT_EQ(::send(session.peer, "!", 1, MSG_OOB), 1);
    ASSERT_EQ(::send(session.peer, "x", 1, 0), 1);
    session.waitFor(POLLPRI);
    EXPECT_NO_THROW(session.players.processExceptions());
    session.checkRetired(true);
    EXPECT_EQ(session.visits.inputs, 0u);
    EXPECT_EQ(session.visits.commands, 0u);
    EXPECT_EQ(session.visits.outputs, 0u);
#endif
}

TEST(LoginPlayerRetirement, RetryingAnOldOwnerCannotCloseOrRemoveAReusedDescriptor) {
    Visits replacementVisits;
    NetworkSession session;
    session.waitFor(POLLOUT);
    const de::LoginRetirementTime now{};
    EXPECT_TRUE(session.players.retirePlayer(session.descriptor, false, now, {[](LoginPlayer&, bool) {}, {}}));
    auto socket = std::make_unique<Socket>("127.0.0.1", session.listener.port());
    ASSERT_EQ(socket->getSOCKET(), session.descriptor);
    socket->connect();
    const int replacementPeer = session.listener.accept();
    auto* replacement = addTracked(session.players, replacementVisits, std::move(socket));
    session.players.processOutputs();
    EXPECT_EQ(replacementVisits.outputs, 0u);
    session.players.retryRetiredPlayers(now + 5s, releaseAccount);
    EXPECT_EQ(session.players.getPlayer(session.descriptor), replacement);
    EXPECT_EQ(session.players.pendingRetirements(), 0u);
    EXPECT_EQ(session.visits.destroyed, 1u);
    EXPECT_NE(::fcntl(session.descriptor, F_GETFD), -1);
    ASSERT_EQ(::send(replacementPeer, "x", 1, 0), 1);
    session.waitFor(POLLIN);
    session.players.processInputs();
    session.players.processOutputs();
    EXPECT_EQ(replacementVisits.inputs, 1u);
    EXPECT_EQ(replacementVisits.outputs, 1u);
    EXPECT_TRUE(session.players.retirePlayer(session.descriptor, false, now, releaseAccount));
    EXPECT_EQ(replacementVisits.destroyed, 1u);
    ::close(replacementPeer);
}

TEST(LoginPlayerRetirement, BothProductionIncomingRepliesRetireUnderTheLookupLockDespiteFailedLogout) {
    for (const bool accepted : {true, false}) {
        SCOPED_TRACE(accepted);
        ASSERT_EXIT(
            {
                NetworkSession session;
                session.player->setID("account");
                session.player->setGameServerIP("127.0.0.1");
                session.player->setPlayerStatus(LPS_AFTER_SENDING_LG_INCOMING_CONNECTION);
                session.visits.manager = &session.players;
                de::loginContext().setLoginPlayerManager(&session.players);
                if (accepted) {
                    GLIncomingConnectionOK packet;
                    packet.setPlayerID("account");
                    packet.setTCPPort(7777);
                    packet.setKey(123);
                    GLIncomingConnectionOKHandler::execute(&packet);
                } else {
                    GLIncomingConnectionError packet;
                    packet.setPlayerID("account");
                    GLIncomingConnectionErrorHandler::execute(&packet);
                }
                const bool intact = session.players.size() == 0 && session.players.pendingRetirements() == 1 &&
                                    session.visits.cleanupLocked && session.visits.flushes.size() == 1 &&
                                    session.visits.flushes.front() &&
                                    session.player->getPlayerStatus() == LPS_END_SESSION &&
                                    ::fcntl(session.descriptor, F_GETFD) == -1;
                session.players.retryRetiredPlayers(std::chrono::steady_clock::now() + 10s, releaseAccount);
                de::loginContext().setLoginPlayerManager(nullptr);
                std::_Exit(intact && session.visits.destroyed == 1 ? 0 : 1);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(LoginPlayerRetirement, RemovingBothDescriptorExtremesKeepsTheRemainingPlayersReachable) {
    de::LoginContext context;
    std::array<Visits, 3> visits{};
    LoginPlayerManager players(context);
    std::array<SOCKET, 3> descriptors{};
    const de::LoginRetirementTime now{};
    for (unsigned i = 0; i < descriptors.size(); ++i)
        descriptors[i] = addTracked(players, visits[i], std::make_unique<Socket>())->getSocket()->getSOCKET();
    ASSERT_LT(descriptors[0], descriptors[1]);
    ASSERT_LT(descriptors[1], descriptors[2]);
    EXPECT_TRUE(players.retirePlayer(descriptors[2], false, now, releaseAccount));
    players.processCommands();
    EXPECT_EQ(visits[0].commands, 1u);
    EXPECT_EQ(visits[1].commands, 1u);
    EXPECT_TRUE(players.retirePlayer(descriptors[0], false, now, releaseAccount));
    players.processCommands();
    EXPECT_EQ(visits[1].commands, 2u);
    EXPECT_TRUE(players.retirePlayer(descriptors[1], false, now, releaseAccount));
    EXPECT_NO_THROW(players.processCommands());
    EXPECT_EQ(players.size(), 0u);
    for (const auto& visit : visits)
        EXPECT_EQ(visit.destroyed, 1u);
}

TEST(LoginPlayerRetirement, AdoptingAndReleasingAPendingOwnerNeverAllocates) {
    ASSERT_EXIT(([] {
                    Session session;
                    session.player->loginAccountOwnership().acquire(std::string(96, 'a'), [] { return true; });
                    const de::LoginRetirementActions keep{+[](LoginPlayer&, bool) {}, {}};
                    AllocationProbe probe(1);
                    if (!session.players.retirePlayer(session.descriptor, false, session.now, keep) ||
                        session.players.size() != 0 || session.players.pendingRetirements() != 1)
                        std::_Exit(1);
                    session.players.retryRetiredPlayers(session.now + 5s, releaseAccount);
                    const bool intact = session.players.pendingRetirements() == 0 && probe.attempts() == 0 &&
                                        probe.outstanding() == 0 && ::fcntl(session.descriptor, F_GETFD) == -1;
                    std::_Exit(intact ? 0 : 2);
                }()),
                ::testing::ExitedWithCode(0), "");
}

TEST(LoginPlayerRetirement, AllocationFailuresInLogoutOrReportingRetainOnlyTheRetryOwner) {
    for (std::size_t failAt = 1; failAt <= 16; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(([&] {
                        Session session;
                        session.player->loginAccountOwnership().acquire(std::string(96, 'a'), [] { return true; });
                        unsigned writes = 0;
                        unsigned reports = 0;
                        const auto failure = std::make_exception_ptr(std::runtime_error("logout failed"));
                        const de::LoginRetirementActions actions{
                            [&](LoginPlayer& player, bool) {
                                player.loginAccountOwnership().release([&](const std::string& account) {
                                    auto prepared = account;
                                    auto payload = std::make_unique<std::array<char, 256>>();
                                    if (prepared.size() != 96 || !payload)
                                        std::_Exit(1);
                                    ++writes;
                                    std::rethrow_exception(failure);
                                });
                            },
                            [&](const std::exception_ptr&) {
                                ++reports;
                                std::string message(128, 'm');
                                if (message.size() != 128)
                                    std::_Exit(2);
                            }};
                        AllocationProbe probe(failAt);
                        session.players.retirePlayer(session.descriptor, false, session.now, actions);
                        session.players.retryRetiredPlayers(session.now + 5s, actions);
                        if (session.players.size() != 0 || session.players.pendingRetirements() != 1 || reports != 2 ||
                            writes > 2 || probe.outstanding() != 0 ||
                            session.player->getPlayerStatus() != LPS_END_SESSION ||
                            !session.player->loginAccountOwnership().account())
                            std::_Exit(3);
                        probe.stopFailing();
                        session.players.retryRetiredPlayers(session.now + 10s, releaseAccount);
                        const bool intact = session.players.pendingRetirements() == 0 && probe.outstanding() == 0 &&
                                            (failAt != 16 || !probe.rejected());
                        std::_Exit(intact ? 0 : 4);
                    }()),
                    ::testing::ExitedWithCode(0), "");
    }
}

TEST(LoginPlayerRetirement, ScopedTeardownDrainsAllPendingLocalResourcesWithoutAnotherLogout) {
    ASSERT_EXIT(([] {
                    const int available = nextSocketDescriptor();
                    Visits visits;
                    unsigned attempts = 0;
                    const de::LoginRetirementActions keep{[&](LoginPlayer&, bool) { ++attempts; }, {}};
                    AllocationProbe probe;
                    {
                        de::LoginContext context;
                        LoginPlayerManager players(context);
                        const de::LoginRetirementTime now{};
                        for (unsigned i = 0; i < 32; ++i) {
                            auto* player = addTracked(players, visits, std::make_unique<Socket>());
                            player->loginAccountOwnership().acquire(std::string(96, 'a'), [] { return true; });
                            players.retirePlayer(player->getSocket()->getSOCKET(), false, now, keep);
                        }
                        if (players.size() != 0 || players.pendingRetirements() != 32 || visits.destroyed != 0)
                            std::_Exit(1);
                    }
                    const bool intact = visits.destroyed == 32 && attempts == 32 && probe.outstanding() == 0 &&
                                        nextSocketDescriptor() == available;
                    std::_Exit(intact ? 0 : 2);
                }()),
                ::testing::ExitedWithCode(0), "");
}

} // namespace
