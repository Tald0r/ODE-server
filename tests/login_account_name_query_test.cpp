#include <cstdlib>
#include <exception>
#include <functional>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "CLQueryPlayerID.h"
#include "DatabaseError.h"
#include "LCQueryResultPlayerID.h"
#include "LoginAccountNameQuery.h"
#include "LoginPlayer.h"
#include "Socket.h"
#include "SocketOutputStream.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginAccountRepository.h"

namespace {

class Accounts : public FakeLoginAccountRepository {
public:
    bool accountNameExists(const std::string& name) override {
        queries.push_back(name);
        if (before)
            before();
        if (failure)
            std::rethrow_exception(failure);
        return FakeLoginAccountRepository::accountNameExists(name);
    }
    std::vector<std::string> queries;
    std::function<void()> before;
    std::exception_ptr failure;
};

class QueryPlayer : public LoginPlayer {
public:
    QueryPlayer() : LoginPlayer(new Socket("198.51.100.7", 1234)) {
        setID("visitor");
        setWorldID(7);
        setServerGroupID(9);
        setPlayerStatus(LPS_BEGIN_SESSION);
    }
    ~QueryPlayer() override {
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

using Reply = std::pair<std::string, bool>;

struct Session {
    Session() {
        packet.setPlayerID("Rowan");
    }
    void query() {
        de::queryLoginAccountName(player, packet, accounts, actions);
    }
    Accounts accounts;
    QueryPlayer player;
    CLQueryPlayerID packet;
    std::vector<Reply> replies;
    de::LoginAccountNameQueryActions actions{[&](LoginPlayer&, LCQueryResultPlayerID& reply) {
        replies.emplace_back(reply.getPlayerID(), reply.isExist());
    }};
};

TEST(LoginAccountNameQuery, ARepositoryCallbackCannotChangeTheReplySubject) {
    Session session;
    session.accounts.registeredNames.insert("Rowan");
    session.accounts.before = [&] { session.packet.setPlayerID("Willow"); };
    session.query();
    EXPECT_EQ(session.accounts.queries, (std::vector<std::string>{"Rowan"}));
    EXPECT_EQ(session.replies, (std::vector<Reply>{{"Rowan", true}}));
}

void expectIdentity(const Session& session, bool owned) {
    EXPECT_EQ(session.player.getID(), "visitor");
    EXPECT_EQ(session.player.getWorldID(), 7);
    EXPECT_EQ(session.player.getServerGroupID(), 9);
    EXPECT_EQ(session.player.loginAccountOwnership().owns("visitor"), owned);
    EXPECT_EQ(session.accounts.accountExistsCalls, 0);
    EXPECT_EQ(session.accounts.loadAccountCalls, 0);
    EXPECT_TRUE(session.accounts.insertedAccounts.empty());
    EXPECT_TRUE(session.accounts.markedLoggedOn.empty());
    EXPECT_TRUE(session.accounts.markedLoggedOnAfterRegister.empty());
}

TEST(LoginAccountNameQuery, AvailabilityUsesTheNameProbeWithoutCharacterNameRestrictionsOrAccountWrites) {
    for (const std::string name : {"Rowan", "GM", "NONE", "none"}) {
        for (bool exists : {false, true}) {
            Session session;
            session.packet.setPlayerID(name);
            if (exists)
                session.accounts.registeredNames.insert(name);
            else
                session.accounts.registeredIDs.insert(name);
            session.query();
            EXPECT_EQ(session.accounts.queries, (std::vector<std::string>{name}));
            EXPECT_EQ(session.replies, (std::vector<Reply>{{name, exists}}));
            EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_REGISTER_PLAYER);
            expectIdentity(session, false);
        }
    }
}

TEST(LoginAccountNameQuery, RepeatedQueriesReadCurrentRepositoryResultsWithoutAcquiringTheRequestedAccount) {
    Session session;
    session.player.setPlayerStatus(LPS_WAITING_FOR_CL_REGISTER_PLAYER);
    session.query();
    session.accounts.registeredNames.insert("Rowan");
    session.query();
    session.packet.setPlayerID("Willow");
    session.query();
    EXPECT_EQ(session.accounts.queries, (std::vector<std::string>{"Rowan", "Rowan", "Willow"}));
    EXPECT_EQ(session.replies, (std::vector<Reply>{{"Rowan", false}, {"Rowan", true}, {"Willow", false}}));
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_REGISTER_PLAYER);
    expectIdentity(session, false);
}

TEST(LoginAccountNameQuery, ThePhaseChangesOnlyAfterTheSenderReturnsAndRetainsExistingOwnership) {
    Session session;
    session.player.loginAccountOwnership().acquire("visitor", [] { return true; });
    session.accounts.before = [&] {
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
        expectIdentity(session, true);
    };
    unsigned sends = 0;
    session.actions.send = [&](LoginPlayer& player, LCQueryResultPlayerID& reply) {
        EXPECT_EQ(&player, &session.player);
        EXPECT_EQ(session.accounts.queries, (std::vector<std::string>{"Rowan"}));
        EXPECT_EQ(reply.getPlayerID(), "Rowan");
        EXPECT_EQ(player.getPlayerStatus(), LPS_BEGIN_SESSION);
        expectIdentity(session, true);
        ++sends;
    };
    session.query();
    EXPECT_EQ(sends, 1u);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_REGISTER_PLAYER);
    expectIdentity(session, true);
}

