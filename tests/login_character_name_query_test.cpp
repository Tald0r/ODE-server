#include <unistd.h>

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

#include "CLQueryCharacterName.h"
#include "DatabaseError.h"
#include "GameWorldInfoManager.h"
#include "LCQueryResultCharacterName.h"
#include "LoginCharacterNameQuery.h"
#include "LoginPlayer.h"
#include "ServerContext.h"
#include "Socket.h"
#include "SocketOutputStream.h"
#include "repository/ServerInfoRepository.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginCharacterRepository.h"

namespace {

class Worlds : public ServerInfoRepository {
public:
    bool loadMaxServerGroupID(int&) override {
        return false;
    }
    bool loadMaxWorldID(int&) override {
        return false;
    }
    std::vector<ServerInfoRow> loadServers() override {
        return {};
    }
    std::vector<ServerInfoNonPKRow> loadNonPKServers() override {
        return {};
    }
    std::vector<ServerInfoCastleStatRow> loadCastleStats() override {
        return {};
    }
    std::vector<ServerInfoWorldRow> loadWorlds() override {
        return rows;
    }
    std::vector<ServerInfoWorldRow> rows{{1, "First", WORLD_OPEN}, {7, "Sparse", WORLD_OPEN}};
};

using Query = std::pair<int, std::string>;
using Reply = std::pair<std::string, bool>;

class Characters : public FakeLoginCharacterRepository {
public:
    bool slayerNameExists(WorldID_t world, const std::string& name) override {
        queries.emplace_back(world, name);
        if (before)
            before();
        if (failure)
            std::rethrow_exception(failure);
        return FakeLoginCharacterRepository::slayerNameExists(world, name);
    }
    std::vector<Query> queries;
    std::function<void()> before;
    std::exception_ptr failure;
};

class QueryPlayer : public LoginPlayer {
public:
    QueryPlayer() : LoginPlayer(new Socket("198.51.100.7", 1234)) {
        setID("account");
        setWorldID(1);
        setServerGroupID(9);
        setPlayerStatus(LPS_PC_MANAGEMENT);
        loginAccountOwnership().acquire("account", [] { return true; });
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

struct Session {
    Session() {
        worlds.load(rows);
        packet.setCharacterName("Rowan");
    }
    void query() {
        de::queryLoginCharacterName(player, packet, worlds, characters, actions);
    }
    Worlds rows;
    GameWorldInfoManager worlds;
    Characters characters;
    QueryPlayer player;
    CLQueryCharacterName packet;
    std::vector<Reply> replies;
    de::LoginCharacterNameQueryActions actions{[&](LoginPlayer&, LCQueryResultCharacterName& reply) {
        replies.emplace_back(reply.getCharacterName(), reply.isExist());
    }};
};

TEST(LoginCharacterNameQuery, AConfiguredSparseWorldIsAdmittedByItsID) {
    Session session;
    session.player.setWorldID(7);
    EXPECT_NO_THROW(session.query());
    EXPECT_EQ(session.characters.queries, (std::vector<Query>{{7, "Rowan"}}));
    EXPECT_EQ(session.replies, (std::vector<Reply>{{"Rowan", false}}));
}

TEST(LoginCharacterNameQuery, MissingWorldsBelowTheCountAndAnEmptyWorldZeroFailBeforeLookup) {
    for (int scenario = 0; scenario < 3; ++scenario) {
        Session session;
        session.player.setWorldID(scenario == 0 ? 2 : 0);
        if (scenario == 2) {
            session.rows.rows.clear();
            session.worlds.load(session.rows);
        }
        EXPECT_THROW(session.query(), AssertionError);
        EXPECT_TRUE(session.characters.queries.empty());
        EXPECT_TRUE(session.replies.empty());
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_PC_MANAGEMENT);
    }
}

TEST(LoginCharacterNameQuery, ARepositoryCallbackCannotChangeTheReplyNameOrReservedNamePolicy) {
    for (bool reserved : {false, true}) {
        Session session;
        const std::string original = reserved ? "GM" : "Rowan";
        session.packet.setCharacterName(original);
        session.characters.before = [&] { session.packet.setCharacterName(reserved ? "Rowan" : "GM"); };
        session.query();
        EXPECT_EQ(session.characters.queries, (std::vector<Query>{{1, original}}));
        EXPECT_EQ(session.replies, (std::vector<Reply>{{original, reserved}}));
    }
}

TEST(LoginCharacterNameQuery, BoundaryAndClosedWorldsAreQueriedWhenConfigured) {
    Session session;
    session.rows.rows = {{0, "Zero", WORLD_OPEN}, {255, "Last", WORLD_CLOSE}};
    session.worlds.load(session.rows);
    for (const int world : {0, 255}) {
        session.player.setWorldID(world);
        session.query();
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
    }
    EXPECT_EQ(session.characters.queries, (std::vector<Query>{{0, "Rowan"}, {255, "Rowan"}}));
    EXPECT_EQ(session.replies, (std::vector<Reply>{{"Rowan", false}, {"Rowan", false}}));
}

TEST(LoginCharacterNameQuery, OneLookupPrecedesTheCaseSensitiveReservedNamePolicy) {
    const Reply cases[] = {{"Rowan", false},
                           {"NONE", true},
                           {"xGMaster", true},
                           {"none", false},
                           {"gm", false},
                           // UTF-8 for the reserved Korean operations title.
                           {"\xec\x9a\xb4\xec\x98\x81", true}};
    for (const auto& [name, reserved] : cases) {
        for (bool exists : {false, true}) {
            Session session;
            session.packet.setCharacterName(name);
            if (exists)
                session.characters.existingNames.insert(name);
            session.actions.send = [&](LoginPlayer& player, LCQueryResultCharacterName& reply) {
                EXPECT_EQ(&player, &session.player);
                EXPECT_EQ(player.getPlayerStatus(), LPS_PC_MANAGEMENT);
                EXPECT_EQ(session.characters.slayerNameExistsCalls, 1);
                EXPECT_EQ(reply.getCharacterName(), name);
                EXPECT_EQ(reply.isExist(), exists || reserved);
            };
            session.query();
            EXPECT_EQ(session.characters.queries, (std::vector<Query>{{1, name}}));
            EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
            EXPECT_TRUE(session.player.loginAccountOwnership().owns("account"));
            EXPECT_EQ(session.player.getID(), "account");
            EXPECT_EQ(session.player.getServerGroupID(), 9);
        }
    }
}

TEST(LoginCharacterNameQuery, CatalogueReplacementIsObservedOnTheNextQuery) {
    Session session;
    session.characters.before = [&] {
        session.rows.rows = {{9, "Replacement", WORLD_OPEN}};
        session.worlds.load(session.rows);
    };
    session.query();
    session.characters.before = {};
    EXPECT_THROW(session.query(), AssertionError);
    EXPECT_EQ(session.characters.queries.size(), 1u);
    EXPECT_EQ(session.replies.size(), 1u);
    session.player.setWorldID(9);
    session.query();
    EXPECT_EQ(session.characters.queries, (std::vector<Query>{{1, "Rowan"}, {9, "Rowan"}}));
    EXPECT_EQ(session.replies, (std::vector<Reply>{{"Rowan", false}, {"Rowan", false}}));
}

TEST(LoginCharacterNameQuery, RepositoryAndSendFailuresRetainTheirIdentityAndTheOriginalPhase) {
    const std::exception_ptr failures[] = {std::make_exception_ptr(DatabaseError("query failed")),
                                           std::make_exception_ptr(NoSuchElementException("missing world")),
                                           std::make_exception_ptr(Error("operation failed")),
                                           std::make_exception_ptr(DisconnectException("send failed")),
                                           std::make_exception_ptr(std::runtime_error("standard failure")),
                                           std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        for (bool duringSend : {false, true}) {
            for (bool reserved : {false, true}) {
                Session session;
                session.player.setPlayerStatus(LPS_WAITING_FOR_CL_REGISTER_PLAYER);
                if (reserved)
                    session.packet.setCharacterName("GM");
                unsigned sends = 0;
                if (!duringSend)
                    session.characters.failure = failure;
                session.actions.send = [&](LoginPlayer&, LCQueryResultCharacterName&) {
                    ++sends;
                    std::rethrow_exception(failure);
                };
                try {
                    session.query();
                    FAIL() << "failure was swallowed";
                } catch (...) {
                    EXPECT_EQ(std::current_exception(), failure);
                }
                EXPECT_EQ(session.characters.queries.size(), 1u);
                EXPECT_EQ(sends, duringSend ? 1u : 0u);
                EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_REGISTER_PLAYER);
                EXPECT_TRUE(session.player.loginAccountOwnership().owns("account"));
                EXPECT_EQ(session.player.getID(), "account");
            }
        }
    }
}

TEST(LoginCharacterNameQuery, AFailedSendRetainsItsPartialOutputAndAllowsAnotherQuery) {
    Session session;
    session.actions.send = [&](LoginPlayer& player, LCQueryResultCharacterName& reply) {
        player.sendPacket(&reply);
        throw DisconnectException("failure after buffering");
    };
    EXPECT_THROW(session.query(), DisconnectException);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_PC_MANAGEMENT);
    const auto first = session.player.bufferedBytes();
    EXPECT_EQ(first.size(), szPacketHeader + 7);
    EXPECT_TRUE(session.player.loginAccountOwnership().owns("account"));
    session.actions = de::defaultLoginCharacterNameQueryActions();
    session.query();
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
    EXPECT_EQ(session.player.bufferedBytes().size(), 2 * first.size());
    EXPECT_EQ(session.characters.queries.size(), 2u);
}

