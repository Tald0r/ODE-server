//////////////////////////////////////////////////////////////////////////////
// Filename    : gm_command_router_test.cpp
// Description : The GM chat command tables (docs/reviews/RESTRUCTURING.md 4.1): every
//               registered command declares a permission level, the names are
//               unique, a name matches as a prefix of the message, a caller
//               below the level runs nothing, and an unknown name runs nothing
//               either. The relay table - the commands another game server may
//               send - is pinned the same way.
//
//               The production tables are the subject, not a copy of them:
//               this links the real registration and stubs the command bodies,
//               so a name, a gate or an order that changes there fails here.
//               The expected table below is the source of truth for the gates;
//               changing one means editing this file.
//////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "gm/GMCommands.h"

using de::gm::Command;
using de::gm::CommandContext;
using de::gm::CommandRouter;
using de::gm::Permission;

namespace {

struct Call {
    std::string name;
    std::string msg;
    int i;
};

std::vector<Call>& calls() {
    static std::vector<Call> recorded;
    return recorded;
}

void record(const char* name, const std::string& msg, int i) {
    calls().push_back(Call{name, msg, i});
}

// Runs a message against a table as a caller of the given level, and answers
// the name of the command body that ran, "" for none. Neither a creature nor
// a player is needed: the bodies are stubbed below.
std::string run(const CommandRouter& router, const std::string& msg, Permission caller, bool* dispatched = nullptr) {
    calls().clear();
    const std::string::size_type star = msg.find_first_of('*', 0);
    const CommandContext context{nullptr, nullptr, msg, static_cast<int>(star), caller};
    const bool ran = router.dispatch(context);
    if (dispatched != nullptr)
        *dispatched = ran;
    if (calls().empty())
        return "";
    return calls().front().name;
}

CommandRouter operatorTable() {
    CommandRouter router;
    de::gm::registerOperatorCommands(router);
    return router;
}

CommandRouter broadcastTable() {
    CommandRouter router;
    de::gm::registerBroadcastCommands(router);
    return router;
}

CommandRouter relayTable() {
    CommandRouter router;
    de::gm::registerRelayCommands(router);
    return router;
}

// The three names the chat ladder compared against that the move off the
// legacy code pages turned into mojibake. Each is longer than the number of
// characters it is compared over, so none can ever match.
const std::string kFindAlias = "\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd";
const std::string kOpenPayMapAlias = "\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xd5\xb7\xd1\xb5\xef\xbf\xbd\xcd\xbc";
const std::string kClosePayMapAlias =
    "\xef\xbf\xbd\xd8\xb1\xef\xbf\xbd\xef\xbf\xbd\xd5\xb7\xd1\xb5\xef\xbf\xbd\xcd\xbc";

struct Expected {
    std::string name;
    std::string::size_type matchLength;
    Permission permission;
    std::string body;
};

// The operator commands, in the order a message is matched against them.
std::vector<Expected> expectedOperatorCommands() {
    return {
        {"save", 4, Permission::DM, "opsave"},
        {"wall", 4, Permission::Helper, "opwall"},
        {"shutdown", 8, Permission::God, "opshutdown"},
        {"pay", 3, Permission::God, "oppay"},
        {"kick", 4, Permission::Helper, "opkick"},
        {"mute", 4, Permission::God, "opmute"},
        {"denychat", 8, Permission::Helper, "opdenychat"},
        {"freezing", 8, Permission::God, "opfreezing"},
        {"deny", 4, Permission::Helper, "opdeny"},
        {"info", 4, Permission::God, "opinfo"},
        {"trace", 5, Permission::Helper, "optrace"},
        {"warp", 4, Permission::Helper, "opwarp"},
        {"create", 6, Permission::God, "opcreate"},
        {"summon", 6, Permission::God, "opsummon"},
        {"grant", 5, Permission::God, "opgrant"},
        {"command", 7, Permission::God, "opcommand"},
        {"fun", 3, Permission::God, "opfun"},
        {"recall", 6, Permission::God, "oprecall"},
        {"user", 4, Permission::God, "opuser"},
        {"set", 3, Permission::God, "opset"},
        {"load", 4, Permission::God, "opload"},
        {"view", 4, Permission::God, "opview"},
        {"combat", 6, Permission::God, "opcombat"},
        // Ungated, as the ladder left it: the branch tested two names and
        // the gate bound to the second one alone.
        {"find", 4, Permission::God, "opfind"},
        {kFindAlias, 4, Permission::God, "opfind"},
        {"credit", 6, Permission::God, "opcredit"},
        {"soulchain", 9, Permission::God, "opsoulchain"},
        {"log", 3, Permission::God, "oplog"},
        {"bug_report", 10, Permission::God, "opbugreport"},
        {"CrashReport", 11, Permission::God, "opcrashreport"},
        {"OpenPayMap", 10, Permission::God, "opopenpaymap"},
        {kOpenPayMapAlias, 12, Permission::God, "opopenpaymap"},
        {"ClosePayMap", 11, Permission::God, "opclosepaymap"},
        {kClosePayMapAlias, 12, Permission::God, "opclosepaymap"},
    };
}

// The prefixes answered before the operator commands.
std::vector<Expected> expectedBroadcastCommands() {
    return {
        {"zone", 4, Permission::God, "opzone"},
        {"guild", 5, Permission::God, "opguild"},
        {"world", 5, Permission::God, "opworld"},
        {"allworld", 8, Permission::God, "opworld"},
    };
}

// The commands another game server may relay, in the order a relayed message
// is matched against them. A relayed message carries no player and no
// creature, so every body named here is correct without one.
std::vector<Expected> expectedRelayCommands() {
    return {
        {"save", 4, Permission::God, "opsave"},
        {"wall", 4, Permission::God, "opwall"},
        {"shutdown", 8, Permission::God, "opshutdown"},
        {"kick", 4, Permission::God, "opkick"},
        {"mute", 4, Permission::God, "opmute"},
        {"freezing", 8, Permission::God, "opfreezing"},
        {"set", 3, Permission::God, "opset"},
        {"load", 4, Permission::God, "opload"},
        {"combat", 6, Permission::God, "opcombat"},
        {"command", 7, Permission::God, "opcommand"},
        {"modifyunioninfo", 15, Permission::God, "opmodifyunioninfo"},
        {"refreshguildunion", 17, Permission::God, "oprefreshguildunion"},
    };
}

} // namespace

