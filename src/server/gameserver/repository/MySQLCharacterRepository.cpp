#include "DB.h"
#include "ServerContext.h"
#include "repository/CharacterRepository.h"

namespace {

// MySQL implementation of the character-row repository.
//  - Vampire saveExps writes SilverDamage ONLY when it is non-zero: the
//    ",SilverDamage = %d" fragment is composed into a %s slot, so a zero
//    value leaves the column untouched -- this save cannot reset a
//    vampire's silver damage. Ousters writes it unconditionally. Slayer
//    has no SilverDamage at all.
//  - tinysave's SET fragment is caller-composed raw SQL (sprintf'd
//    "Column=value" strings), applied verbatim. Slayer's WHERE spells the
//    column NAME, the others Name -- cosmetic, MySQL column identifiers
//    are case-insensitive.
//  - The `Rank` backticks are LOAD-BEARING on MySQL 8: RANK became a
//    reserved word in 8.0.2, and this project supports 5.7 or 8. The
//    5.7-based integration tier cannot catch their removal.
//  - An UPDATE for a name with no row matches zero rows, silently, and
//    affected-rows may be 0 when nothing changed (no CLIENT_FOUND_ROWS);
//    nothing here checks it.
//  - Character names are interpolated raw (no escaping).
//  - The load SELECTs are POSITIONAL: the loaders read column N of the
//    result, so the column list's order is the contract -- including
//    `Rank` (see above), the un-spaced "Sex,MasterEffectColor" token and
//    the INTE spelling (INT is a MySQL type keyword).
//  - The loaders apply the record after the load returns, so the race
//    classes' setters run outside the BEGIN_DB/END_DB try and after the
//    Statement is freed. A driver exception mid-read applies nothing to
//    the creature.
//  - The loads filter on Active = 'ACTIVE': an INACTIVE row (a character
//    deleted while the login server handed it over) loads as "no row".
//    Name is the primary key, so at most one row can match.
//  - Every selected column is read with getInt (atoi of the field text)
//    except StashNum and the vampire/ousters Competence pair (getBYTE)
//    and the enum/varchar columns (getString). Out-of-int-range unsigned
//    values (Fame, Gold, GoalExp are int(10) unsigned) come back through
//    atoi.
class MySQLCharacterRepository : public CharacterRepository {
public:
    bool loadSlayer(const string& ownerName, SlayerLoadRecord& record) {
        bool found = false;
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            Result* pResult = pStmt->executeQuery(
                "SELECT Name, AdvancementClass, AdvancementGoalExp, Competence, CompetenceShape, "
                "Sex,MasterEffectColor, "
                "HairStyle, HairColor, SkinColor, Phone, STR, STRGoalExp, DEX, DEXGoalExp, INTE, INTGoalExp, "
                "AdvancedSTR, AdvancedDEX, AdvancedINT, Bonus, `Rank`, RankGoalExp, CurrentHP, HP, CurrentMP, MP, "
                "Fame, Gold, GuildID, BladeLevel, BladeGoalExp, SwordLevel, SwordGoalExp, GunLevel, GunGoalExp, "
                "EnchantLevel, EnchantGoalExp, HealLevel, HealGoalExp, ETCLevel, ETCGoalExp, ZoneID, XCoord, YCoord, "
                "Sight, GunBonusExp, RifleBonusExp, Alignment, StashGold, StashNum, ResurrectZone, Reward, SMSCharge "
                "FROM Slayer WHERE Name = '%s' AND Active = 'ACTIVE'",
                ownerName.c_str());

            if (pResult->next()) {
                uint i = 0;
                record.name = pResult->getString(++i);
                record.advancementClass = pResult->getInt(++i);
                record.advancementGoalExp = pResult->getInt(++i);
                record.competence = pResult->getInt(++i);
                record.competenceShape = pResult->getInt(++i);
                record.sex = pResult->getString(++i);
                record.masterEffectColor = pResult->getInt(++i);
                record.hairStyle = pResult->getString(++i);
                record.hairColor = pResult->getInt(++i);
                record.skinColor = pResult->getInt(++i);
                record.phone = pResult->getString(++i);
                record.str = pResult->getInt(++i);
                record.strGoalExp = pResult->getInt(++i);
                record.dex = pResult->getInt(++i);
                record.dexGoalExp = pResult->getInt(++i);
                record.inte = pResult->getInt(++i);
                record.intGoalExp = pResult->getInt(++i);
                record.advancedSTR = pResult->getInt(++i);
                record.advancedDEX = pResult->getInt(++i);
                record.advancedINT = pResult->getInt(++i);
                record.bonus = pResult->getInt(++i);
                record.rank = pResult->getInt(++i);
                record.rankGoalExp = pResult->getInt(++i);
                record.currentHP = pResult->getInt(++i);
                record.maxHP = pResult->getInt(++i);
                record.currentMP = pResult->getInt(++i);
                record.maxMP = pResult->getInt(++i);
                record.fame = pResult->getInt(++i);
                record.gold = pResult->getInt(++i);
                record.guildID = pResult->getInt(++i);
                record.bladeLevel = pResult->getInt(++i);
                record.bladeGoalExp = pResult->getInt(++i);
                record.swordLevel = pResult->getInt(++i);
                record.swordGoalExp = pResult->getInt(++i);
                record.gunLevel = pResult->getInt(++i);
                record.gunGoalExp = pResult->getInt(++i);
                record.enchantLevel = pResult->getInt(++i);
                record.enchantGoalExp = pResult->getInt(++i);
                record.healLevel = pResult->getInt(++i);
                record.healGoalExp = pResult->getInt(++i);
                record.etcLevel = pResult->getInt(++i);
                record.etcGoalExp = pResult->getInt(++i);
                record.zoneID = pResult->getInt(++i);
                record.x = pResult->getInt(++i);
                record.y = pResult->getInt(++i);
                record.sight = pResult->getInt(++i);
                record.gunBonusExp = pResult->getInt(++i);
                record.rifleBonusExp = pResult->getInt(++i);
                record.alignment = pResult->getInt(++i);
                record.stashGold = pResult->getInt(++i);
                record.stashNum = pResult->getBYTE(++i);
                record.resurrectZone = pResult->getInt(++i);
                record.reward = pResult->getInt(++i);
                record.smsCharge = pResult->getInt(++i);
                found = true;
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)

        return found;
    }

