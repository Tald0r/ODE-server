#include <fcntl.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "GLIncomingConnectionError.h"
#include "GLIncomingConnectionOK.h"
#include "LCReconnect.h"
#include "LoginConnection.h"
#include "LoginContext.h"
#include "LoginIncomingReply.h"
#include "LoginPlayerManager.h"
#include "Socket.h"
#include "SocketOutputStream.h"
#include "support/AllocationProbe.h"

namespace {

using namespace std::chrono_literals;

struct Session {
    Session() {
        auto incoming = de::makeLoginConnection(std::make_unique<Socket>());
        player = incoming.get();
        player->setID("account");
        player->setGameServerIP("203.0.113.7");
        player->setPlayerStatus(LPS_AFTER_SENDING_LG_INCOMING_CONNECTION);
        descriptor = player->getSocket()->getSOCKET();
        players.addPlayer(player);
        incoming.release();
        accepted.setPlayerID("account");
        accepted.setTCPPort(7777);
        accepted.setKey(123);
        refused.setPlayerID("account");
    }
    de::LoginContext context;
    LoginPlayerManager players{context};
    LoginPlayer* player = nullptr;
    SOCKET descriptor = INVALID_SOCKET;
    GLIncomingConnectionOK accepted;
    GLIncomingConnectionError refused;
    de::LoginRetirementTime now{};
    de::LoginIncomingReplyActions actions{[](LoginPlayer&, LCReconnect&) {}, {[](LoginPlayer&, bool) {}, {}}};
};

bool dispatch(Session& session, bool accepted) {
    return accepted
               ? de::completeLoginIncomingConnection(session.players, session.accepted, session.now, session.actions)
               : de::refuseLoginIncomingConnection(session.players, session.refused, session.now, session.actions);
}

TEST(LoginIncomingReply, ASuccessReplyCannotRetireASessionOutsideThePendingPhase) {
    Session session;
    session.player->setPlayerStatus(LPS_PC_MANAGEMENT);
    EXPECT_FALSE(de::completeLoginIncomingConnection(session.players, session.accepted, session.now, session.actions));
    EXPECT_EQ(session.players.size(), 1u);
    EXPECT_EQ(session.players.getPlayer(session.descriptor), session.player);
}

TEST(LoginIncomingReply, AnErrorReplyOutsideThePendingPhaseIsRefusedWithoutAnAssertion) {
    Session session;
    session.player->setPlayerStatus(LPS_PC_MANAGEMENT);
    EXPECT_NO_THROW(EXPECT_FALSE(
        de::refuseLoginIncomingConnection(session.players, session.refused, session.now, session.actions)));
    EXPECT_EQ(session.players.size(), 1u);
}

TEST(LoginIncomingReply, AFailedReconnectSendStillRetiresItsAdmittedPlayer) {
    Session session;
    session.actions.send = [](LoginPlayer&, LCReconnect&) { throw std::runtime_error("send failed"); };
    EXPECT_NO_THROW(EXPECT_TRUE(
        de::completeLoginIncomingConnection(session.players, session.accepted, session.now, session.actions)));
    EXPECT_EQ(session.players.size(), 0u);
}

TEST(LoginIncomingReply, BothRepliesPreserveEveryNonPendingPhaseWithoutCallingActions) {
    for (int value = 0; value < PLAYER_STATUS_MAX; ++value) {
        const auto status = static_cast<PlayerStatus>(value);
        if (status == LPS_AFTER_SENDING_LG_INCOMING_CONNECTION)
            continue;
        SCOPED_TRACE(value);
        Session session;
        session.player->setPlayerStatus(status);
        unsigned calls = 0;
        session.actions = {[&](LoginPlayer&, LCReconnect&) { ++calls; },
                           {[&](LoginPlayer&, bool) { ++calls; }, [&](const std::exception_ptr&) { ++calls; }}};
        EXPECT_FALSE(dispatch(session, true));
        EXPECT_FALSE(dispatch(session, false));
        EXPECT_EQ(calls, 0u);
        EXPECT_EQ(session.players.size(), 1u);
        EXPECT_EQ(session.player->getPlayerStatus(), status);
        EXPECT_EQ(session.player->getID(), "account");
        EXPECT_NE(::fcntl(session.descriptor, F_GETFD), -1);
    }
}

TEST(LoginIncomingReply, MissingAccountsAndEveryIdentityMismatchPreserveThePendingSession) {
    for (const auto& account : {std::string(), std::string("NONE"), std::string("Account"), std::string("account "),
                                std::string("account\0suffix", 14), std::string(96, 'a')}) {
        Session session;
        session.accepted.setPlayerID(account);
        session.refused.setPlayerID(account);
        EXPECT_FALSE(dispatch(session, true));
        EXPECT_FALSE(dispatch(session, false));
        EXPECT_EQ(session.players.getPlayer(session.descriptor), session.player);
        EXPECT_EQ(session.player->getPlayerStatus(), LPS_AFTER_SENDING_LG_INCOMING_CONNECTION);
        session.player->setID(account);
        if (account.empty() || account == "NONE") {
            EXPECT_FALSE(dispatch(session, true));
            EXPECT_FALSE(dispatch(session, false));
            EXPECT_EQ(session.players.size(), 1u);
        }
    }
}

TEST(LoginIncomingReply, ReconnectUsesTheSavedPublicAddressAndOwnsItsCompleteWireFieldsBeforeSending) {
    Session session;
    session.accepted.setKey(0x12345678);
    session.player->loginAccountOwnership().acquire("account", [] { return true; });
    std::string bytes;
    unsigned cleanups = 0;
    session.actions.send = [&](LoginPlayer& player, LCReconnect& packet) {
        EXPECT_EQ(&player, session.player);
        EXPECT_EQ(session.players.getPlayer(session.descriptor), &player);
        EXPECT_THROW(session.players.lock(), Error);
        EXPECT_EQ(player.getPlayerStatus(), LPS_AFTER_SENDING_LG_INCOMING_CONNECTION);
        player.setGameServerIP("192.0.2.1");
        player.setID("changed identity");
        EXPECT_EQ(packet.getGameServerIP(), "203.0.113.7");
        SocketOutputStream wire(nullptr, 64);
        packet.write(wire);
        bytes.assign(wire.getBuffer(), wire.length());
    };
    session.actions.retirement.disconnect = [&](LoginPlayer& player, bool flush) {
        ++cleanups;
        EXPECT_TRUE(flush);
        EXPECT_EQ(session.players.size(), 0u);
        EXPECT_THROW(session.players.getPlayer(session.descriptor), NoSuchElementException);
        EXPECT_THROW(session.players.lock(), Error);
        EXPECT_TRUE(player.loginAccountOwnership().owns("account"));
        player.loginAccountOwnership().release([](const std::string&) {});
    };
    EXPECT_TRUE(dispatch(session, true));
    EXPECT_EQ(bytes, std::string("\x0b"
                                 "203.0.113.7"
                                 "\x61\x1e\x00\x00\x78\x56\x34\x12",
                                 20));
    EXPECT_EQ(cleanups, 1u);
    EXPECT_EQ(session.players.size(), 0u);
    EXPECT_EQ(session.players.pendingRetirements(), 0u);
    EXPECT_EQ(::fcntl(session.descriptor, F_GETFD), -1);
    EXPECT_NO_THROW({
        session.players.lock();
        session.players.unlock();
    });
}

TEST(LoginIncomingReply, PortAndKeyKeepTheirExistingUnsignedWireWidths) {
    for (const auto value : {0u, 1u, 65535u, std::numeric_limits<uint>::max()}) {
        Session session;
        session.accepted.setTCPPort(value);
        session.accepted.setKey(value);
        session.actions.send = [&](LoginPlayer&, LCReconnect& packet) {
            EXPECT_EQ(packet.getGameServerPort(), value);
            EXPECT_EQ(packet.getKey(), value);
        };
        EXPECT_TRUE(dispatch(session, true));
    }
}

TEST(LoginIncomingReply, AGameRefusalRetiresWithoutSendingAndBothDuplicateReplyKindsAreIgnored) {
    for (const bool accepted : {true, false}) {
        Session session;
        unsigned sends = 0;
        unsigned cleanups = 0;
        session.refused.setMessage("game rejected connection");
        session.actions.send = [&](LoginPlayer&, LCReconnect&) { ++sends; };
        session.actions.retirement.disconnect = [&](LoginPlayer&, bool flush) {
            EXPECT_TRUE(flush);
            ++cleanups;
        };
        EXPECT_TRUE(dispatch(session, accepted));
        EXPECT_FALSE(dispatch(session, true));
        EXPECT_FALSE(dispatch(session, false));
        EXPECT_EQ(sends, accepted ? 1u : 0u);
        EXPECT_EQ(cleanups, 1u);
        EXPECT_EQ(session.players.pendingRetirements(), 0u);
    }
}

TEST(LoginIncomingReply, AllSendFailuresAreReportedUnchangedAndCloseWithoutFlushingPartialReplies) {
    const auto failures = std::array{
        std::make_exception_ptr(Error("send failed")), std::make_exception_ptr(NoSuchElementException("send failed")),
        std::make_exception_ptr(DatabaseError("send failed")),
        std::make_exception_ptr(std::runtime_error("send failed")), std::make_exception_ptr(std::bad_alloc{})};
    for (const auto& failure : failures) {
        Session session;
        unsigned reports = 0;
        unsigned cleanups = 0;
        session.actions.send = [&](LoginPlayer&, LCReconnect&) { std::rethrow_exception(failure); };
        session.actions.retirement.disconnect = [&](LoginPlayer&, bool flush) {
            EXPECT_FALSE(flush);
            ++cleanups;
        };
        session.actions.retirement.report = [&](const std::exception_ptr& error) {
            EXPECT_EQ(error, failure);
            EXPECT_EQ(session.players.size(), 0u);
            ++reports;
            throw std::runtime_error("report failed");
        };
        EXPECT_TRUE(dispatch(session, true));
        EXPECT_EQ(reports, 1u);
        EXPECT_EQ(cleanups, 1u);
        EXPECT_EQ(::fcntl(session.descriptor, F_GETFD), -1);
        EXPECT_NO_THROW({
            session.players.lock();
            session.players.unlock();
        });
    }
}

TEST(LoginIncomingReply, InvalidReconnectSerializationStillConsumesTheAdmittedSession) {
    for (const auto& address : {std::string(), std::string(16, 'x')}) {
        Session session;
        session.player->setGameServerIP(address);
        unsigned reports = 0;
        session.actions.send = [](LoginPlayer&, LCReconnect& packet) {
            SocketOutputStream wire(nullptr, 64);
            packet.write(wire);
        };
        session.actions.retirement.disconnect = [](LoginPlayer&, bool flush) { EXPECT_FALSE(flush); };
        session.actions.retirement.report = [&](const std::exception_ptr& failure) {
            EXPECT_THROW(std::rethrow_exception(failure), ProtocolException);
            ++reports;
        };
        EXPECT_TRUE(dispatch(session, true));
        EXPECT_EQ(reports, 1u);
        EXPECT_EQ(session.players.size(), 0u);
    }
}

TEST(LoginIncomingReply, SendAndLogoutFailuresKeepOnlyTheAccountOwnerUntilTheSuppliedRetryDeadline) {
    Session session;
    const auto sendFailure = std::make_exception_ptr(ConnectException("send failed"));
    const auto logoutFailure = std::make_exception_ptr(DatabaseError("logout failed"));
    session.player->loginAccountOwnership().acquire("account", [] { return true; });
    const auto* borrowed = session.player->loginAccountOwnership().account();
    std::vector<std::exception_ptr> reports;
    session.actions.send = [&](LoginPlayer&, LCReconnect&) { std::rethrow_exception(sendFailure); };
    session.actions.retirement.disconnect = [&](LoginPlayer&, bool flush) {
        EXPECT_FALSE(flush);
        std::rethrow_exception(logoutFailure);
    };
    session.actions.retirement.report = [&](const std::exception_ptr& error) { reports.push_back(error); };
    EXPECT_TRUE(dispatch(session, true));
    EXPECT_EQ(reports, (std::vector<std::exception_ptr>{sendFailure, logoutFailure}));
    EXPECT_EQ(session.players.pendingRetirements(), 1u);
    EXPECT_EQ(session.player->loginAccountOwnership().account(), borrowed);
    EXPECT_EQ(session.player->getPlayerStatus(), LPS_END_SESSION);
    unsigned releases = 0;
    const de::LoginRetirementActions release{[&](LoginPlayer& player, bool flush) {
                                                 EXPECT_FALSE(flush);
                                                 player.loginAccountOwnership().release(
                                                     [&](const std::string& account) {
                                                         EXPECT_EQ(account, "account");
                                                         ++releases;
                                                     });
                                             },
                                             {}};
    session.players.retryRetiredPlayers(session.now + 5s - 1ns, release);
    EXPECT_EQ(releases, 0u);
    session.players.retryRetiredPlayers(session.now + 5s, release);
    session.players.retryRetiredPlayers(session.now + 10s, release);
    EXPECT_EQ(releases, 1u);
    EXPECT_EQ(session.players.pendingRetirements(), 0u);
}

TEST(LoginIncomingReply, AReplyForOneAccountPreservesOtherPendingAccounts) {
    Session session;
    auto other = de::makeLoginConnection(std::make_unique<Socket>());
    auto* borrowed = other.get();
    other->setID("other");
    other->setPlayerStatus(LPS_AFTER_SENDING_LG_INCOMING_CONNECTION);
    const auto descriptor = other->getSocket()->getSOCKET();
    session.players.addPlayer(other.get());
    other.release();
    EXPECT_TRUE(dispatch(session, false));
    EXPECT_EQ(session.players.size(), 1u);
    EXPECT_EQ(session.players.getPlayer(descriptor), borrowed);
    EXPECT_EQ(borrowed->getPlayerStatus(), LPS_AFTER_SENDING_LG_INCOMING_CONNECTION);
    EXPECT_NE(::fcntl(descriptor, F_GETFD), -1);
}

TEST(LoginIncomingReply, FailedLockAcquisitionPreservesTheCallersLockAndPendingPlayer) {
    for (const bool accepted : {true, false}) {
        ASSERT_EXIT(
            {
                Session session;
                session.players.lock();
                bool refused = false;
                try {
                    (void)dispatch(session, accepted);
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
}

TEST(LoginIncomingReply, ProductionHandlersRefuseLateRepliesWithoutRetiringAnotherPhase) {
    for (const bool accepted : {true, false}) {
        ASSERT_EXIT(
            {
                Session session;
                session.player->setPlayerStatus(LPS_PC_MANAGEMENT);
                de::loginContext().setLoginPlayerManager(&session.players);
                if (accepted)
                    GLIncomingConnectionOKHandler::execute(&session.accepted);
                else
                    GLIncomingConnectionErrorHandler::execute(&session.refused);
                const bool intact = session.players.size() == 1 &&
                                    session.players.getPlayer(session.descriptor) == session.player &&
                                    session.player->getPlayerStatus() == LPS_PC_MANAGEMENT;
                de::loginContext().setLoginPlayerManager(nullptr);
                std::_Exit(intact ? 0 : 1);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(LoginIncomingReply, TheProductionSuccessHandlerRetiresAfterReconnectSerializationFails) {
    ASSERT_EXIT(
        {
            Session session;
            session.player->setGameServerIP("");
            de::loginContext().setLoginPlayerManager(&session.players);
            GLIncomingConnectionOKHandler::execute(&session.accepted);
            const bool intact = session.players.size() == 0 && session.players.pendingRetirements() == 0 &&
                                ::fcntl(session.descriptor, F_GETFD) == -1;
            de::loginContext().setLoginPlayerManager(nullptr);
            std::_Exit(intact ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(LoginIncomingReply, AllocationFailuresEitherPreserveLookupForRetryOrRetireTheAdmittedOwner) {
    for (const bool accepted : {true, false}) {
        for (std::size_t failAt = 1; failAt <= 24; ++failAt) {
            SCOPED_TRACE(accepted);
            SCOPED_TRACE(failAt);
            ASSERT_EXIT(([&] {
                            auto session = std::make_unique<Session>();
                            const std::string account(96, 'a');
                            session->player->setID(account);
                            session->accepted.setPlayerID(account);
                            session->refused.setPlayerID(account);
                            session->player->loginAccountOwnership().acquire(account, [] { return true; });
                            unsigned cleanups = 0;
                            bool sent = false;
                            int flushed = -1;
                            session->actions.send = [&](LoginPlayer&, LCReconnect& packet) {
                                SocketOutputStream wire(nullptr, 64);
                                packet.write(wire);
                                sent = true;
                            };
                            session->actions.retirement.disconnect = [&](LoginPlayer& player, bool flush) {
                                ++cleanups;
                                flushed = flush;
                                std::string statement(160, 's');
                                if (statement.size() != 160)
                                    std::_Exit(1);
                                player.loginAccountOwnership().release([](const std::string&) {});
                            };
                            session->actions.retirement.report = [](const std::exception_ptr&) {
                                std::string diagnostic(128, 'd');
                                if (diagnostic.size() != 128)
                                    std::_Exit(2);
                                throw std::bad_alloc{};
                            };
                            const de::LoginRetirementActions release{+[](LoginPlayer& player, bool) {
                                                                         player.loginAccountOwnership().release(
                                                                             [](const std::string&) {});
                                                                     },
                                                                     {}};
                            AllocationProbe probe(failAt);
                            bool handled = false;
                            bool threw = false;
                            try {
                                handled = dispatch(*session, accepted);
                            } catch (const std::bad_alloc&) {
                                threw = true;
                            }
                            probe.stopFailing();
                            if (threw) {
                                if (session->players.size() != 1 || cleanups != 0 || sent || probe.outstanding() != 0 ||
                                    !session->player->loginAccountOwnership().owns(account))
                                    std::_Exit(3);
                                handled = dispatch(*session, accepted);
                            }
                            if (!handled || session->players.size() != 0 || cleanups != 1 ||
                                flushed != (accepted ? sent : true) || ::fcntl(session->descriptor, F_GETFD) != -1)
                                std::_Exit(4);
                            session->players.retryRetiredPlayers(session->now + 5s, release);
                            if (session->players.pendingRetirements() != 0)
                                std::_Exit(5);
                            session.reset();
                            const bool intact = probe.outstanding() == 0 && (failAt != 24 || !probe.rejected());
                            std::_Exit(intact ? 0 : 6);
                        }()),
                        ::testing::ExitedWithCode(0), "");
        }
    }
}

} // namespace