TEST(LoginAccountNameQuery, LookupAndBothSenderBoundariesPropagateFailuresWithoutChangingTheSession) {
    const std::exception_ptr failures[] = {std::make_exception_ptr(DatabaseError("lookup failed")),
                                           std::make_exception_ptr(NoSuchElementException("missing result")),
                                           std::make_exception_ptr(Error("operation failed")),
                                           std::make_exception_ptr(DisconnectException("send failed")),
                                           std::make_exception_ptr(std::runtime_error("standard failure")),
                                           std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        for (int stage = 0; stage < 3; ++stage) {
            for (bool owned : {false, true}) {
                Session session;
                if (owned)
                    session.player.loginAccountOwnership().acquire("visitor", [] { return true; });
                unsigned sends = 0;
                if (stage == 0)
                    session.accounts.failure = failure;
                if (stage == 2) {
                    session.actions = de::defaultLoginAccountNameQueryActions();
                    session.player.observe = [&](Packet&) {
                        ++sends;
                        std::rethrow_exception(failure);
                    };
                } else {
                    session.actions.send = [&](LoginPlayer&, LCQueryResultPlayerID&) {
                        ++sends;
                        std::rethrow_exception(failure);
                    };
                }
                try {
                    session.query();
                    FAIL() << "failure was swallowed";
                } catch (...) {
                    EXPECT_EQ(std::current_exception(), failure);
                }
                EXPECT_EQ(session.accounts.queries, (std::vector<std::string>{"Rowan"}));
                EXPECT_EQ(sends, stage == 0 ? 0u : 1u);
                EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
                EXPECT_TRUE(session.player.bufferedBytes().empty());
                expectIdentity(session, owned);
            }
        }
    }
}

TEST(LoginAccountNameQuery, AFailedSendKeepsBufferedOutputAndTheCallerCanQueryAgain) {
    Session session;
    session.actions.send = [&](LoginPlayer& player, LCQueryResultPlayerID& reply) {
        player.sendPacket(&reply);
        throw DisconnectException("failure after buffering");
    };
    EXPECT_THROW(session.query(), DisconnectException);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
    const auto first = session.player.bufferedBytes();
    EXPECT_EQ(first.size(), szPacketHeader + 7);
    expectIdentity(session, false);
    session.actions = de::defaultLoginAccountNameQueryActions();
    session.query();
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_REGISTER_PLAYER);
    EXPECT_EQ(session.player.bufferedBytes().size(), 2 * first.size());
    EXPECT_EQ(session.accounts.queries.size(), 2u);
}

TEST(LoginAccountNameQuery, TheProductionSenderPreservesBoundaryNameBytesForBothResults) {
    for (const std::size_t length : {1u, 20u}) {
        for (bool exists : {false, true}) {
            Session session;
            const std::string name(length, 'n');
            session.packet.setPlayerID(name);
            if (exists)
                session.accounts.registeredNames.insert(name);
            session.actions = de::defaultLoginAccountNameQueryActions();
            session.player.observe = [&](Packet& reply) {
                EXPECT_EQ(reply.getPacketID(), Packet::PACKET_LC_QUERY_RESULT_PLAYER_ID);
                EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
            };
            session.query();
            std::string expected(1, static_cast<char>(length));
            expected += name;
            expected.push_back(exists ? '\1' : '\0');
            EXPECT_EQ(session.player.bufferedBytes().substr(szPacketHeader), expected);
            EXPECT_EQ(session.player.sent, 1u);
            EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_REGISTER_PLAYER);
            EXPECT_EQ(session.packet.getPlayerID(), name);
            expectIdentity(session, false);
        }
    }
}

int checkAllocationFailure(std::size_t position, bool exists, bool owned) {
    auto session = std::make_unique<Session>();
    const std::string name(20, 'n');
    session->packet.setPlayerID(name);
    if (exists)
        session->accounts.registeredNames.insert(name);
    if (owned)
        session->player.loginAccountOwnership().acquire("visitor", [] { return true; });
    unsigned sent = 0;
    bool correctReply = false;
    session->actions.send = [&](LoginPlayer&, LCQueryResultPlayerID& reply) {
        SocketOutputStream output(nullptr, 64);
        reply.write(output);
        correctReply = reply.getPlayerID() == name && reply.isExist() == exists;
        ++sent;
    };
    AllocationProbe probe(position);
    bool failed = false;
    try {
        session->query();
    } catch (const std::bad_alloc&) {
        failed = true;
    } catch (...) {
        return 1;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (position == 24 && failed) ||
        session->player.loginAccountOwnership().owns("visitor") != owned)
        return 2;
    if (session->player.getPlayerStatus() != (sent ? LPS_WAITING_FOR_CL_REGISTER_PLAYER : LPS_BEGIN_SESSION) ||
        (sent && (!correctReply || failed)))
        return 3;
    if (failed)
        session->query();
    if (sent != 1 || !correctReply || session->player.getPlayerStatus() != LPS_WAITING_FOR_CL_REGISTER_PLAYER)
        return 4;
    session.reset();
    return probe.outstanding() == 0 ? 0 : 5;
}

TEST(LoginAccountNameQuery, AllocationFailuresReleasePreparationAndRetainOwnershipForRetry) {
    for (bool exists : {false, true}) {
        for (bool owned : {false, true}) {
            for (std::size_t position = 1; position <= 24; ++position) {
                SCOPED_TRACE(::testing::Message() << exists << "/" << owned << "/" << position);
                ASSERT_EXIT(std::_Exit(checkAllocationFailure(position, exists, owned)), ::testing::ExitedWithCode(0),
                            "");
            }
        }
    }
}

} // namespace