    bool loadSlayerAccount(const string& name, string& playerID, string& race) {
        bool found = false;
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            Result* pResult = pStmt->executeQuery("SELECT PlayerID,Race FROM Slayer WHERE Name = '%s'", name.c_str());

            if (pResult->getRowCount() == 1) {
                pResult->next();
                playerID = pResult->getString(1);
                race = pResult->getString(2);
                found = true;
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)

        return found;
    }

    bool loadSlayerPlayerID(const string& name, string& playerID) {
        return loadSlayerPlayerID(PLAYERID_SPELLING_WHISPER, name, playerID);
    }

    // The two spellings of the name-to-account lookup, indexed by
    // SlayerPlayerIDSpelling.
    bool loadSlayerPlayerID(SlayerPlayerIDSpelling spelling, const string& name, string& playerID) {
        static const char* const kSpellings[PLAYERID_SPELLING_MAX] = {
            "SELECT PlayerID FROM Slayer WHERE Name='%s'", // PLAYERID_SPELLING_WHISPER
            "SELECT PlayerID FROM Slayer where Name='%s'", // PLAYERID_SPELLING_OPDENY
        };
        if (spelling >= PLAYERID_SPELLING_MAX) {
            throw Error("CharacterRepository: unknown SlayerPlayerIDSpelling");
        }
        bool found = false;
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            Result* pResult = pStmt->executeQuery(kSpellings[spelling], name.c_str());

            if (pResult->next()) {
                playerID = pResult->getString(1);
                found = true;
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)

        return found;
    }

    bool loadVampireRedistributeAttr(const string& name, int& redistributeAttr) {
        bool found = false;
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            Result* pResult = pStmt->executeQuery("SELECT RedistributeAttr FROM Vampire WHERE Name='%s'", name.c_str());

            if (pResult->next()) {
                redistributeAttr = pResult->getInt(1);
                found = true;
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)

        return found;
    }

