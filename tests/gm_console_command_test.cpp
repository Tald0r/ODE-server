//////////////////////////////////////////////////////////////////////////////
// Filename    : gm_console_command_test.cpp
// Description : The *command console's sub-command table
//               (docs/reviews/RESTRUCTURING.md 4.1): every registered sub-command
//               declares a permission level and whether its body runs with no
//               player behind the line, the names are unique and matched
//               whole, a caller below the level runs nothing, a row that needs
//               a player runs nothing when a relayed line carries none, and an
//               unknown name runs nothing either - which is what leaves the
//               console with no reply to send.
//
//               The production table is the subject, not a copy of it: this
//               links the real registration and stubs the bodies, so a name,
//               a gate or an order that changes there fails here. The
//               expected table below is the source of truth for the gates;
//               changing one means editing this file.
//////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "GCSystemMessage.h"
#include "gm/ConsoleSubcommands.h"

using de::gm::Permission;
using de::gm::Relay;
using de::gm::Subcommand;
using de::gm::SubcommandTable;

namespace {

struct Call {
    std::string body;
    std::string value1;
};

std::vector<Call>& calls() {
    static std::vector<Call> recorded;
    return recorded;
}

void record(const char* body, const std::string& value1) {
    calls().push_back(Call{body, value1});
}

// What the console keeps while a sub-command runs: the reply it will send and
// the flag that says whether to send it.
struct Reply {
    GCSystemMessage packet;
    bool send = true;
};

// A stand-in for the player who typed the line. The stubbed bodies never read
// it; what it carries is that the line has a player at all, which is exactly
// what a line another game server relayed does not.
GamePlayer* const kSomePlayer = reinterpret_cast<GamePlayer*>(0x1);

// Runs a sub-command name against a table as a caller of the given level, and
// answers the name of the body that ran, "" for none. A null player is a
// relayed line.
std::string run(const SubcommandTable& table, const std::string& name, Permission caller,
                GamePlayer* pGamePlayer = kSomePlayer, const std::string& value1 = "argument", Reply* reply = nullptr,
                bool* dispatched = nullptr) {
    calls().clear();
    Reply own;
    Reply& out = (reply != nullptr) ? *reply : own;
    const bool ran = table.dispatch(name, caller, pGamePlayer, value1, out.packet, out.send);
    if (dispatched != nullptr)
        *dispatched = ran;
    if (calls().empty())
        return "";
    return calls().front().body;
}

SubcommandTable consoleTable() {
    SubcommandTable table;
    de::gm::registerConsoleSubcommands(table);
    return table;
}

struct Expected {
    std::string name;
    Permission permission;
    Relay relay;
    std::string body;
};

// Every sub-command, in the order a word is matched against them. All of them
// are GOD: the console itself is only reached from a GOD-gated chat command
// and from the relay another game server sends. PlayerOnly is a body that
// reads the player who typed the line without asking whether there is one.
std::vector<Expected> expectedSubcommands() {
    return {
        {"balanceZoneGroup", Permission::God, Relay::Allowed, "opBalanceZoneGroup"},
        {"regenMasterLair", Permission::God, Relay::Allowed, "opRegenMasterLair"},
        {"showMasterLairStatus", Permission::God, Relay::Allowed, "opShowMasterLairStatus"},
        {"invincible", Permission::God, Relay::PlayerOnly, "opInvincible"},
        {"ghost", Permission::God, Relay::PlayerOnly, "opGhost"},
        {"clearInventory", Permission::God, Relay::PlayerOnly, "opClearInventory"},
        {"clearRankBonus", Permission::God, Relay::PlayerOnly, "opClearRankBonus"},
        {"setCastleOwner", Permission::God, Relay::PlayerOnly, "opSetCastleOwner"},
        {"setCastleOwnerGuild", Permission::God, Relay::Allowed, "opSetCastleOwnerGuild"},
        {"showWarList", Permission::God, Relay::PlayerOnly, "opShowWarList"},
        {"startRaceWar", Permission::God, Relay::Allowed, "opStartRaceWar"},
        {"startWar", Permission::God, Relay::PlayerOnly, "opStartWar"},
        {"removeWar", Permission::God, Relay::Allowed, "opRemoveWar"},
        {"removeRaceWar", Permission::God, Relay::Allowed, "opRemoveRaceWar"},
        {"LevelWar", Permission::God, Relay::Allowed, "opLevelWar"},
        {"saveBloodBibleOwner", Permission::God, Relay::Allowed, "opSaveBloodBibleOwner"},
        {"killAllMonster", Permission::God, Relay::Allowed, "opKillAllMonster"},
        {"killAllPC", Permission::God, Relay::Allowed, "opKillAllPC"},
        {"showZonePCNum", Permission::God, Relay::Allowed, "opShowZonePCNum"},
        {"showPKZonePCNum", Permission::God, Relay::Allowed, "opShowPKZonePCNum"},
        {"setPKZonePCNum", Permission::God, Relay::Allowed, "opSetPKZonePCNum"},
        {"suicide", Permission::God, Relay::PlayerOnly, "opSuicide"},
        {"heal", Permission::God, Relay::PlayerOnly, "opHeal"},
        {"setGold", Permission::God, Relay::PlayerOnly, "opSetGold"},
        {"Quest", Permission::God, Relay::PlayerOnly, "opQuest"},
        {"QuestEnding", Permission::God, Relay::PlayerOnly, "opQuestEnding"},
        {"NotifyWin", Permission::God, Relay::Allowed, "opNotifyWin"},
        {"Horn", Permission::God, Relay::PlayerOnly, "opHorn"},
        {"Loud", Permission::God, Relay::PlayerOnly, "opLoud"},
        {"Game", Permission::God, Relay::Allowed, "opGame"},
        {"changeSex", Permission::God, Relay::PlayerOnly, "opChangeSex"},
        {"Firecraker", Permission::God, Relay::PlayerOnly, "opFirecraker"},
        {"Bulletin", Permission::God, Relay::PlayerOnly, "opBulletin"},
        {"SetHP", Permission::God, Relay::PlayerOnly, "opSetHP"},
        {"ResetAttr", Permission::God, Relay::PlayerOnly, "opResetAttr"},
        {"CTF", Permission::God, Relay::Allowed, "opCTF"},
        {"ViewDamage", Permission::God, Relay::PlayerOnly, "opViewDamage"},
        {"GoodsReload", Permission::God, Relay::PlayerOnly, "opGoodsReload"},
        {"PetStash", Permission::God, Relay::PlayerOnly, "opPetStash"},
        {"ZoneEvent", Permission::God, Relay::PlayerOnly, "opZoneEvent"},
        {"EventZonePCLimit", Permission::God, Relay::PlayerOnly, "opEventZonePCLimit"},
        {"KickOutAll", Permission::God, Relay::PlayerOnly, "opKickOutAll"},
        {"StartTrap", Permission::God, Relay::PlayerOnly, "opStartTrap"},
        {"SMSTest", Permission::God, Relay::PlayerOnly, "opSMSTest"},
        {"ForceNick", Permission::God, Relay::PlayerOnly, "opForceNick"},
        {"RemoveNick", Permission::God, Relay::PlayerOnly, "opRemoveNick"},
        {"StartGDRLair", Permission::God, Relay::Allowed, "opStartGDRLair"},
        {"ResetGDRLair", Permission::God, Relay::Allowed, "opResetGDRLair"},
        {"GuildRecall", Permission::God, Relay::Allowed, "opGuildRecall"},
        {"ResetSiege", Permission::God, Relay::PlayerOnly, "opResetSiege"},
        {"InitSiege", Permission::God, Relay::PlayerOnly, "opInitSiege"},
        {"IAmAttacker", Permission::God, Relay::PlayerOnly, "opIAmAttacker"},
        {"IAmDefender", Permission::God, Relay::PlayerOnly, "opIAmDefender"},
        {"IAmReinforce", Permission::God, Relay::PlayerOnly, "opIAmReinforce"},
        {"showpcstat", Permission::God, Relay::PlayerOnly, "opShowpcstat"},
        {"advanceclass", Permission::God, Relay::PlayerOnly, "opAdvanceclass"},
        {"addDynamicZone", Permission::God, Relay::Allowed, "opAddDynamicZone"},
        {"enterDynamicZone", Permission::God, Relay::PlayerOnly, "opEnterDynamicZone"},
        {"clearDynamicZone", Permission::God, Relay::PlayerOnly, "opClearDynamicZone"},
        {"setTimeOutAllZoneEffect", Permission::God, Relay::PlayerOnly, "opSetTimeOutAllZoneEffect"},
        {"printTile", Permission::God, Relay::PlayerOnly, "opPrintTile"},
    };
}

} // namespace

