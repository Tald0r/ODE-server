#ifndef __LOGIN_CHARACTER_REPOSITORY_H__
#define __LOGIN_CHARACTER_REPOSITORY_H__

#include <string>
#include <vector>

#include "Types.h"

// The loginserver's character persistence: the race tables (Slayer,
// Vampire, Ousters) as the character list, creation and selection read
// and write them, the FlagSet row a new character starts with, and the
// balance tables creation prices its starting stats from (RankEXPInfo,
// VampEXPBalanceInfo, OustersEXPBalanceInfo, STR/DEX/INTBalanceInfo).
//
// Every method takes the WorldID and runs on
// de::serverContext().database().getConnection(worldID): the loginserver's
// per-WorldDBInfo-row connection map (an unmapped id falls through to a
// world-default connection the loginserver never sets; see
// LoginCharacterPurgeRepository.h). The balance tables are read on that
// world connection too.
//
// The Slayer table is the character index: every character has a Slayer
// row whose Race column names its race, and a Vampire or Ousters row
// besides when it is one. loadSlayerList returns every ACTIVE Slayer row
// of the account, and the caller follows a VAMPIRE or OUSTERS race to the
// matching table through loadVampireListRow / loadOustersListRow.
//
// Reads are typed to the driver getter used: getInt -> int, getString ->
// std::string, getWORD -> WORD, getDWORD -> DWORD. The callers narrow from
// there. Names, account ids and the slot / sex / hair-style texts are
// interpolated raw.
//
// Not enclosed (SQL on the same tables elsewhere in the tree): the race
// tables belong to the gameserver's Character, Gold, Stash, SkillSave,
// RankBonus and CharacterPurge repositories, the sharedserver's
// SharedGuildRepository (GuildID and Gold) and the loginserver's
// LoginCharacterPurgeRepository (the INACTIVE flip and the purge);
// FlagSet to the gameserver's FlagSetRepository and the purges; the
// balance tables to the gameserver's BalanceInfoRepository.

// Which race table a selection reads: the Slayer statement projects
// GREATEST(SwordLevel,BladeLevel,GunLevel,EnchantLevel,HealLevel) where the
// other two project Level.
enum LoginRaceTable {
    LOGIN_RACE_TABLE_SLAYER,
    LOGIN_RACE_TABLE_VAMPIRE,
    LOGIN_RACE_TABLE_OUSTERS,
    LOGIN_RACE_TABLE_MAX
};

// Which attribute balance table a creation probe reads.
enum LoginAttrTable { LOGIN_ATTR_TABLE_STR, LOGIN_ATTR_TABLE_DEX, LOGIN_ATTR_TABLE_INT, LOGIN_ATTR_TABLE_MAX };

// The FlagSet row a new character starts with: '11110010001' for a
// Slayer, '00000000001' for the other races.
enum LoginFlagSetPreset { LOGIN_FLAGSET_SLAYER, LOGIN_FLAGSET_OTHER, LOGIN_FLAGSET_MAX };

// CLSelectPCHandler's read. level is GREATEST(...) of the five skill
// domains for a Slayer and Level for the others.
struct LoginSelectRow {
    WORD zoneID;
    std::string slot;
    int level;
    int competence;
};

// The character-list builder: one ACTIVE Slayer row.
// hairColor .. rank are read for every row; the caller uses them only
// when race is "SLAYER".
struct LoginSlayerListRow {
    std::string race;
    std::string name;
    std::string slot;
    std::string sex;
    int hairColor;
    int skinColor;
    int advancementClass;
    int str;
    int strExp;
    int dex;
    int dexExp;
    int inte;
    int intExp;
    int hp;
    int currentHP;
    int mp;
    int currentMP;
    int fame;
    // BladeLevel, SwordLevel, GunLevel, HealLevel, EnchantLevel, ETCLevel:
    // the six skill domains below SKILL_DOMAIN_VAMPIRE, in column order.
    int domainLevel[6];
    int alignment;
    DWORD shape;
    int helmetColor;
    int jacketColor;
    int pantsColor;
    int weaponColor;
    int shieldColor;
    int rank;
};

struct LoginVampireListRow {
    std::string name;
    std::string slot;
    std::string sex;
    int batColor;
    int skinColor;
    int advancementClass;
    int str;
    int dex;
    int inte;
    int hp;
    int currentHP;
    int rank;
    // The GoalExp column, which the caller stores as the exp.
    int goalExp;
    int level;
    int bonus;
    int fame;
    int alignment;
    DWORD shape;
    int coatColor;
};

struct LoginOustersListRow {
    std::string name;
    std::string slot;
    std::string sex;
    int advancementClass;
    int str;
    int dex;
    int inte;
    int hp;
    int currentHP;
    int rank;
    int exp;
    int level;
    int bonus;
    int skillBonus;
    int fame;
    int alignment;
    int coatType;
    int armType;
    int coatColor;
    int hairColor;
    int armColor;
    int bootsColor;
};