    void saveVampireRedistributeAttr(int redistributeAttr, const string& name) {
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            pStmt->executeQuery("UPDATE Vampire SET RedistributeAttr = %d WHERE Name='%s'", redistributeAttr,
                                name.c_str());
            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)
    }

    bool loadSlayerMasterStats(const string& name, SlayerMasterStatsRow& row) {
        bool found = false;
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            Result* pResult =
                pStmt->executeQuery("SELECT Fame, BladeLevel, SwordLevel, GunLevel, HealLevel, EnchantLevel "
                                    "FROM Slayer WHERE Name = '%s'",
                                    name.c_str());

            if (pResult->next()) {
                int i = 0;
                row.fame = pResult->getInt(++i);
                row.bladeLevel = pResult->getInt(++i);
                row.swordLevel = pResult->getInt(++i);
                row.gunLevel = pResult->getInt(++i);
                row.healLevel = pResult->getInt(++i);
                row.enchantLevel = pResult->getInt(++i);
                found = true;
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)

        return found;
    }

    bool loadVampireLevel(const string& name, int& level) {
        return loadLevel("SELECT Level FROM Vampire WHERE Name = '%s'", name, level);
    }

    bool loadOustersLevel(const string& name, int& level) {
        return loadLevel("SELECT Level FROM Ousters WHERE Name = '%s'", name, level);
    }

    bool loadSlayerRaceText(const string& name, string& raceText) {
        bool found = false;
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            Result* pResult = pStmt->executeQuery("SELECT Race FROM Slayer where Name='%s'", name.c_str());

            if (pResult->next()) {
                raceText = pResult->getString(1);
                found = true;
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)

        return found;
    }

    bool loadGuildID(const string& name, CharacterRace race, int& guildID) {
        bool found = false;
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            Result* pResult =
                pStmt->executeQuery("SELECT GuildID FROM %s where Name='%s'", characterRaceTable(race), name.c_str());

            if (pResult->next()) {
                guildID = pResult->getInt(1);
                found = true;
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)

        return found;
    }