//////////////////////////////////////////////////////////////////////////////
// The sub-command bodies, stubbed. The gameserver's own definitions need a
// zone, an inventory and a database; what is under test is which of them the
// table picks, so each one only says that it ran and what it was handed.
//////////////////////////////////////////////////////////////////////////////

namespace de::gm {

void opBalanceZoneGroup(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opBalanceZoneGroup", value1);
}
void opRegenMasterLair(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opRegenMasterLair", value1);
}
void opShowMasterLairStatus(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opShowMasterLairStatus", value1);
}
void opInvincible(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opInvincible", value1);
}
void opGhost(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opGhost", value1);
}
void opClearInventory(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opClearInventory", value1);
}
void opClearRankBonus(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opClearRankBonus", value1);
}
void opSetCastleOwner(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opSetCastleOwner", value1);
}
void opSetCastleOwnerGuild(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opSetCastleOwnerGuild", value1);
}
void opShowWarList(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opShowWarList", value1);
}
void opStartRaceWar(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opStartRaceWar", value1);
}
void opStartWar(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opStartWar", value1);
}
void opRemoveWar(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opRemoveWar", value1);
}
void opRemoveRaceWar(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opRemoveRaceWar", value1);
}
void opLevelWar(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opLevelWar", value1);
}
void opSaveBloodBibleOwner(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opSaveBloodBibleOwner", value1);
}
void opKillAllMonster(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opKillAllMonster", value1);
}
void opKillAllPC(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opKillAllPC", value1);
}
void opShowZonePCNum(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opShowZonePCNum", value1);
}
void opShowPKZonePCNum(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opShowPKZonePCNum", value1);
}
void opSetPKZonePCNum(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opSetPKZonePCNum", value1);
}
void opSuicide(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opSuicide", value1);
}
void opHeal(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opHeal", value1);
}
void opSetGold(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opSetGold", value1);
}
void opQuest(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opQuest", value1);
}
void opQuestEnding(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opQuestEnding", value1);
}
void opNotifyWin(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opNotifyWin", value1);
}
void opHorn(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opHorn", value1);
}
void opLoud(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opLoud", value1);
}
void opGame(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opGame", value1);
}
void opChangeSex(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opChangeSex", value1);
}
void opFirecraker(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opFirecraker", value1);
}
void opBulletin(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opBulletin", value1);
}
void opSetHP(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opSetHP", value1);
}
void opResetAttr(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opResetAttr", value1);
}
void opCTF(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opCTF", value1);
}
void opViewDamage(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opViewDamage", value1);
}
void opGoodsReload(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opGoodsReload", value1);
}
void opPetStash(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opPetStash", value1);
}
void opZoneEvent(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opZoneEvent", value1);
}
void opEventZonePCLimit(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opEventZonePCLimit", value1);
}
void opKickOutAll(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opKickOutAll", value1);
}
void opStartTrap(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opStartTrap", value1);
}
void opSMSTest(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opSMSTest", value1);
}
void opForceNick(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opForceNick", value1);
}
void opRemoveNick(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opRemoveNick", value1);
}
void opStartGDRLair(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opStartGDRLair", value1);
}
void opResetGDRLair(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opResetGDRLair", value1);
}
void opGuildRecall(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opGuildRecall", value1);
}
void opResetSiege(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opResetSiege", value1);
}
void opInitSiege(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opInitSiege", value1);
}
void opIAmAttacker(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opIAmAttacker", value1);
}
void opIAmDefender(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opIAmDefender", value1);
}
void opIAmReinforce(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opIAmReinforce", value1);
}
void opShowpcstat(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opShowpcstat", value1);
}
void opAdvanceclass(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opAdvanceclass", value1);
}
void opAddDynamicZone(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opAddDynamicZone", value1);
}
void opEnterDynamicZone(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opEnterDynamicZone", value1);
}
void opClearDynamicZone(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opClearDynamicZone", value1);
}
void opSetTimeOutAllZoneEffect(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opSetTimeOutAllZoneEffect", value1);
}
void opPrintTile(GamePlayer*, const std::string& value1, GCSystemMessage&, bool&) {
    record("opPrintTile", value1);
}

} // namespace de::gm