TEST(LoginCharacterNameQuery, DefaultSendingPreservesBoundaryNameBytesAndPublishesAfterward) {
    for (const std::size_t length : {1u, 20u}) {
        for (bool exists : {false, true}) {
            Session session;
            const std::string name(length, 'n');
            session.packet.setCharacterName(name);
            if (exists)
                session.characters.existingNames.insert(name);
            session.actions = de::defaultLoginCharacterNameQueryActions();
            session.player.observe = [&](Packet& reply) {
                EXPECT_EQ(reply.getPacketID(), Packet::PACKET_LC_QUERY_RESULT_CHARACTER_NAME);
                EXPECT_EQ(session.player.getPlayerStatus(), LPS_PC_MANAGEMENT);
                EXPECT_TRUE(session.player.loginAccountOwnership().owns("account"));
            };
            session.query();
            const auto bytes = session.player.bufferedBytes();
            std::string expected(1, static_cast<char>(length));
            expected += name;
            expected.push_back(exists ? '\1' : '\0');
            EXPECT_EQ(bytes.substr(szPacketHeader), expected);
            EXPECT_EQ(session.player.sent, 1u);
            EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
            EXPECT_EQ(session.packet.getCharacterName(), name);
        }
    }
}

int checkProductionGuard() {
    char directory[] = "/tmp/darkeden-name-query-XXXXXX";
    if (!mkdtemp(directory) || chdir(directory) != 0)
        return 1;
    Session session;
    session.player.setWorldID(2);
    auto* previous = de::serverContext().exchangeGameWorldInfoManager(&session.worlds);
    bool guarded = false;
    try {
        CLQueryCharacterNameHandler::execute(&session.packet, &session.player);
    } catch (const AssertionError& error) {
        guarded = error.getMessage().find("queryLoginCharacterName") != std::string::npos &&
                  error.getMessage().find("contains(worldID)") != std::string::npos;
    } catch (...) {
    }
    de::serverContext().exchangeGameWorldInfoManager(previous);
    const bool logged = access("assertion_failed.log", F_OK) == 0;
    unlink("assertion_failed.log");
    chdir("/");
    rmdir(directory);
    return guarded && logged && session.player.sent == 0 && session.player.getPlayerStatus() == LPS_PC_MANAGEMENT &&
                   session.player.loginAccountOwnership().owns("account")
               ? 0
               : 2;
}