    void saveSex(const string& name, const string& sexText) {
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();

            pStmt->executeQuery("UPDATE Slayer SET SEX='%s' WHERE Name='%s'", sexText.c_str(), name.c_str());
            pStmt->executeQuery("UPDATE Vampire SET SEX='%s' WHERE Name='%s'", sexText.c_str(), name.c_str());

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)
    }

    bool loadVampire(const string& ownerName, VampireLoadRecord& record) {
        bool found = false;
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            Result* pResult = pStmt->executeQuery(
                "SELECT Name, AdvancementClass, AdvancementGoalExp, Sex, MasterEffectColor, BatColor, SkinColor, STR, "
                "DEX, INTE, HP, CurrentHP, Fame, GoalExp, Level, Bonus, Gold, GuildID, ZoneID, XCoord, YCoord, Sight, "
                "Alignment, StashGold, StashNum, Competence, CompetenceShape, ResurrectZone, SilverDamage, Reward, "
                "SMSCharge, `Rank`, RankGoalExp FROM Vampire WHERE Name = '%s' AND Active = 'ACTIVE'",
                ownerName.c_str());

            if (pResult->next()) {
                uint i = 0;
                record.name = pResult->getString(++i);
                record.advancementClass = pResult->getInt(++i);
                record.advancementGoalExp = pResult->getInt(++i);
                record.sex = pResult->getString(++i);
                record.masterEffectColor = pResult->getInt(++i);
                record.batColor = pResult->getInt(++i);
                record.skinColor = pResult->getInt(++i);
                record.str = pResult->getInt(++i);
                record.dex = pResult->getInt(++i);
                record.inte = pResult->getInt(++i);
                record.maxHP = pResult->getInt(++i);
                record.currentHP = pResult->getInt(++i);
                record.fame = pResult->getInt(++i);
                record.goalExp = pResult->getInt(++i);
                record.level = pResult->getInt(++i);
                record.bonus = pResult->getInt(++i);
                record.gold = pResult->getInt(++i);
                record.guildID = pResult->getInt(++i);
                record.zoneID = pResult->getInt(++i);
                record.x = pResult->getInt(++i);
                record.y = pResult->getInt(++i);
                record.sight = pResult->getInt(++i);
                record.alignment = pResult->getInt(++i);
                record.stashGold = pResult->getInt(++i);
                record.stashNum = pResult->getBYTE(++i);
                record.competence = pResult->getBYTE(++i);
                record.competenceShape = pResult->getBYTE(++i);
                record.resurrectZone = pResult->getInt(++i);
                record.silverDamage = pResult->getInt(++i);
                record.reward = pResult->getInt(++i);
                record.smsCharge = pResult->getInt(++i);
                record.rank = pResult->getInt(++i);
                record.rankGoalExp = pResult->getInt(++i);
                found = true;
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)

        return found;
    }

    bool loadOusters(const string& ownerName, OustersLoadRecord& record) {
        bool found = false;
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            Result* pResult = pStmt->executeQuery(
                "SELECT Name, AdvancementClass, AdvancementGoalExp, Sex,MasterEffectColor, STR, DEX, INTE, HP, "
                "CurrentHP, MP, CurrentMP, Fame, GoalExp, Level, Bonus, SkillBonus, Gold, GuildID, ZoneID, XCoord, "
                "YCoord, Sight, Alignment, StashGold, StashNum, Competence, CompetenceShape, ResurrectZone, "
                "SilverDamage, SMSCharge, `Rank`, RankGoalExp, HairColor FROM Ousters WHERE Name = '%s' AND Active = "
                "'ACTIVE'",
                ownerName.c_str());

            if (pResult->next()) {
                uint i = 0;
                record.name = pResult->getString(++i);
                record.advancementClass = pResult->getInt(++i);
                record.advancementGoalExp = pResult->getInt(++i);
                record.sex = pResult->getString(++i);
                record.masterEffectColor = pResult->getInt(++i);
                record.str = pResult->getInt(++i);
                record.dex = pResult->getInt(++i);
                record.inte = pResult->getInt(++i);
                record.maxHP = pResult->getInt(++i);
                record.currentHP = pResult->getInt(++i);
                record.maxMP = pResult->getInt(++i);
                record.currentMP = pResult->getInt(++i);
                record.fame = pResult->getInt(++i);
                record.goalExp = pResult->getInt(++i);
                record.level = pResult->getInt(++i);
                record.bonus = pResult->getInt(++i);
                record.skillBonus = pResult->getInt(++i);
                record.gold = pResult->getInt(++i);
                record.guildID = pResult->getInt(++i);
                record.zoneID = pResult->getInt(++i);
                record.x = pResult->getInt(++i);
                record.y = pResult->getInt(++i);
                record.sight = pResult->getInt(++i);
                record.alignment = pResult->getInt(++i);
                record.stashGold = pResult->getInt(++i);
                record.stashNum = pResult->getBYTE(++i);
                record.competence = pResult->getBYTE(++i);
                record.competenceShape = pResult->getBYTE(++i);
                record.resurrectZone = pResult->getInt(++i);
                record.silverDamage = pResult->getInt(++i);
                record.smsCharge = pResult->getInt(++i);
                record.rank = pResult->getInt(++i);
                record.rankGoalExp = pResult->getInt(++i);
                record.hairColor = pResult->getInt(++i);
                found = true;
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)

        return found;
    }

    void saveSlayerVitals(const string& ownerName, const SlayerVitalsRecord& record) {
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            pStmt->executeQuery("UPDATE Slayer SET CurrentHP=%d, HP=%d, CurrentMP=%d, MP=%d, ZoneID=%d, XCoord=%d, "
                                "YCoord=%d WHERE Name='%s'",
                                record.currentHP, record.maxHP, record.currentMP, record.maxMP, record.zoneID, record.x,
                                record.y, ownerName.c_str());
            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)
    }

    void saveVampireVitals(const string& ownerName, const VampireVitalsRecord& record) {
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            pStmt->executeQuery("UPDATE Vampire SET CurrentHP = %d, HP = %d, SilverDamage = %d, ZoneID = %d, "
                                "XCoord = %d, YCoord = %d WHERE Name = '%s'",
                                record.currentHP, record.maxHP, record.silverDamage, record.zoneID, record.x, record.y,
                                ownerName.c_str());
            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)
    }

    void saveOustersVitals(const string& ownerName, const OustersVitalsRecord& record) {
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            pStmt->executeQuery("UPDATE Ousters SET CurrentHP = %d, HP = %d, CurrentMP = %d, MP = %d, ZoneID = %d, "
                                "XCoord = %d, YCoord = %d WHERE Name = '%s'",
                                record.currentHP, record.maxHP, record.currentMP, record.maxMP, record.zoneID, record.x,
                                record.y, ownerName.c_str());
            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)
    }

    void saveSlayerExps(const string& ownerName, const SlayerExpsRecord& record) {
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            pStmt->executeQuery(
                "UPDATE Slayer SET STRGoalExp=%u, DEXGoalExp=%u, INTGoalExp=%u, BladeGoalExp=%u, SwordGoalExp=%u, "
                "GunGoalExp=%u, EnchantGoalExp=%u, HealGoalExp=%u, ETCGoalExp=%u, Alignment=%d, Fame=%u, "
                "`Rank`=%d, "
                "RankGoalExp=%u, AdvancementClass=%u, AdvancementGoalExp=%d, AdvancedSTR=%u, AdvancedDEX=%u, "
                "AdvancedINT=%u, Bonus=%u WHERE Name='%s'",
                record.strGoalExp, record.dexGoalExp, record.intGoalExp, record.bladeGoalExp, record.swordGoalExp,
                record.gunGoalExp, record.enchantGoalExp, record.healGoalExp, record.etcGoalExp, record.alignment,
                record.fame, record.rank, record.rankGoalExp, record.advancementClass, record.advancementGoalExp,
                record.advancedSTR, record.advancedDEX, record.advancedINT, record.advancedAttrBonus,
                ownerName.c_str());
            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)
    }

    void saveExps(const string& ownerName, CharacterRace race, const CharacterExpsRecord& record) {
        Statement* pStmt = NULL;

        // The SilverDamage clause is the only text the two rows do not
        // share, and each race's spelling of it is preserved: an ousters'
        // row always takes the column, a vampire's only when the value is
        // non-zero, and the vampire fragment carries no space after its
        // comma.
        char silverDam[40];
        if (race == CHARACTER_RACE_OUSTERS)
            snprintf(silverDam, sizeof(silverDam), ", SilverDamage = %d", record.silverDamage);
        else if (record.silverDamage != 0)
            snprintf(silverDam, sizeof(silverDam), ",SilverDamage = %d", record.silverDamage);
        else
            silverDam[0] = '\0';

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            pStmt->executeQuery("UPDATE %s SET Alignment=%d, Fame=%d, GoalExp=%u%s, `Rank`=%d, RankGoalExp=%u, "
                                "AdvancementClass=%u, AdvancementGoalExp=%d WHERE Name='%s'",
                                characterRaceTable(race), record.alignment, record.fame, record.goalExp, silverDam,
                                record.rank, record.rankGoalExp, record.advancementClass, record.advancementGoalExp,
                                ownerName.c_str());
            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)
    }

    void tinysave(const string& ownerName, CharacterRace race, const string& fieldFragment) {
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            if (race == CHARACTER_RACE_SLAYER)
                pStmt->executeQuery("UPDATE Slayer SET %s WHERE NAME='%s'", fieldFragment.c_str(), ownerName.c_str());
            else if (race == CHARACTER_RACE_VAMPIRE)
                pStmt->executeQuery("UPDATE Vampire SET %s WHERE Name='%s'", fieldFragment.c_str(), ownerName.c_str());
            else
                pStmt->executeQuery("UPDATE Ousters SET %s WHERE Name='%s'", fieldFragment.c_str(), ownerName.c_str());
            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)
    }

private:
    // Both Level reads go through this helper, so END_DB's DBError.log line
    // names it rather than the calling method.
    static bool loadLevel(const char* format, const string& name, int& level) {
        bool found = false;
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = de::serverContext().database().getConnection("DARKEDEN")->createStatement();
            Result* pResult = pStmt->executeQuery(format, name.c_str());

            if (pResult->next()) {
                level = pResult->getInt(1);
                found = true;
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)

        return found;
    }
};

} // namespace

CharacterRepository& defaultCharacterRepository() {
    static MySQLCharacterRepository instance;
    return instance;
}