//////////////////////////////////////////////////////////////////////////////
// What every registered sub-command owes
//////////////////////////////////////////////////////////////////////////////

TEST(GMConsoleCommands, EveryRegisteredSubcommandDeclaresAPermissionLevel) {
    const SubcommandTable table = consoleTable();
    EXPECT_FALSE(table.commands().empty());
    for (const Subcommand& subcommand : table.commands()) {
        EXPECT_FALSE(subcommand.name.empty());
        EXPECT_TRUE(subcommand.permission == Permission::God || subcommand.permission == Permission::DM ||
                    subcommand.permission == Permission::Helper || subcommand.permission == Permission::Everyone)
            << subcommand.name;
        EXPECT_TRUE(subcommand.relay == Relay::Allowed || subcommand.relay == Relay::PlayerOnly) << subcommand.name;
        EXPECT_NE(subcommand.handler, nullptr) << subcommand.name;
    }
}

TEST(GMConsoleCommands, SubcommandNamesAreUnique) {
    const SubcommandTable table = consoleTable();
    std::vector<std::string> seen;
    for (const Subcommand& subcommand : table.commands()) {
        EXPECT_EQ(std::count(seen.begin(), seen.end(), subcommand.name), 0) << subcommand.name;
        seen.push_back(subcommand.name);
    }
}