//////////////////////////////////////////////////////////////////////////////
// The command bodies, stubbed. The gameserver's own definitions need a zone,
// an inventory and a database; what is under test is which of them the table
// picks, so each one only says that it ran.
//////////////////////////////////////////////////////////////////////////////

namespace de::gm {

void opsave(GamePlayer*, std::string msg, int i) {
    record("opsave", msg, i);
}
void opwall(GamePlayer*, std::string msg, int i) {
    record("opwall", msg, i);
}
void opshutdown(GamePlayer*, std::string msg, int i) {
    record("opshutdown", msg, i);
}
void oppay(GamePlayer*, std::string msg, int i) {
    record("oppay", msg, i);
}
void opkick(GamePlayer*, std::string msg, int i) {
    record("opkick", msg, i);
}
void opmute(GamePlayer*, std::string msg, int i) {
    record("opmute", msg, i);
}
void opdenychat(GamePlayer*, std::string msg, int i) {
    record("opdenychat", msg, i);
}
void opfreezing(GamePlayer*, std::string msg, int i) {
    record("opfreezing", msg, i);
}
void opdeny(GamePlayer*, std::string msg, int i) {
    record("opdeny", msg, i);
}
void opinfo(GamePlayer*, std::string msg, int i) {
    record("opinfo", msg, i);
}
void optrace(GamePlayer*, std::string msg, int i) {
    record("optrace", msg, i);
}
void opwarp(GamePlayer*, std::string msg, int i) {
    record("opwarp", msg, i);
}
void opcreate(GamePlayer*, std::string msg, int i) {
    record("opcreate", msg, i);
}
void opsummon(GamePlayer*, std::string msg, int i) {
    record("opsummon", msg, i);
}
void opgrant(GamePlayer*, std::string msg, int i) {
    record("opgrant", msg, i);
}
void opcommand(GamePlayer*, std::string msg, int i) {
    record("opcommand", msg, i);
}
void opfun(GamePlayer*, std::string msg, int i) {
    record("opfun", msg, i);
}
void oprecall(GamePlayer*, std::string msg, int i) {
    record("oprecall", msg, i);
}
void opuser(GamePlayer*, std::string msg, int i) {
    record("opuser", msg, i);
}
void opset(GamePlayer*, std::string msg, int i) {
    record("opset", msg, i);
}
void opload(GamePlayer*, std::string msg, int i) {
    record("opload", msg, i);
}
void opview(GamePlayer*, std::string msg, int i) {
    record("opview", msg, i);
}
void opcombat(GamePlayer*, std::string msg, int i) {
    record("opcombat", msg, i);
}
void opfind(GamePlayer*, std::string msg, int i) {
    record("opfind", msg, i);
}
void opcredit(GamePlayer*, std::string msg, int i) {
    record("opcredit", msg, i);
}
void opsoulchain(GamePlayer*, std::string msg, int i) {
    record("opsoulchain", msg, i);
}
void oplog(GamePlayer*, std::string msg, int i) {
    record("oplog", msg, i);
}
void opbugreport(Creature*, GamePlayer*, std::string msg, int i) {
    record("opbugreport", msg, i);
}
void opcrashreport(Creature*, GamePlayer*, std::string msg, int i) {
    record("opcrashreport", msg, i);
}
void opopenpaymap(GamePlayer*, std::string msg, int i) {
    record("opopenpaymap", msg, i);
}
void opclosepaymap(GamePlayer*, std::string msg, int i) {
    record("opclosepaymap", msg, i);
}
void opzone(std::string msg, int i) {
    record("opzone", msg, i);
}
void opguild(std::string msg, int i, Creature*) {
    record("opguild", msg, i);
}
void opworld(GamePlayer*, std::string msg, int i, bool) {
    record("opworld", msg, i);
}
void opmodifyunioninfo(GamePlayer*, std::string msg, int i, bool) {
    record("opmodifyunioninfo", msg, i);
}
void oprefreshguildunion(GamePlayer*, std::string msg, int i, bool) {
    record("oprefreshguildunion", msg, i);
}

} // namespace de::gm