TEST(LoginCharacterNameQuery, TheProductionHandlerRejectsMissingWorldsBeforeReachingTheDatabase) {
    ASSERT_EXIT(std::_Exit(checkProductionGuard()), ::testing::ExitedWithCode(0), "");
}

int checkAllocationFailure(std::size_t position, bool missingWorld) {
    auto session = std::make_unique<Session>();
    session->packet.setCharacterName(std::string(20, 'n'));
    if (missingWorld)
        session->player.setWorldID(2);
    unsigned sent = 0;
    session->actions.send = [&](LoginPlayer&, LCQueryResultCharacterName& reply) {
        SocketOutputStream output(nullptr, 64);
        reply.write(output);
        ++sent;
    };
    AllocationProbe probe(position);
    bool failed = false;
    bool guarded = false;
    try {
        session->query();
    } catch (const std::bad_alloc&) {
        failed = true;
    } catch (const AssertionError&) {
        guarded = true;
    } catch (...) {
        return 1;
    }
    probe.stopFailing();
    if ((!missingWorld && (failed != probe.rejected() || guarded)) || (position == 32 && probe.rejected()))
        return 2;
    if (missingWorld && ((!failed && !guarded) || sent || !session->characters.queries.empty()))
        return 3;
    if (!session->player.loginAccountOwnership().owns("account") ||
        session->player.getPlayerStatus() != (sent ? LPS_WAITING_FOR_CL_GET_PC_LIST : LPS_PC_MANAGEMENT))
        return 4;
    if (failed || guarded) {
        session->player.setWorldID(1);
        session->query();
        if (sent != 1 || session->player.getPlayerStatus() != LPS_WAITING_FOR_CL_GET_PC_LIST)
            return 5;
    }
    session.reset();
    return probe.outstanding() == 0 ? 0 : 6;
}

TEST(LoginCharacterNameQuery, AllocationFailuresPreserveTheSessionAndReleasePreparationBeforeRetry) {
    for (bool missingWorld : {false, true}) {
        for (std::size_t position = 1; position <= 32; ++position) {
            SCOPED_TRACE(::testing::Message() << missingWorld << "/" << position);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure(position, missingWorld)), ::testing::ExitedWithCode(0), "");
        }
    }
}

} // namespace