//////////////////////////////////////////////////////////////////////////////
// The production table
//////////////////////////////////////////////////////////////////////////////

TEST(GMConsoleCommands, TheTableIsTheExpectedOne) {
    const SubcommandTable table = consoleTable();
    const std::vector<Expected> expected = expectedSubcommands();

    ASSERT_EQ(table.commands().size(), expected.size());
    for (std::size_t n = 0; n < expected.size(); ++n) {
        EXPECT_EQ(table.commands()[n].name, expected[n].name) << "row " << n;
        EXPECT_EQ(static_cast<int>(table.commands()[n].permission), static_cast<int>(expected[n].permission))
            << expected[n].name;
        EXPECT_EQ(static_cast<int>(table.commands()[n].relay), static_cast<int>(expected[n].relay)) << expected[n].name;
    }
}

TEST(GMConsoleCommands, EverySubcommandRunsItsOwnBody) {
    const SubcommandTable table = consoleTable();
    for (const Expected& expected : expectedSubcommands())
        EXPECT_EQ(run(table, expected.name, expected.permission), expected.body);
}

//////////////////////////////////////////////////////////////////////////////
// Name matching
//////////////////////////////////////////////////////////////////////////////

// A name is compared with the whole word that follows *command, so - unlike
// the chat commands, which match as a prefix of the message - a longer word
// reaches nothing.
TEST(GMConsoleCommands, ANameIsMatchedWhole) {
    const SubcommandTable table = consoleTable();

    EXPECT_EQ(run(table, "heal", Permission::God), "opHeal");
    EXPECT_EQ(run(table, "healx", Permission::God), "");
    EXPECT_EQ(run(table, "hea", Permission::God), "");
    EXPECT_EQ(run(table, "", Permission::God), "");
    EXPECT_EQ(run(table, "HEAL", Permission::God), "") << "names are compared case-sensitively";
}

TEST(GMConsoleCommands, AnUnknownSubcommandRunsNothing) {
    const SubcommandTable table = consoleTable();

    Reply reply;
    reply.packet.setMessage("nothing");
    bool dispatched = true;
    EXPECT_EQ(run(table, "nosuchsubcommand", Permission::God, kSomePlayer, "argument", &reply, &dispatched), "");
    EXPECT_FALSE(dispatched);

    // The console answers an unknown name by dropping the packet it had
    // prepared, so the reply it was holding never reaches anyone.
    EXPECT_EQ(reply.packet.getMessage(), "nothing");
    EXPECT_TRUE(reply.send);
}