//////////////////////////////////////////////////////////////////////////////
// What every registered command owes
//////////////////////////////////////////////////////////////////////////////

TEST(GMCommandRouter, EveryRegisteredCommandDeclaresAPermissionLevel) {
    for (const CommandRouter& router : {operatorTable(), broadcastTable(), relayTable()}) {
        EXPECT_FALSE(router.commands().empty());
        for (const Command& command : router.commands()) {
            EXPECT_FALSE(command.name.empty());
            EXPECT_GT(command.matchLength, 0u) << command.name;
            EXPECT_TRUE(command.permission == Permission::God || command.permission == Permission::DM ||
                        command.permission == Permission::Helper || command.permission == Permission::Everyone)
                << command.name;
            EXPECT_NE(command.handler, nullptr) << command.name;
        }
    }
}

TEST(GMCommandRouter, CommandNamesAreUnique) {
    for (const CommandRouter& router : {operatorTable(), broadcastTable(), relayTable()}) {
        std::vector<std::string> seen;
        for (const Command& command : router.commands()) {
            EXPECT_EQ(std::count(seen.begin(), seen.end(), command.name), 0) << command.name;
            seen.push_back(command.name);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// The production tables
//////////////////////////////////////////////////////////////////////////////

TEST(GMCommandRouter, OperatorTableIsTheExpectedOne) {
    const CommandRouter router = operatorTable();
    const std::vector<Expected> expected = expectedOperatorCommands();

    ASSERT_EQ(router.commands().size(), expected.size());
    for (std::size_t n = 0; n < expected.size(); ++n) {
        EXPECT_EQ(router.commands()[n].name, expected[n].name) << "row " << n;
        EXPECT_EQ(router.commands()[n].matchLength, expected[n].matchLength) << expected[n].name;
        EXPECT_EQ(static_cast<int>(router.commands()[n].permission), static_cast<int>(expected[n].permission))
            << expected[n].name;
    }
}

TEST(GMCommandRouter, BroadcastTableIsTheExpectedOne) {
    const CommandRouter router = broadcastTable();
    const std::vector<Expected> expected = expectedBroadcastCommands();

    ASSERT_EQ(router.commands().size(), expected.size());
    for (std::size_t n = 0; n < expected.size(); ++n) {
        EXPECT_EQ(router.commands()[n].name, expected[n].name) << "row " << n;
        EXPECT_EQ(router.commands()[n].matchLength, expected[n].matchLength) << expected[n].name;
        EXPECT_EQ(static_cast<int>(router.commands()[n].permission), static_cast<int>(expected[n].permission))
            << expected[n].name;
    }
}

// Every row runs the body the table binds to it, for a caller that clears its
// gate. The two aliases are skipped: nothing can reach them.
TEST(GMCommandRouter, EveryReachableCommandRunsItsOwnBody) {
    const CommandRouter router = operatorTable();
    for (const Expected& expected : expectedOperatorCommands()) {
        if (expected.name.size() > expected.matchLength)
            continue;
        EXPECT_EQ(run(router, "*" + expected.name + " argument", expected.permission), expected.body);
    }
}

//////////////////////////////////////////////////////////////////////////////
// Gating
//////////////////////////////////////////////////////////////////////////////

TEST(GMCommandRouter, ACallerBelowTheLevelRunsNothing) {
    const CommandRouter router = operatorTable();

    // *shutdown is GOD only.
    EXPECT_EQ(run(router, "*shutdown 10", Permission::God), "opshutdown");
    EXPECT_EQ(run(router, "*shutdown 10", Permission::DM), "");
    EXPECT_EQ(run(router, "*shutdown 10", Permission::Helper), "");
    EXPECT_EQ(run(router, "*shutdown 10", Permission::Everyone), "");

    // *save takes a DM as well.
    EXPECT_EQ(run(router, "*save", Permission::God), "opsave");
    EXPECT_EQ(run(router, "*save", Permission::DM), "opsave");
    EXPECT_EQ(run(router, "*save", Permission::Helper), "");

    // *kick takes anything that is not a plain player.
    EXPECT_EQ(run(router, "*kick someone", Permission::Helper), "opkick");
    EXPECT_EQ(run(router, "*kick someone", Permission::Everyone), "");

    // *find is GOD-only, like the two pay-map commands.
    EXPECT_EQ(run(router, "*find someone", Permission::God), "opfind");
    EXPECT_EQ(run(router, "*find someone", Permission::DM), "");
    EXPECT_EQ(run(router, "*OpenPayMap", Permission::Everyone), "");
    EXPECT_EQ(run(router, "*ClosePayMap", Permission::DM), "");
}

TEST(GMCommandRouter, ARefusedCommandDoesNotAnswerTheMessage) {
    const CommandRouter router = operatorTable();

    bool dispatched = true;
    EXPECT_EQ(run(router, "*shutdown 10", Permission::Everyone, &dispatched), "");
    EXPECT_FALSE(dispatched);
}

// A name the caller may not run is passed over rather than refused, so a
// later name that also matches still gets its turn.
TEST(GMCommandRouter, ARefusedNameLetsALaterMatchAnswer) {
    CommandRouter router;
    router.add("deny", Permission::God, [](const CommandContext& context) { record("first", context.msg, context.i); });
    router.add("den", Permission::Everyone,
               [](const CommandContext& context) { record("second", context.msg, context.i); });

    EXPECT_EQ(run(router, "*deny someone", Permission::God), "first");
    EXPECT_EQ(run(router, "*deny someone", Permission::Everyone), "second");
}

//////////////////////////////////////////////////////////////////////////////
// Name matching
//////////////////////////////////////////////////////////////////////////////

// A name is compared against as many characters of the message as it is long,
// so anything following it is left for the body to read - including a longer
// word that happens to start with it.
TEST(GMCommandRouter, ANameMatchesAsAPrefix) {
    const CommandRouter router = operatorTable();

    EXPECT_EQ(run(router, "*save", Permission::God), "opsave");
    EXPECT_EQ(run(router, "*save world", Permission::God), "opsave");
    EXPECT_EQ(run(router, "*savex", Permission::God), "opsave");
    EXPECT_EQ(run(router, "*sav", Permission::God), "");
    EXPECT_EQ(run(router, "*", Permission::God), "");
}

// *denychat is registered before *deny, which is a prefix of it, so both
// names reach their own body.
TEST(GMCommandRouter, TheFirstOfTwoNamesSharingAPrefixWins) {
    const CommandRouter router = operatorTable();

    EXPECT_EQ(run(router, "*denychat someone", Permission::Helper), "opdenychat");
    EXPECT_EQ(run(router, "*deny someone", Permission::Helper), "opdeny");
}

TEST(GMCommandRouter, AnUnknownCommandRunsNothing) {
    const CommandRouter router = operatorTable();

    bool dispatched = true;
    EXPECT_EQ(run(router, "*nosuchcommand argument", Permission::God, &dispatched), "");
    EXPECT_FALSE(dispatched);

    EXPECT_EQ(run(router, "*zone 1013", Permission::God), "") << "a broadcast prefix is not an operator command";
}

// The mojibake aliases are compared over fewer characters than they carry, so
// even the text itself does not reach them.
TEST(GMCommandRouter, TheMojibakeAliasesCannotMatch) {
    const CommandRouter router = operatorTable();

    EXPECT_EQ(run(router, "*" + kFindAlias, Permission::God), "");
    EXPECT_EQ(run(router, "*" + kOpenPayMapAlias, Permission::God), "");
    EXPECT_EQ(run(router, "*" + kClosePayMapAlias, Permission::God), "");
}

//////////////////////////////////////////////////////////////////////////////
// The broadcast prefixes
//////////////////////////////////////////////////////////////////////////////

TEST(GMCommandRouter, BroadcastPrefixesAreGODOnly) {
    const CommandRouter router = broadcastTable();

    EXPECT_EQ(run(router, "*zone 1013", Permission::God), "opzone");
    EXPECT_EQ(run(router, "*zone 1013", Permission::DM), "");
    EXPECT_EQ(run(router, "*guild 1", Permission::God), "opguild");
    EXPECT_EQ(run(router, "*guild 1", Permission::Helper), "");
}

// *world and *allworld run the command they carry before relaying the whole
// message, so two bodies run for one message.
TEST(GMCommandRouter, TheWorldPrefixesRunTheCarriedCommandFirst) {
    const CommandRouter router = broadcastTable();

    calls().clear();
    const std::string msg = "*world *save now";
    const CommandContext context{nullptr, nullptr, msg, 0, Permission::God};
    EXPECT_TRUE(router.dispatch(context));

    ASSERT_EQ(calls().size(), 2u);
    EXPECT_EQ(calls()[0].name, "opsave");
    EXPECT_EQ(calls()[0].msg, "*save now") << "the carried command runs on its own text";
    EXPECT_EQ(calls()[1].name, "opworld");
    EXPECT_EQ(calls()[1].msg, msg) << "the whole message is relayed";
}

// A carried command the caller may not run relays all the same.
TEST(GMCommandRouter, TheWorldPrefixesRelayEvenWhenTheCarriedCommandIsRefused) {
    CommandRouter router;
    de::gm::registerBroadcastCommands(router);

    calls().clear();
    const std::string msg = "*allworld *nosuchcommand";
    const CommandContext context{nullptr, nullptr, msg, 0, Permission::God};
    EXPECT_TRUE(router.dispatch(context));

    ASSERT_EQ(calls().size(), 1u);
    EXPECT_EQ(calls()[0].name, "opworld");
}

//////////////////////////////////////////////////////////////////////////////
// The relay table
//////////////////////////////////////////////////////////////////////////////

TEST(GMRelayCommands, RelayTableIsTheExpectedOne) {
    const CommandRouter router = relayTable();
    const std::vector<Expected> expected = expectedRelayCommands();

    ASSERT_EQ(router.commands().size(), expected.size());
    for (std::size_t n = 0; n < expected.size(); ++n) {
        EXPECT_EQ(router.commands()[n].name, expected[n].name) << "row " << n;
        EXPECT_EQ(router.commands()[n].matchLength, expected[n].matchLength) << expected[n].name;
        EXPECT_EQ(static_cast<int>(router.commands()[n].permission), static_cast<int>(expected[n].permission))
            << expected[n].name;
    }
}

TEST(GMRelayCommands, EveryRelayCommandRunsItsOwnBody) {
    const CommandRouter router = relayTable();
    for (const Expected& expected : expectedRelayCommands())
        EXPECT_EQ(run(router, "*" + expected.name + " argument", Permission::God), expected.body);
}

// *world relays whatever a GOD typed after it, so any chat command at all can
// arrive as a relay. One the table does not name runs nothing.
TEST(GMRelayCommands, ACommandOutsideTheTableRunsNothing) {
    const CommandRouter router = relayTable();

    bool dispatched = true;
    EXPECT_EQ(run(router, "*warp 1013 10 10", Permission::God, &dispatched), "");
    EXPECT_FALSE(dispatched);

    EXPECT_EQ(run(router, "*create 1 2", Permission::God), "");
    EXPECT_EQ(run(router, "*info someone", Permission::God), "");
    EXPECT_EQ(run(router, "*zone 1013", Permission::God), "");
}

// The two guild-union commands answer a relayed message and nothing else.
TEST(GMRelayCommands, TheGuildUnionCommandsAreRelayOnly) {
    EXPECT_EQ(run(operatorTable(), "*modifyunioninfo 5", Permission::God), "");
    EXPECT_EQ(run(broadcastTable(), "*refreshguildunion", Permission::God), "");

    EXPECT_EQ(run(relayTable(), "*modifyunioninfo 5", Permission::God), "opmodifyunioninfo");
    EXPECT_EQ(run(relayTable(), "*refreshguildunion", Permission::God), "oprefreshguildunion");
}