// CLCreatePCHandler's INSERT INTO Slayer, field for field in the
// statement's parameter order. The texts (race, slot, sex, hairStyle) are
// the caller's *2String lookups. Fixed columns (Phone 0, ZoneID 2101,
// XCoord 65, YCoord 45, Sight 13, Gold 0, Alignment 7500, creation_date
// now()) are in the statement.
struct LoginNewSlayer {
    std::string race;
    std::string name;
    std::string playerID;
    std::string slot;
    int serverGroupID;
    std::string sex;
    std::string hairStyle;
    int hairColor;
    int skinColor;
    int str;
    int strExp;
    int strGoalExp;
    int dex;
    int dexExp;
    int dexGoalExp;
    int inte;
    int intExp;
    int intGoalExp;
    int rank;
    int rankExp;
    int rankGoalExp;
    int hp;
    int currentHP;
    int mp;
    int currentMP;
    DWORD shape;
    int helmetColor;
    int jacketColor;
    int pantsColor;
    int weaponColor;
    int shieldColor;
};

// INSERT INTO Vampire. Fixed columns: STR/DEX/INTE 20, HP/CurrentHP 50,
// ZoneID 1003, XCoord 62, YCoord 64, Sight 13, Alignment 7500, Exp 0,
// Rank 1, RankExp 0, CoatColor 377.
struct LoginNewVampire {
    std::string name;
    std::string playerID;
    std::string slot;
    int serverGroupID;
    std::string sex;
    int skinColor;
    int goalExp;
    int rankGoalExp;
    DWORD shape;
};

// INSERT INTO Ousters. Fixed columns: Sex 'FEMALE', BONUS 0, HP/CurrentHP/
// MP/CurrentMP 50, ZoneID 1311, XCoord 24, YCoord 73, Sight 13, Alignment
// 7500, Exp 0, Rank 1, RankExp 0, CoatColor/ArmColor/BootsColor 377.
struct LoginNewOusters {
    std::string name;
    std::string playerID;
    std::string slot;
    int serverGroupID;
    int str;
    int dex;
    int inte;
    int goalExp;
    int rankGoalExp;
    int hairColor;
};

class LoginCharacterRepository {
public:
    virtual ~LoginCharacterRepository() {}

    // --- name and slot probes ---------------------------------------------------
    // Is there a Slayer row of that name, ACTIVE or not? (CLCreatePCHandler
    // and CLQueryCharacterNameHandler share the statement.)
    virtual bool slayerNameExists(WorldID_t worldID, const std::string& name) = 0;
    // Does the account already have an ACTIVE Slayer row in that slot?
    virtual bool slotOccupied(WorldID_t worldID, const std::string& playerID, const std::string& slot) = 0;
    // The Name of the account's Slayer row in slot SLOT<n>, ACTIVE or not.
    // False when there is none; name is untouched then.
    virtual bool loadSlayerNameInSlot(WorldID_t worldID, const std::string& playerID, int slot, std::string& name) = 0;

    // --- the balance probes creation makes -------------------------------------
    // RankEXPInfo.GoalExp at Level 1 for one RankType (0 Slayer, 1 Vampire,
    // 2 Ousters). False when there is no row; goalExp is untouched then.
    virtual bool loadRankGoalExp(WorldID_t worldID, int rankType, int& goalExp) = 0;
    // VampEXPBalanceInfo.GoalExp at Level 1.
    virtual bool loadVampireGoalExp(WorldID_t worldID, int& goalExp) = 0;
    // OustersEXPBalanceInfo.GoalExp at Level 1.
    virtual bool loadOustersGoalExp(WorldID_t worldID, int& goalExp) = 0;
    // <STR|DEX|INT>BalanceInfo.GoalExp / .AccumExp at one level.
    virtual bool loadAttrGoalExp(WorldID_t worldID, LoginAttrTable attr, int level, int& goalExp) = 0;
    virtual bool loadAttrAccumExp(WorldID_t worldID, LoginAttrTable attr, int level, int& accumExp) = 0;

    // --- creation -------------------------------------------------------------
    virtual void insertSlayer(WorldID_t worldID, const LoginNewSlayer& row) = 0;
    virtual void insertVampire(WorldID_t worldID, const LoginNewVampire& row) = 0;
    virtual void insertOusters(WorldID_t worldID, const LoginNewOusters& row) = 0;
    // INSERT IGNORE INTO FlagSet with the preset's FlagData text.
    virtual void insertFlagSet(WorldID_t worldID, const std::string& name, LoginFlagSetPreset preset) = 0;

    // --- selection (CLSelectPCHandler) ------------------------------------------
    // The ACTIVE row of that name AND account in one race table. False
    // unless exactly one row; row is untouched then.
    virtual bool loadCharacterForSelect(WorldID_t worldID, LoginRaceTable table, const std::string& name,
                                        const std::string& playerID, LoginSelectRow& row) = 0;
    // ServerGroupID on all three race tables for that name, three
    // statements on one Statement, in Slayer / Vampire / Ousters order.
    virtual void setCharacterServerGroup(WorldID_t worldID, int serverGroupID, const std::string& name) = 0;

    // --- the character list ---------------------------------------------------
    virtual std::vector<LoginSlayerListRow> loadSlayerList(WorldID_t worldID, const std::string& playerID) = 0;
    // The ACTIVE Vampire / Ousters row of that account AND name. False when
    // there is none; row is untouched then.
    virtual bool loadVampireListRow(WorldID_t worldID, const std::string& playerID, const std::string& name,
                                    LoginVampireListRow& row) = 0;
    virtual bool loadOustersListRow(WorldID_t worldID, const std::string& playerID, const std::string& name,
                                    LoginOustersListRow& row) = 0;
};

// The process-wide MySQL-backed instance, wired in
// MySQLLoginCharacterRepository.cpp.
LoginCharacterRepository& defaultLoginCharacterRepository();

#endif