//////////////////////////////////////////////////////////////////////////////
// Gating
//////////////////////////////////////////////////////////////////////////////

TEST(GMConsoleCommands, ACallerBelowGODRunsNothing) {
    const SubcommandTable table = consoleTable();

    EXPECT_EQ(run(table, "heal", Permission::God), "opHeal");
    EXPECT_EQ(run(table, "heal", Permission::DM), "");
    EXPECT_EQ(run(table, "heal", Permission::Helper), "");
    EXPECT_EQ(run(table, "heal", Permission::Everyone), "");
}

TEST(GMConsoleCommands, ARefusedSubcommandDoesNotAnswer) {
    const SubcommandTable table = consoleTable();

    bool dispatched = true;
    EXPECT_EQ(run(table, "heal", Permission::Everyone, kSomePlayer, "argument", nullptr, &dispatched), "");
    EXPECT_FALSE(dispatched);
}

//////////////////////////////////////////////////////////////////////////////
// What a body is handed
//////////////////////////////////////////////////////////////////////////////

TEST(GMConsoleCommands, ABodyIsHandedTheRestOfTheLine) {
    const SubcommandTable table = consoleTable();

    calls().clear();
    Reply reply;
    EXPECT_TRUE(table.dispatch("setGold", Permission::God, kSomePlayer, "1000", reply.packet, reply.send));
    ASSERT_EQ(calls().size(), 1u);
    EXPECT_EQ(calls().front().body, "opSetGold");
    EXPECT_EQ(calls().front().value1, "1000");
}

namespace {

// A body fills in the reply the console sends, or clears the flag to send
// nothing at all.
void answersTheLine(GamePlayer*, const std::string& value1, GCSystemMessage& gcSystemMessage, bool& bSendPacket) {
    gcSystemMessage.setMessage(value1);
    bSendPacket = false;
}

} // namespace

TEST(GMConsoleCommands, ABodyWritesTheReplyTheConsoleSends) {
    SubcommandTable table;
    table.add("answer", Permission::God, Relay::Allowed, answersTheLine);

    Reply reply;
    reply.packet.setMessage("nothing");
    EXPECT_TRUE(table.dispatch("answer", Permission::God, nullptr, "a message", reply.packet, reply.send));
    EXPECT_EQ(reply.packet.getMessage(), "a message");
    EXPECT_FALSE(reply.send);
}

//////////////////////////////////////////////////////////////////////////////
// A line with no player behind it
//////////////////////////////////////////////////////////////////////////////

// *command reaches the console from a relay as well, and a relayed line
// carries no player. A row that reads one is passed over, the way a row the
// caller may not run is.
TEST(GMConsoleCommands, APlayerOnlySubcommandIsPassedOverWithNoPlayer) {
    const SubcommandTable table = consoleTable();

    EXPECT_EQ(run(table, "heal", Permission::God, kSomePlayer), "opHeal");

    bool dispatched = true;
    EXPECT_EQ(run(table, "heal", Permission::God, nullptr, "argument", nullptr, &dispatched), "");
    EXPECT_FALSE(dispatched);
}

// The castle-owner row answers the relay a siege war sends between game
// servers, so it has to run without a player.
TEST(GMConsoleCommands, AnAllowedSubcommandRunsWithNoPlayer) {
    const SubcommandTable table = consoleTable();

    EXPECT_EQ(run(table, "setCastleOwnerGuild", Permission::God, nullptr, "1013 5"), "opSetCastleOwnerGuild");
}

// Every row, against a relayed line: the ones declared Allowed run and the
// ones declared PlayerOnly do not.
TEST(GMConsoleCommands, EveryRowAnswersARelayedLineAsDeclared) {
    const SubcommandTable table = consoleTable();

    for (const Expected& expected : expectedSubcommands()) {
        const std::string ran = run(table, expected.name, expected.permission, nullptr);
        if (expected.relay == Relay::Allowed)
            EXPECT_EQ(ran, expected.body) << expected.name;
        else
            EXPECT_EQ(ran, "") << expected.name;
    }
}
