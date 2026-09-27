// MySQL-backed integration tier for the gameserver repositories: runs the
// REAL MySQL implementations against a throwaway MySQL 5.7 loaded with the
// initdb/ schema and the production sql_mode. This is the authority the
// fakes in tests/support/ are corrected against — every quirk a fake pins
// is pinned HERE first, against the actual server.
//
// Not part of the default ctest suite: it needs a database, which
// tests/integration/mysql_test.sh provides (a handrolled container
// fixture — start MySQL, wait for the schema import, run this binary on
// the same docker network, tear down). Run `make integration-test` from
// the host.
//
// Characters come from PlayerFixtures.h: ready-made low/mid/high level
// profiles per race. Each test persists exactly the profiles it wants —
// an unpersisted profile doubles as the "character with no rows" case.
//
// Wiring mirrors a zone thread: one world connection and one "dist"
// connection registered for this thread — two sockets, same server, same
// DARKEDEN schema, exactly like production (getDistConnection ignores its
// name argument; see MySQLGoodsRepository.cpp).

#include <cstdlib>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "PlayerFixtures.h"
#include "ServerContext.h"
#include "Thread.h"
#include "guild/GuildUnionJoinOffer.h"
#include "repository/BalanceInfoRepository.h"
#include "repository/BloodBibleSignRepository.h"
#include "repository/BulletinBoardRepository.h"
#include "repository/CharacterPurgeRepository.h"
#include "repository/CharacterRepository.h"
#include "repository/ComebackEventRepository.h"
#include "repository/ContentInfoRepository.h"
#include "repository/CoupleRepository.h"
#include "repository/EffectSaveRepository.h"
#include "repository/FlagSetRepository.h"
#include "repository/FlagWarRepository.h"
#include "repository/FriendRepository.h"
#include "repository/GameInfoRepository.h"
#include "repository/GoldRepository.h"
#include "repository/GoodsRepository.h"
#include "repository/GuildRepository.h"
#include "repository/ItemObjectRepository.h"
#include "repository/ItemRepository.h"
#include "repository/MessageRepository.h"
#include "repository/MofusPointRepository.h"
#include "repository/NicknameRepository.h"
#include "repository/PlayRecordRepository.h"
#include "repository/QuestInfoRepository.h"
#include "repository/QuestItemRepository.h"
#include "repository/RankBonusRepository.h"
#include "repository/RegenZoneRepository.h"
#include "repository/SMSAddressRepository.h"
#include "repository/SessionRepository.h"
#include "repository/SkillSaveRepository.h"
#include "repository/SpecialEventRepository.h"
#include "repository/StashRepository.h"
#include "repository/SystemAvailabilityRepository.h"
#include "repository/WarInfoRepository.h"
#include "repository/ZoneInfoRepository.h"

namespace {

std::string env(const char* name, const char* fallback) {
    const char* value = getenv(name);
    return value != NULL && *value != '\0' ? value : fallback;
}

// --- RankBonusData against real MySQL -------------------------------------

class RankBonusMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        PlayerFixtures::removeAll();
        execSQL("DELETE FROM RankBonusData WHERE OwnerID IN ('itvampmid', 'itslaymid')");
    }
};

TEST_F(RankBonusMySQL, LoadTypesComeBackTypeAscendingNotInsertionOrder) {
    // No ORDER BY on the query, but the covering index (OwnerID, Type)
    // fully serves it: InnoDB's index scan returns Type order.
    PlayerFixture vampire = PlayerFixtures::midLevelVampire();
    vampire.persist();

    RankBonusRepository& repository = defaultRankBonusRepository();
    repository.insert(vampire.name, 7);
    repository.insert(vampire.name, 3);

    std::vector<DWORD> types = repository.loadTypes(vampire.name);
    ASSERT_EQ(2u, types.size());
    EXPECT_EQ(3u, types[0]);
    EXPECT_EQ(7u, types[1]);
}

TEST_F(RankBonusMySQL, KeylessTableStoresDuplicatesAndDeleteOneRemovesThemAll) {
    PlayerFixture vampire = PlayerFixtures::midLevelVampire();
    vampire.persist();

    RankBonusRepository& repository = defaultRankBonusRepository();
    repository.insert(vampire.name, 7);
    repository.insert(vampire.name, 7); // no PK, no unique key: second row lands
    repository.insert(vampire.name, 3);
    ASSERT_EQ(3u, repository.loadTypes(vampire.name).size());

    repository.deleteOne(vampire.name, 7);

    std::vector<DWORD> types = repository.loadTypes(vampire.name);
    ASSERT_EQ(1u, types.size());
    EXPECT_EQ(3u, types[0]);
}

TEST_F(RankBonusMySQL, DeleteAllClearsOnlyThatOwner) {
    PlayerFixture vampire = PlayerFixtures::midLevelVampire();
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    vampire.persist();
    slayer.persist();

    RankBonusRepository& repository = defaultRankBonusRepository();
    repository.insert(vampire.name, 7);
    repository.insert(slayer.name, 7);

    repository.deleteAll(vampire.name);

    EXPECT_TRUE(repository.loadTypes(vampire.name).empty());
    EXPECT_EQ(1u, repository.loadTypes(slayer.name).size());
}

// --- stash columns against real MySQL -------------------------------------

class StashMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        PlayerFixtures::removeAll();
    }
};

TEST_F(StashMySQL, NonOustersSaveWritesSlayerAndVampireTables) {
    // Character creation gives every non-Ousters character a Slayer AND a
    // Vampire row; the save hits both — the Slayer-unconditional quirk.
    PlayerFixture slayer = PlayerFixtures::lowLevelSlayer();
    slayer.persist();

    defaultStashRepository().saveStashGold(slayer.name, false, 1234);

    int gold = -1;
    ASSERT_TRUE(defaultStashRepository().loadStashGold(slayer.name, CHARACTER_RACE_SLAYER, gold));
    EXPECT_EQ(1234, gold);
    ASSERT_TRUE(defaultStashRepository().loadStashGold(slayer.name, CHARACTER_RACE_VAMPIRE, gold));
    EXPECT_EQ(1234, gold);
}

TEST_F(StashMySQL, OustersSaveWritesSlayerAndOustersTables) {
    PlayerFixture ousters = PlayerFixtures::midLevelOusters();
    ousters.persist();

    defaultStashRepository().saveStashNum(ousters.name, true, 3);

    EXPECT_EQ("3", queryScalar("SELECT StashNum FROM Slayer WHERE Name='" + ousters.name + "'"));
    EXPECT_EQ("3", queryScalar("SELECT StashNum FROM Ousters WHERE Name='" + ousters.name + "'"));
}

TEST_F(StashMySQL, SaveAgainstMissingRowsIsASilentNoOp) {
    // Deliberately NOT persisted: the profile stands in for a character
    // with no rows. The UPDATEs match zero rows — no error, no warning.
    PlayerFixture ghost = PlayerFixtures::highLevelVampire();

    defaultStashRepository().saveStashGold(ghost.name, false, 500);

    int gold = -1;
    EXPECT_FALSE(defaultStashRepository().loadStashGold(ghost.name, CHARACTER_RACE_VAMPIRE, gold));
}

TEST_F(StashMySQL, GoldAboveIntMaxClampsToZeroDestroyingTheBalance) {
    // The (int) cast emits a NEGATIVE literal; the UNSIGNED StashGold
    // column clamps it to 0 (warning 1264) under the non-strict sql_mode.
    // NOT stored negative — the balance is destroyed. Unreachable with
    // the MAX_MONEY cap, pinned anyway.
    PlayerFixture vampire = PlayerFixtures::highLevelVampire();
    vampire.persist();
    defaultStashRepository().saveStashGold(vampire.name, false, 5000); // a real balance first

    defaultStashRepository().saveStashGold(vampire.name, false, 4000000000u);

    int gold = -1;
    ASSERT_TRUE(defaultStashRepository().loadStashGold(vampire.name, CHARACTER_RACE_VAMPIRE, gold));
    EXPECT_EQ(0, gold);
}

// --- system-availability flags against real MySQL --------------------------

TEST(SystemAvailabilityMySQL, TheBootReadTakesTheFirstTwoColumnsOfASelectStar) {
    // The statement is "SELECT *" and the caller reads columns 1 and 2
    // POSITIONALLY, so a column inserted before them would silently
    // reassign every system flag. Both callers are compiled out of this
    // build, so the compiler will never notice; this test is the only
    // thing that would.
    SystemAvailabilityRepository& repository = defaultSystemAvailabilityRepository();

    execSQL("DELETE FROM SystemAvailabilities WHERE SystemKind >= 31000");
    execSQL("INSERT INTO SystemAvailabilities (SystemKind, Available, Description) "
            "VALUES (31000, 7, 'it-availability')");

    std::vector<SystemAvailabilityRow> rows = repository.loadAll();

    bool seen = false;
    for (size_t r = 0; r < rows.size(); r++) {
        if (rows[r].systemKind == 31000) {
            seen = true;
            // 7, not 1: the caller turns this into a bool for most systems
            // but reads it as an integer for the three limit rows, so the
            // repository must hand back the column rather than a flag.
            EXPECT_EQ(7, rows[r].available);
        }
    }
    EXPECT_TRUE(seen) << "the seeded row did not come back — has a column been "
                         "inserted before SystemKind or Available?";

    // The seeded production rows come back too, which is what the boot
    // path depends on.
    EXPECT_FALSE(rows.empty());

    repository.deleteSystemKind(31000);
    EXPECT_EQ("0", queryScalar("SELECT count(*) FROM SystemAvailabilities WHERE SystemKind=31000"));

    // The delete is scoped to the one kind, so the seeded rows survive it.
    EXPECT_NE("0", queryScalar("SELECT count(*) FROM SystemAvailabilities"));
}

// --- the friend list against real MySQL ------------------------------------

// Every case here pins a FAILURE, and that is deliberate. FriendList and
// FriendHistory do not exist: initdb/DARKEDEN.sql defines 374 tables and
// neither is among them, in that schema or in USERINFO.sql, and the only
// mention of either name anywhere in the tree is the handler these
// statements came from. So the friend feature has never worked against
// the shipped schema, and no success path is reachable to assert.
//
// Whoever adds the tables should expect these cases to fail, and should
// replace them with the round-trip assertions they were always meant to
// be: insert a mutual pair and read it back, spool a message and drain
// it, blacklist and check both directions.
class FriendMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        clean();
    }
    virtual void TearDown() {
        clean();
    }
    static void clean() {
        // No-ops while the tables are absent, and correct the moment
        // they are added — which is the scenario this fixture exists for.
        execSQLIgnoringErrors("DELETE FROM FriendList WHERE Owner_Name LIKE 'it-%'");
        execSQLIgnoringErrors("DELETE FROM FriendHistory WHERE Owner_Name LIKE 'it-%'");
    }
};

TEST_F(FriendMySQL, EveryReadRaisesBecauseTheTableIsNotInTheSchema) {
    FriendRepository& repository = defaultFriendRepository();

    // Catch the DatabaseError END_DB throws rather than using
    // EXPECT_ANY_THROW, which would pass for any failure at all. Be
    // precise about what that buys: it pins the failure to the SQL
    // layer, and nothing more. It CANNOT tell a missing table from a
    // dropped connection — Statement::executeQuery raises
    // SQLQueryException for both, and END_DB converts both to the same
    // DatabaseError.
    bool refused = false;
    try {
        repository.loadFriends("it-friend");
    } catch (const DatabaseError&) {
        refused = true;
    }
    EXPECT_TRUE(refused) << "loadFriends succeeded, so FriendList now exists — see this fixture's comment";

    refused = false;
    try {
        repository.loadMessages("it-friend");
    } catch (const DatabaseError&) {
        refused = true;
    }
    EXPECT_TRUE(refused) << "loadMessages succeeded, so FriendHistory now exists";

    refused = false;
    try {
        repository.friendExists("it-friend", "it-other");
    } catch (const DatabaseError&) {
        refused = true;
    }
    EXPECT_TRUE(refused) << "friendExists succeeded, so FriendList now exists";
}

TEST_F(FriendMySQL, EveryWriteRaisesToo) {
    FriendRepository& repository = defaultFriendRepository();

    bool refused = false;
    try {
        repository.insertFriend("it-friend", "it-other");
    } catch (const DatabaseError&) {
        refused = true;
    }
    EXPECT_TRUE(refused) << "insertFriend succeeded, so FriendList now exists";

    refused = false;
    try {
        repository.insertMessage("hello", "it-other", "it-friend");
    } catch (const DatabaseError&) {
        refused = true;
    }
    EXPECT_TRUE(refused) << "insertMessage succeeded, so FriendHistory now exists";

    // The deletes fail the same way, which matters because the handler
    // runs them without checking anything first.
    refused = false;
    try {
        repository.deleteFriend("it-friend", "it-other");
    } catch (const DatabaseError&) {
        refused = true;
    }
    EXPECT_TRUE(refused) << "deleteFriend succeeded, so FriendList now exists";

    // The remaining three. insertBlacklisted and
    // hasBlacklisted are the only statements CG_ADD_FRIEND_BLACK
    // reaches, and deleteMessages is the one the IsHave flag gates, so
    // without these a table-adder would get no signal from that whole
    // branch.
    refused = false;
    try {
        repository.insertBlacklisted("it-other", "it-friend");
    } catch (const DatabaseError&) {
        refused = true;
    }
    EXPECT_TRUE(refused) << "insertBlacklisted succeeded, so FriendList now exists";

    refused = false;
    try {
        repository.hasBlacklisted("it-other", "it-friend");
    } catch (const DatabaseError&) {
        refused = true;
    }
    EXPECT_TRUE(refused) << "hasBlacklisted succeeded, so FriendList now exists";

    refused = false;
    try {
        repository.deleteMessages("it-friend");
    } catch (const DatabaseError&) {
        refused = true;
    }
    EXPECT_TRUE(refused) << "deleteMessages succeeded, so FriendHistory now exists";
}

// --- capture-the-flag against real MySQL -----------------------------------

class FlagWarMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        clean();
    }
    virtual void TearDown() {
        clean();
    }
    static void clean() {
        execSQL("DELETE FROM FlagPolePosition WHERE ZoneID >= 31000");
        // The repository's wipe is unconditional, so the tier owns the
        // whole table for the run. (Name LIKE 'it-%' would scope it, as
        // the FlagWarHistory line below does; owning the table is what
        // lets the wipe test seed a row it does not own.)
        execSQL("DELETE FROM FlagWarStat");
        execSQL("DELETE FROM FlagWarHistory WHERE FlagWarID LIKE 'it-%'");
    }
};

TEST_F(FlagWarMySQL, ThePoleLoadTurnsTheRaceEnumIntoAZeroBasedIndex) {
    // FlagPolePosition.Race is an ENUM('SLAYER','VAMPIRE','OUSTERS'), and
    // the statement selects "Race-1". MySQL evaluates arithmetic on an
    // ENUM against its 1-BASED index, so the "-1" is what makes the
    // value line up with Race_t. Getting this wrong would shift every
    // flag pole one race over, which is why it is pinned here rather
    // than left to the comment.
    execSQL("INSERT INTO FlagPolePosition (ZoneID, CenterX, CenterY, Width, Height, Race, MonsterType) "
            "VALUES (31000, 10, 20, 3, 4, 'SLAYER', 77)");
    execSQL("INSERT INTO FlagPolePosition (ZoneID, CenterX, CenterY, Width, Height, Race, MonsterType) "
            "VALUES (31001, 11, 21, 5, 6, 'VAMPIRE', 78)");
    execSQL("INSERT INTO FlagPolePosition (ZoneID, CenterX, CenterY, Width, Height, Race, MonsterType) "
            "VALUES (31002, 12, 22, 7, 8, 'OUSTERS', 79)");

    std::vector<FlagPoleRow> poles = defaultFlagWarRepository().loadFlagPoles();

    // The table carries the production seed rows too, so pick out ours.
    int seen = 0;
    for (size_t r = 0; r < poles.size(); r++) {
        if (poles[r].zoneID == 31000) {
            seen++;
            EXPECT_EQ(10, poles[r].centerX);
            EXPECT_EQ(20, poles[r].centerY);
            EXPECT_EQ(3, poles[r].width);
            EXPECT_EQ(4, poles[r].height);
            EXPECT_EQ(0, poles[r].race); // SLAYER is enum index 1
            EXPECT_EQ(77, poles[r].monsterType);
        }
        if (poles[r].zoneID == 31001) {
            seen++;
            EXPECT_EQ(1, poles[r].race); // VAMPIRE
        }
        if (poles[r].zoneID == 31002) {
            seen++;
            EXPECT_EQ(2, poles[r].race); // OUSTERS
        }
    }
    EXPECT_EQ(3, seen);
}

TEST_F(FlagWarMySQL, ThePerFlagTallyIsCheckedBeforeInsertBecauseNothingEnforcesIt) {
    FlagWarRepository& repository = defaultFlagWarRepository();

    EXPECT_FALSE(repository.flagStatExists("it-carl", 4242));
    repository.insertFlagStat("it-acc1", "it-carl", 1, 7, 4242);
    EXPECT_TRUE(repository.flagStatExists("it-carl", 4242));

    // The probe is keyed on BOTH columns: the same player with another
    // flag, and another player with this flag, are both misses.
    EXPECT_FALSE(repository.flagStatExists("it-carl", 4243));
    EXPECT_FALSE(repository.flagStatExists("it-dana", 4242));

    // (Name, ItemID) is an INDEX, not a unique constraint, so the
    // database would happily take a second identical row. That is
    // exactly why the caller probes first, and why the probe is part of
    // the repository rather than a detail of the handler.
    repository.insertFlagStat("it-acc1", "it-carl", 1, 7, 4242);
    EXPECT_EQ("2", queryScalar("SELECT count(*) FROM FlagWarStat WHERE Name='it-carl' AND ItemID=4242"));
}

TEST_F(FlagWarMySQL, TheHistoryRollUpIsRefusedByOnlyFullGroupByAndTheWipeTakesEverything) {
    FlagWarRepository& repository = defaultFlagWarRepository();

    // Two flags for one player on server 7, one on server 8, one for
    // another player. The GROUP BY is on (Name, ServerID), so the same
    // name on two servers is two rows.
    repository.insertFlagStat("it-acc1", "it-carl", 1, 7, 1);
    repository.insertFlagStat("it-acc1", "it-carl", 1, 7, 2);
    repository.insertFlagStat("it-acc1", "it-carl", 1, 8, 3);
    repository.insertFlagStat("it-acc2", "it-dana", 2, 7, 4);

    // THE ROLL-UP CANNOT RUN. The statement groups by (Name, ServerID)
    // while selecting PlayerID and Race bare, and this project's
    // required sql_mode KEEPS ONLY_FULL_GROUP_BY (it removes
    // NO_ZERO_DATE and STRICT_TRANS_TABLES, not this one), so MySQL
    // refuses it with error 1055 and the read throws. Which means no
    // flag-war history is recorded on a correctly configured server,
    // and never has been.
    //
    // Task 3.2 moves statements without fixing them, so the throw is
    // what this tier pins. Anyone who fixes the GROUP BY should expect
    // this assertion to be the thing that fails.
    // Pin more than "something threw": catch the DatabaseError END_DB
    // throws, so a failure raising some other type would fail here rather
    // than quietly validating the claim. The error owns its message, which
    // is the line END_DB wrote to DBError.log, so the refusal MySQL gave
    // reads back here — the statement that was refused and the reason.
    bool refused = false;
    std::string reported;
    try {
        repository.loadFlagWarStatTotals();
    } catch (const DatabaseError& error) {
        refused = true;
        reported = error.message();
    }
    EXPECT_TRUE(refused) << "loadFlagWarStatTotals did not raise the ONLY_FULL_GROUP_BY refusal";
    EXPECT_NE(std::string::npos, reported.find("loadFlagWarStatTotals")) << "message was: " << reported;
    EXPECT_NE(std::string::npos, reported.find("only_full_group_by")) << "message was: " << reported;

    // The tally itself is fine. The history writer is fed BY HAND
    // below with what the roll-up would have produced, so the writer is
    // covered without pretending the reader is — nothing was actually
    // rolled into those rows.
    EXPECT_EQ("2", queryScalar("SELECT count(*) FROM FlagWarStat WHERE Name='it-carl' AND ServerID=7"));
    EXPECT_EQ("1", queryScalar("SELECT count(*) FROM FlagWarStat WHERE Name='it-carl' AND ServerID=8"));
    EXPECT_EQ("1", queryScalar("SELECT count(*) FROM FlagWarStat WHERE Name='it-dana' AND ServerID=7"));

    repository.insertFlagWarHistory("it-round1", "it-acc1", "it-carl", 1, 7, 2);
    repository.insertFlagWarHistory("it-round1", "it-acc1", "it-carl", 1, 8, 1);
    repository.insertFlagWarHistory("it-round1", "it-acc2", "it-dana", 2, 7, 1);
    EXPECT_EQ("3", queryScalar("SELECT count(*) FROM FlagWarHistory WHERE FlagWarID='it-round1'"));
    EXPECT_EQ("2", queryScalar("SELECT FlagNum FROM FlagWarHistory "
                               "WHERE FlagWarID='it-round1' AND Name='it-carl' AND ServerID=7"));

    // The wipe is UNCONDITIONAL — no WHERE at all. Seed a row the test
    // does not own, so that a hypothetical scoped implementation
    // (WHERE Name LIKE 'it-%') would leave it behind and fail here;
    // without this row the assertion could not tell the two apart.
    execSQL("INSERT INTO FlagWarStat (PlayerID, Name, Race, ServerID, ItemID) "
            "VALUES ('other-acc', 'other', 0, 9, 99)");
    repository.deleteAllFlagWarStats();
    EXPECT_EQ("0", queryScalar("SELECT count(*) FROM FlagWarStat"));
    EXPECT_EQ("3", queryScalar("SELECT count(*) FROM FlagWarHistory WHERE FlagWarID='it-round1'"));
}

// --- mofus power points against real MySQL ---------------------------------

class MofusMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        clean();
    }
    virtual void TearDown() {
        clean();
    }
    static void clean() {
        execSQL("DELETE FROM MofusPowerPoint WHERE OwnerID LIKE 'it-%'");
        execSQL("DELETE FROM MofusLog WHERE OwnerID LIKE 'it-%'");
    }
};

TEST_F(MofusMySQL, TheFirstSaveInsertsAndEveryLaterOneAccumulates) {
    MofusPointRepository& repository = defaultMofusPointRepository();

    // No row yet: the UPDATE matches nothing, which is exactly how the
    // caller knows to insert. OwnerID is the PRIMARY KEY, so the insert
    // can only happen once.
    int point = -1;
    EXPECT_FALSE(repository.loadPowerPoint("it-mofus", point));
    EXPECT_EQ(-1, point); // left alone when there is no row

    EXPECT_FALSE(repository.increasePowerPoint("it-mofus", 30));
    EXPECT_EQ("0", queryScalar("SELECT count(*) FROM MofusPowerPoint WHERE OwnerID='it-mofus'"));

    repository.insertPowerPoint("it-mofus", 30);
    ASSERT_TRUE(repository.loadPowerPoint("it-mofus", point));
    EXPECT_EQ(30, point);

    // Now the UPDATE matches, so the caller does NOT insert again.
    EXPECT_TRUE(repository.increasePowerPoint("it-mofus", 12));
    ASSERT_TRUE(repository.loadPowerPoint("it-mofus", point));
    EXPECT_EQ(42, point);

    // The balance is relative and signed: Point is int(11) SIGNED, so a
    // negative amount really does subtract and can go below zero. That is
    // the inline statement's arithmetic, kept.
    EXPECT_TRUE(repository.increasePowerPoint("it-mofus", -50));
    ASSERT_TRUE(repository.loadPowerPoint("it-mofus", point));
    EXPECT_EQ(-8, point);

    // The live shape the callers actually produce: a FIRST save
    // that is a spend, which creates the row already negative. That is
    // CGUsePowerPointHandler's -300 against a character who has never
    // received a mofus credit.
    EXPECT_FALSE(repository.increasePowerPoint("it-spend", -300));
    repository.insertPowerPoint("it-spend", -300);
    ASSERT_TRUE(repository.loadPowerPoint("it-spend", point));
    EXPECT_EQ(-300, point);

    // And the quirk that sends it there: affected-rows counts rows
    // CHANGED, not matched, because the driver sets no
    // CLIENT_FOUND_ROWS. Adding zero to an EXISTING row reports false,
    // so the caller would insert against the primary key and raise —
    // which the mofus swallow then eats.
    EXPECT_FALSE(repository.increasePowerPoint("it-spend", 0));
    bool refused = false;
    try {
        repository.insertPowerPoint("it-spend", 0);
    } catch (const DatabaseError&) {
        refused = true;
    }
    EXPECT_TRUE(refused) << "the duplicate-key insert did not raise";
}

TEST_F(MofusMySQL, TheAuditRowIsStampedByTheDatabaseAndKeepsEverySave) {
    MofusPointRepository& repository = defaultMofusPointRepository();

    repository.logPowerPoint("it-mofus", 5, 7);
    repository.logPowerPoint("it-mofus", 9, 11);

    // MofusLog has no unique key, so every save appends — it is an audit
    // trail, not a balance.
    EXPECT_EQ("2", queryScalar("SELECT count(*) FROM MofusLog WHERE OwnerID='it-mofus'"));
    EXPECT_EQ("7", queryScalar("SELECT SavePoint FROM MofusLog WHERE OwnerID='it-mofus' AND RecvPoint=5"));
    EXPECT_EQ("11", queryScalar("SELECT SavePoint FROM MofusLog WHERE OwnerID='it-mofus' AND RecvPoint=9"));

    // SaveTime is now(), stamped by the database rather than the caller.
    EXPECT_EQ(queryScalar("SELECT CURDATE()"),
              queryScalar("SELECT DATE(SaveTime) FROM MofusLog WHERE OwnerID='it-mofus' AND RecvPoint=5"));
}

// --- couple pairings against real MySQL ------------------------------------

class CoupleMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        clean();
    }
    virtual void TearDown() {
        clean();
    }
    static void clean() {
        execSQL("DELETE FROM CoupleInfo WHERE MalePartnerName LIKE 'it-%' OR FemalePartnerName LIKE 'it-%'");
    }
};

TEST_F(CoupleMySQL, APairingIsOneRowFoundFromEitherSide) {
    CoupleRepository& repository = defaultCoupleRepository();

    // The row carries one column per sex, so the SAME row answers both
    // partners' probes — each naming its own column. This is the property
    // the sex-derived column names exist for, and the one a mixed-up
    // derivation would break.
    repository.insertCouple(MALE, "it-mike", FEMALE, "it-fay", 0);

    EXPECT_EQ("it-mike", queryScalar("SELECT MalePartnerName FROM CoupleInfo WHERE FemalePartnerName='it-fay'"));
    EXPECT_EQ("it-fay", queryScalar("SELECT FemalePartnerName FROM CoupleInfo WHERE MalePartnerName='it-mike'"));

    EXPECT_EQ(1, repository.countPairingWithPartner(MALE, "it-mike", "it-fay"));
    EXPECT_EQ(1, repository.countPairingWithPartner(FEMALE, "it-fay", "it-mike"));
    // The other derivation, each column from its own character's sex.
    // Same row, either way round.
    EXPECT_EQ(1, repository.countPairing(MALE, "it-mike", FEMALE, "it-fay"));
    EXPECT_EQ(1, repository.countPairing(FEMALE, "it-fay", MALE, "it-mike"));

    EXPECT_EQ(1, repository.countPairingsOf(MALE, "it-mike"));
    EXPECT_EQ(1, repository.countPairingsOf(FEMALE, "it-fay"));

    std::string partner;
    ASSERT_TRUE(repository.loadPartnerName(MALE, "it-mike", partner));
    EXPECT_EQ("it-fay", partner);
    ASSERT_TRUE(repository.loadPartnerName(FEMALE, "it-fay", partner));
    EXPECT_EQ("it-mike", partner);

    // CoupleDate is stamped by the database, not by the caller.
    EXPECT_EQ(queryScalar("SELECT CURDATE()"),
              queryScalar("SELECT CoupleDate FROM CoupleInfo WHERE MalePartnerName='it-mike'"));
}

TEST_F(CoupleMySQL, ProbesAreScopedToTheNamedPairAndMissAnyoneElse) {
    CoupleRepository& repository = defaultCoupleRepository();

    repository.insertCouple(MALE, "it-mike", FEMALE, "it-fay", 0);

    // Right names, wrong side: the male name looked up in the female
    // column finds nothing. Note precisely what this pins and what it
    // does not: every assertion below still holds if the column table
    // were swapped end for end, so this test establishes that the
    // derivation is sex-DEPENDENT, not that it is sex-CORRECT. The
    // orientation is pinned by the raw-SQL pair in the test above,
    // which reads MalePartnerName and FemalePartnerName by name.
    EXPECT_EQ(0, repository.countPairingsOf(FEMALE, "it-mike"));
    EXPECT_EQ(0, repository.countPairingWithPartner(MALE, "it-fay", "it-mike"));

    // A stranger, and a real character paired with the wrong partner.
    EXPECT_EQ(0, repository.countPairingsOf(MALE, "it-nobody"));
    EXPECT_EQ(0, repository.countPairingWithPartner(MALE, "it-mike", "it-eve"));

    std::string partner = "untouched";
    EXPECT_FALSE(repository.loadPartnerName(MALE, "it-nobody", partner));
    EXPECT_EQ("untouched", partner); // left alone when there is no row
}

TEST_F(CoupleMySQL, TheThreeDeletesDifferInWhatTheyMatchNotInWhatTheyMean) {
    CoupleRepository& repository = defaultCoupleRepository();

    // Race is part of every DELETE's WHERE, so a pairing of another race
    // survives all three. (Nothing stops two rows sharing a name across
    // races; the statements were always race-scoped.)
    repository.insertCouple(MALE, "it-mike", FEMALE, "it-fay", 0);
    repository.insertCouple(MALE, "it-mike", FEMALE, "it-fay", 1);

    repository.deletePairing(MALE, "it-mike", FEMALE, "it-fay", 0);
    // The DELETEs filter on Race; the count probes do NOT. So the
    // race-1 row survives the delete AND still answers the probe. That
    // asymmetry is deliberate: isCouple() and
    // hasCouple() see a pairing in ANY race, while removeCouple() only
    // removes the one matching the character's own.
    EXPECT_EQ(1, repository.countPairing(MALE, "it-mike", FEMALE, "it-fay"));
    EXPECT_EQ("1", queryScalar("SELECT count(*) FROM CoupleInfo WHERE MalePartnerName='it-mike'"));
    EXPECT_EQ("1", queryScalar("SELECT Race FROM CoupleInfo WHERE MalePartnerName='it-mike'"));

    // The counter-column form reaches the same row from one character
    // plus a partner NAME. Its only textual difference from the above is
    // a lower-case "where", which no assertion here can see (MySQL
    // cannot tell the spellings apart).
    repository.deletePairingWithPartner(MALE, "it-mike", "it-fay", 1);
    EXPECT_EQ(0, repository.countPairingsOf(MALE, "it-mike"));

    // The one-sided form takes every pairing of a character in a race.
    repository.insertCouple(MALE, "it-mike", FEMALE, "it-fay", 2);
    repository.insertCouple(MALE, "it-mike", FEMALE, "it-eve", 2);
    EXPECT_EQ(2, repository.countPairingsOf(MALE, "it-mike"));
    repository.deletePairingsOf(MALE, "it-mike", 2);
    EXPECT_EQ(0, repository.countPairingsOf(MALE, "it-mike"));
    EXPECT_EQ(0, repository.countPairingsOf(FEMALE, "it-fay"));
}

// --- carried gold against real MySQL --------------------------------------

class GoldMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        PlayerFixtures::removeAll();
    }
};

TEST_F(GoldMySQL, IncreaseAndDecreaseAreRelativeOnTheRowBalance) {
    PlayerFixture vampire = PlayerFixtures::midLevelVampire();
    vampire.persist(); // Vampire.Gold column default is 0

    defaultGoldRepository().increaseGold(vampire.name, CHARACTER_RACE_VAMPIRE, 500);
    defaultGoldRepository().decreaseGold(vampire.name, CHARACTER_RACE_VAMPIRE, 200);

    int gold = -1;
    ASSERT_TRUE(defaultGoldRepository().loadGold(vampire.name, CHARACTER_RACE_VAMPIRE, gold));
    EXPECT_EQ(300, gold);
}

TEST_F(GoldMySQL, OperationsTargetOnlyTheOwnRaceTableAndSlayerRowsStartAt2000) {
    // Unlike the stash writes, gold has NO Slayer fan-out — and the
    // Slayer table's Gold column DEFAULTS to 2000 while Vampire/Ousters
    // default to 0, so a fresh slayer's twin Vampire row sits at 0.
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    slayer.persist();

    defaultGoldRepository().increaseGold(slayer.name, CHARACTER_RACE_SLAYER, 500);

    int gold = -1;
    ASSERT_TRUE(defaultGoldRepository().loadGold(slayer.name, CHARACTER_RACE_SLAYER, gold));
    EXPECT_EQ(2500, gold);
    ASSERT_TRUE(defaultGoldRepository().loadGold(slayer.name, CHARACTER_RACE_VAMPIRE, gold));
    EXPECT_EQ(0, gold);
}

TEST_F(GoldMySQL, DecreaseBelowTheRowBalanceRaisesOutOfRangeAndLeavesTheRowUntouched) {
    // The caller clamps the delta against its IN-MEMORY balance; when the
    // row holds less (integrity drift), Gold - delta on the UNSIGNED
    // column raises ER_DATA_OUT_OF_RANGE (1690) — same failure shape as
    // taking a Num=0 goods row — and the row keeps its balance.
    PlayerFixture ousters = PlayerFixtures::lowLevelOusters();
    ousters.persist();
    defaultGoldRepository().increaseGold(ousters.name, CHARACTER_RACE_OUSTERS, 10);

    EXPECT_ANY_THROW(defaultGoldRepository().decreaseGold(ousters.name, CHARACTER_RACE_OUSTERS, 50));

    int gold = -1;
    ASSERT_TRUE(defaultGoldRepository().loadGold(ousters.name, CHARACTER_RACE_OUSTERS, gold));
    EXPECT_EQ(10, gold);
}

TEST_F(GoldMySQL, TheClampedGuildFeeZeroesARowThatCannotPayInsteadOfThrowing) {
    // SGAddGuildMemberOKHandler charges the guild-registration fee to a
    // character who is NOT logged in, so there is no in-memory balance to
    // clamp against and the clamp lives in the statement:
    // SET Gold = IF (fee > Gold, 0, Gold - fee). That makes it behave
    // differently from decreaseGold at exactly one point — a payer short
    // of the fee — and this pins both sides of that difference.
    PlayerFixture payer = PlayerFixtures::midLevelVampire();
    payer.persist();
    defaultGoldRepository().increaseGold(payer.name, CHARACTER_RACE_VAMPIRE, 1000);

    // Enough to pay: an ordinary subtraction.
    defaultGoldRepository().decreaseGoldClamped(payer.name, CHARACTER_RACE_VAMPIRE, 400);
    int gold = -1;
    ASSERT_TRUE(defaultGoldRepository().loadGold(payer.name, CHARACTER_RACE_VAMPIRE, gold));
    EXPECT_EQ(600, gold);

    // Short of the fee: emptied, silently. The caller cannot tell this
    // from a fee that was paid in full, and never could.
    defaultGoldRepository().decreaseGoldClamped(payer.name, CHARACTER_RACE_VAMPIRE, 5000);
    ASSERT_TRUE(defaultGoldRepository().loadGold(payer.name, CHARACTER_RACE_VAMPIRE, gold));
    EXPECT_EQ(0, gold);

    // The same shortfall through the relative write raises
    // ER_DATA_OUT_OF_RANGE and leaves the row alone. Two writers of one
    // column, two failure shapes — which is why they are two methods.
    defaultGoldRepository().increaseGold(payer.name, CHARACTER_RACE_VAMPIRE, 10);
    EXPECT_ANY_THROW(defaultGoldRepository().decreaseGold(payer.name, CHARACTER_RACE_VAMPIRE, 5000));
    ASSERT_TRUE(defaultGoldRepository().loadGold(payer.name, CHARACTER_RACE_VAMPIRE, gold));
    EXPECT_EQ(10, gold);

    // A name with no row is a silent no-op, like the other writes.
    PlayerFixture ghost = PlayerFixtures::highLevelSlayer(); // never persisted
    defaultGoldRepository().decreaseGoldClamped(ghost.name, CHARACTER_RACE_SLAYER, 100);
    EXPECT_FALSE(defaultGoldRepository().loadGold(ghost.name, CHARACTER_RACE_SLAYER, gold));
}

TEST_F(GoldMySQL, OperationsAgainstMissingRowsAreSilentNoOps) {
    PlayerFixture ghost = PlayerFixtures::highLevelSlayer(); // never persisted

    defaultGoldRepository().increaseGold(ghost.name, CHARACTER_RACE_SLAYER, 500);
    defaultGoldRepository().decreaseGold(ghost.name, CHARACTER_RACE_SLAYER, 500);

    int gold = -1;
    EXPECT_FALSE(defaultGoldRepository().loadGold(ghost.name, CHARACTER_RACE_SLAYER, gold));
}

// --- character-row saves against real MySQL -------------------------------
// CharacterRepository is write-only and has NO fake tier, so these tests
// carry the load: every written column is asserted with a distinct
// sentinel so argument transpositions cannot survive, and every dispatch
// branch is exercised. Known holes: the `Rank` backticks (load-bearing
// only on MySQL 8, where RANK is reserved — this tier runs 5.7) and the
// tinysave WHERE-casing (immaterial to MySQL) are not testable here; a
// signature break surfaces in the gameserver build, not in the default
// ctest suite (nothing there compiles this repository).

class CharacterMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        PlayerFixtures::removeAll();
    }
};

TEST_F(CharacterMySQL, TheWhisperProbeReadsOnlyTheAccountAndTakesTheFirstRow) {
    CharacterRepository& repository = defaultCharacterRepository();

    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();

    std::string playerID = "untouched";
    EXPECT_FALSE(repository.loadSlayerPlayerID(slayer.name, playerID));
    EXPECT_EQ("untouched", playerID);

    slayer.persist();
    execSQL("UPDATE Slayer SET PlayerID='it-acct' WHERE Name='" + slayer.name + "'");

    ASSERT_TRUE(repository.loadSlayerPlayerID(slayer.name, playerID));
    EXPECT_EQ("it-acct", playerID);

    // Distinct from loadSlayerAccount, which selects two columns and
    // requires EXACTLY one row. Both answer here, for the same row.
    std::string accountID;
    std::string race;
    ASSERT_TRUE(repository.loadSlayerAccount(slayer.name, accountID, race));
    EXPECT_EQ("it-acct", accountID);
}

TEST_F(CharacterMySQL, TheConnectProbeReadsTheAccountAndRaceOffTheSlayerRow) {
    CharacterRepository& repository = defaultCharacterRepository();

    PlayerFixture vampire = PlayerFixtures::midLevelVampire();

    std::string playerID = "untouched";
    std::string race = "untouched";
    EXPECT_FALSE(repository.loadSlayerAccount(vampire.name, playerID, race)); // no row yet
    EXPECT_EQ("untouched", playerID);                                         // left alone
    EXPECT_EQ("untouched", race);

    vampire.persist();
    execSQL("UPDATE Slayer SET PlayerID='it-acct', Race='VAMPIRE' WHERE Name='" + vampire.name + "'");

    ASSERT_TRUE(repository.loadSlayerAccount(vampire.name, playerID, race));
    EXPECT_EQ("it-acct", playerID);
    // A vampire's SLAYER row carries the real race — every character has
    // one whatever its race. That is the whole point of this probe: the
    // connect path takes the race from the database rather than trusting
    // the type the client sent.
    EXPECT_EQ("VAMPIRE", race);
}

TEST_F(CharacterMySQL, SlayerVitalsLandInTheSlayerRowInFull) {
    // Every written column is asserted with a distinct sentinel: an
    // argument transposition in the impl cannot survive this test.
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    slayer.persist();

    SlayerVitalsRecord record;
    record.currentHP = 111;
    record.maxHP = 222;
    record.currentMP = 33;
    record.maxMP = 44;
    record.zoneID = 21;
    record.x = 100;
    record.y = 200;
    defaultCharacterRepository().saveSlayerVitals(slayer.name, record);

    EXPECT_EQ("111", queryScalar("SELECT CurrentHP FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("222", queryScalar("SELECT HP FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("33", queryScalar("SELECT CurrentMP FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("44", queryScalar("SELECT MP FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("21", queryScalar("SELECT ZoneID FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("100", queryScalar("SELECT XCoord FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("200", queryScalar("SELECT YCoord FROM Slayer WHERE Name='" + slayer.name + "'"));
}

TEST_F(CharacterMySQL, VampireVitalsCarrySilverDamageInsteadOfMPInFull) {
    PlayerFixture vampire = PlayerFixtures::midLevelVampire();
    vampire.persist();

    VampireVitalsRecord record;
    record.currentHP = 150;
    record.maxHP = 300;
    record.silverDamage = 12;
    record.zoneID = 23;
    record.x = 50;
    record.y = 60;
    defaultCharacterRepository().saveVampireVitals(vampire.name, record);

    EXPECT_EQ("150", queryScalar("SELECT CurrentHP FROM Vampire WHERE Name='" + vampire.name + "'"));
    EXPECT_EQ("300", queryScalar("SELECT HP FROM Vampire WHERE Name='" + vampire.name + "'"));
    EXPECT_EQ("12", queryScalar("SELECT SilverDamage FROM Vampire WHERE Name='" + vampire.name + "'"));
    EXPECT_EQ("23", queryScalar("SELECT ZoneID FROM Vampire WHERE Name='" + vampire.name + "'"));
    EXPECT_EQ("50", queryScalar("SELECT XCoord FROM Vampire WHERE Name='" + vampire.name + "'"));
    EXPECT_EQ("60", queryScalar("SELECT YCoord FROM Vampire WHERE Name='" + vampire.name + "'"));
}

TEST_F(CharacterMySQL, OustersVitalsLandInTheOustersRowInFull) {
    PlayerFixture ousters = PlayerFixtures::lowLevelOusters();
    ousters.persist();

    OustersVitalsRecord record;
    record.currentHP = 90;
    record.maxHP = 120;
    record.currentMP = 70;
    record.maxMP = 80;
    record.zoneID = 51;
    record.x = 10;
    record.y = 20;
    defaultCharacterRepository().saveOustersVitals(ousters.name, record);

    EXPECT_EQ("90", queryScalar("SELECT CurrentHP FROM Ousters WHERE Name='" + ousters.name + "'"));
    EXPECT_EQ("120", queryScalar("SELECT HP FROM Ousters WHERE Name='" + ousters.name + "'"));
    EXPECT_EQ("70", queryScalar("SELECT CurrentMP FROM Ousters WHERE Name='" + ousters.name + "'"));
    EXPECT_EQ("80", queryScalar("SELECT MP FROM Ousters WHERE Name='" + ousters.name + "'"));
    EXPECT_EQ("51", queryScalar("SELECT ZoneID FROM Ousters WHERE Name='" + ousters.name + "'"));
    EXPECT_EQ("10", queryScalar("SELECT XCoord FROM Ousters WHERE Name='" + ousters.name + "'"));
    EXPECT_EQ("20", queryScalar("SELECT YCoord FROM Ousters WHERE Name='" + ousters.name + "'"));
}

TEST_F(CharacterMySQL, SlayerExpsTailLandsInFull) {
    PlayerFixture slayer = PlayerFixtures::highLevelSlayer();
    slayer.persist();

    SlayerExpsRecord record;
    record.strGoalExp = 1001;
    record.dexGoalExp = 1002;
    record.intGoalExp = 1003;
    record.bladeGoalExp = 2001;
    record.swordGoalExp = 2002;
    record.gunGoalExp = 2003;
    record.enchantGoalExp = 2004;
    record.healGoalExp = 2005;
    record.etcGoalExp = 2006;
    record.alignment = -50;
    record.fame = 777;
    record.rank = 9;
    record.rankGoalExp = 3001;
    record.advancementClass = 4;
    record.advancementGoalExp = 4001;
    record.advancedSTR = 11;
    record.advancedDEX = 12;
    record.advancedINT = 13;
    record.advancedAttrBonus = 14;
    defaultCharacterRepository().saveSlayerExps(slayer.name, record);

    // all 19 written columns, distinct sentinels — transpositions die here
    EXPECT_EQ("1001", queryScalar("SELECT STRGoalExp FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("1002", queryScalar("SELECT DEXGoalExp FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("1003", queryScalar("SELECT INTGoalExp FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("2001", queryScalar("SELECT BladeGoalExp FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("2002", queryScalar("SELECT SwordGoalExp FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("2003", queryScalar("SELECT GunGoalExp FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("2004", queryScalar("SELECT EnchantGoalExp FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("2005", queryScalar("SELECT HealGoalExp FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("2006", queryScalar("SELECT ETCGoalExp FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("-50", queryScalar("SELECT Alignment FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("777", queryScalar("SELECT Fame FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("9", queryScalar("SELECT `Rank` FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("3001", queryScalar("SELECT RankGoalExp FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("4", queryScalar("SELECT AdvancementClass FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("4001", queryScalar("SELECT AdvancementGoalExp FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("11", queryScalar("SELECT AdvancedSTR FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("12", queryScalar("SELECT AdvancedDEX FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("13", queryScalar("SELECT AdvancedINT FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("14", queryScalar("SELECT Bonus FROM Slayer WHERE Name='" + slayer.name + "'"));
}

TEST_F(CharacterMySQL, VampireExpsSkipSilverDamageWhenZero) {
    // The original composed an optional ",SilverDamage = %d" fragment: a
    // zero value leaves the column UNTOUCHED — this save cannot reset a
    // vampire's silver damage to zero.
    PlayerFixture vampire = PlayerFixtures::highLevelVampire();
    vampire.persist();
    execSQL("UPDATE Vampire SET SilverDamage = 7 WHERE Name='" + vampire.name + "'");

    CharacterExpsRecord record;
    record.alignment = 10;
    record.fame = 55;
    record.goalExp = 900;
    record.silverDamage = 0;
    record.rank = 3;
    record.rankGoalExp = 800;
    record.advancementClass = 1;
    record.advancementGoalExp = 700;
    defaultCharacterRepository().saveExps(vampire.name, CHARACTER_RACE_VAMPIRE, record);
    EXPECT_EQ("7", queryScalar("SELECT SilverDamage FROM Vampire WHERE Name='" + vampire.name + "'"));
    // the other seven written columns, distinct sentinels
    EXPECT_EQ("10", queryScalar("SELECT Alignment FROM Vampire WHERE Name='" + vampire.name + "'"));
    EXPECT_EQ("55", queryScalar("SELECT Fame FROM Vampire WHERE Name='" + vampire.name + "'"));
    EXPECT_EQ("900", queryScalar("SELECT GoalExp FROM Vampire WHERE Name='" + vampire.name + "'"));
    EXPECT_EQ("3", queryScalar("SELECT `Rank` FROM Vampire WHERE Name='" + vampire.name + "'"));
    EXPECT_EQ("800", queryScalar("SELECT RankGoalExp FROM Vampire WHERE Name='" + vampire.name + "'"));
    EXPECT_EQ("1", queryScalar("SELECT AdvancementClass FROM Vampire WHERE Name='" + vampire.name + "'"));
    EXPECT_EQ("700", queryScalar("SELECT AdvancementGoalExp FROM Vampire WHERE Name='" + vampire.name + "'"));

    record.silverDamage = 5;
    defaultCharacterRepository().saveExps(vampire.name, CHARACTER_RACE_VAMPIRE, record);
    EXPECT_EQ("5", queryScalar("SELECT SilverDamage FROM Vampire WHERE Name='" + vampire.name + "'"));
}

TEST_F(CharacterMySQL, ExpsVampireBranchHitsOnlyTheVampireTable) {
    // saveExps dispatches on the race the caller passes, so each branch
    // asserts the target row changed AND the twin Slayer row did not.
    PlayerFixture vampire = PlayerFixtures::lowLevelVampire();
    vampire.persist(); // Slayer + Vampire rows
    // Sentinels of their own, so the twin row's columns do not have to be
    // read as whatever the schema defaults them to.
    execSQL("UPDATE Slayer SET Alignment=-99, Fame=98 WHERE Name='" + vampire.name + "'");

    CharacterExpsRecord record;
    record.alignment = 10;
    record.fame = 55;
    record.goalExp = 900;
    record.silverDamage = 4;
    record.rank = 3;
    record.rankGoalExp = 800;
    record.advancementClass = 1;
    record.advancementGoalExp = 700;
    defaultCharacterRepository().saveExps(vampire.name, CHARACTER_RACE_VAMPIRE, record);

    EXPECT_EQ("10", queryScalar("SELECT Alignment FROM Vampire WHERE Name='" + vampire.name + "'"));
    EXPECT_EQ("-99", queryScalar("SELECT Alignment FROM Slayer WHERE Name='" + vampire.name + "'"));
    EXPECT_EQ("98", queryScalar("SELECT Fame FROM Slayer WHERE Name='" + vampire.name + "'"));
}

TEST_F(CharacterMySQL, ExpsOustersBranchHitsOnlyTheOustersTable) {
    PlayerFixture ousters = PlayerFixtures::lowLevelOusters();
    ousters.persist(); // Slayer + Ousters rows
    execSQL("UPDATE Slayer SET Alignment=-99, Fame=98 WHERE Name='" + ousters.name + "'");

    CharacterExpsRecord record;
    record.alignment = 10;
    record.fame = 55;
    record.goalExp = 900;
    record.silverDamage = 4;
    record.rank = 3;
    record.rankGoalExp = 800;
    record.advancementClass = 1;
    record.advancementGoalExp = 700;
    defaultCharacterRepository().saveExps(ousters.name, CHARACTER_RACE_OUSTERS, record);

    EXPECT_EQ("10", queryScalar("SELECT Alignment FROM Ousters WHERE Name='" + ousters.name + "'"));
    EXPECT_EQ("-99", queryScalar("SELECT Alignment FROM Slayer WHERE Name='" + ousters.name + "'"));
    EXPECT_EQ("98", queryScalar("SELECT Fame FROM Slayer WHERE Name='" + ousters.name + "'"));
}

TEST_F(CharacterMySQL, OustersExpsAlwaysWriteSilverDamage) {
    // Unlike the vampire's conditional fragment, the ousters save writes
    // SilverDamage unconditionally — zero included.
    PlayerFixture ousters = PlayerFixtures::midLevelOusters();
    ousters.persist();
    execSQL("UPDATE Ousters SET SilverDamage = 7 WHERE Name='" + ousters.name + "'");

    CharacterExpsRecord record;
    record.alignment = 10;
    record.fame = 55;
    record.goalExp = 900;
    record.silverDamage = 0;
    record.rank = 3;
    record.rankGoalExp = 800;
    record.advancementClass = 1;
    record.advancementGoalExp = 700;
    defaultCharacterRepository().saveExps(ousters.name, CHARACTER_RACE_OUSTERS, record);

    EXPECT_EQ("0", queryScalar("SELECT SilverDamage FROM Ousters WHERE Name='" + ousters.name + "'"));
    // the other seven written columns, distinct sentinels
    EXPECT_EQ("10", queryScalar("SELECT Alignment FROM Ousters WHERE Name='" + ousters.name + "'"));
    EXPECT_EQ("55", queryScalar("SELECT Fame FROM Ousters WHERE Name='" + ousters.name + "'"));
    EXPECT_EQ("900", queryScalar("SELECT GoalExp FROM Ousters WHERE Name='" + ousters.name + "'"));
    EXPECT_EQ("3", queryScalar("SELECT `Rank` FROM Ousters WHERE Name='" + ousters.name + "'"));
    EXPECT_EQ("800", queryScalar("SELECT RankGoalExp FROM Ousters WHERE Name='" + ousters.name + "'"));
    EXPECT_EQ("1", queryScalar("SELECT AdvancementClass FROM Ousters WHERE Name='" + ousters.name + "'"));
    EXPECT_EQ("700", queryScalar("SELECT AdvancementGoalExp FROM Ousters WHERE Name='" + ousters.name + "'"));
}

TEST_F(CharacterMySQL, TinysaveSlayerBranchHitsOnlyTheSlayerTable) {
    // tinysave is the repository's only dispatching method (~400 call sites,
    // including the absolute setGoldEx writes): every branch gets its
    // own test, each asserting the target row changed AND the twin row
    // did not.
    PlayerFixture slayer = PlayerFixtures::lowLevelSlayer();
    slayer.persist(); // Slayer + twin Vampire row

    defaultCharacterRepository().tinysave(slayer.name, CHARACTER_RACE_SLAYER, "StashNum=9");

    EXPECT_EQ("9", queryScalar("SELECT StashNum FROM Slayer WHERE Name='" + slayer.name + "'"));
    EXPECT_EQ("0", queryScalar("SELECT StashNum FROM Vampire WHERE Name='" + slayer.name + "'"));
}

TEST_F(CharacterMySQL, TinysaveVampireBranchHitsOnlyTheVampireTable) {
    PlayerFixture vampire = PlayerFixtures::lowLevelVampire();
    vampire.persist(); // Slayer + Vampire rows

    defaultCharacterRepository().tinysave(vampire.name, CHARACTER_RACE_VAMPIRE, "StashNum=9");

    EXPECT_EQ("9", queryScalar("SELECT StashNum FROM Vampire WHERE Name='" + vampire.name + "'"));
    EXPECT_EQ("0", queryScalar("SELECT StashNum FROM Slayer WHERE Name='" + vampire.name + "'"));
}

TEST_F(CharacterMySQL, TinysaveOustersBranchHitsOnlyTheOustersTable) {
    PlayerFixture ousters = PlayerFixtures::lowLevelOusters();
    ousters.persist(); // Slayer + Ousters rows

    defaultCharacterRepository().tinysave(ousters.name, CHARACTER_RACE_OUSTERS, "StashNum=9");

    EXPECT_EQ("9", queryScalar("SELECT StashNum FROM Ousters WHERE Name='" + ousters.name + "'"));
    EXPECT_EQ("0", queryScalar("SELECT StashNum FROM Slayer WHERE Name='" + ousters.name + "'"));
}

// --- character-row loads against real MySQL -------------------------------
// The loads are POSITIONAL (column N of the SELECT lands in field N of
// the record), so each test sets every column to its own SELECT position
// as the sentinel: a transposed pair in the column list or the reader
// swaps two values and fails here. Name is the one column left as
// persisted (it is the key).

TEST_F(CharacterMySQL, LoadSlayerReturnsEveryColumnInSelectPosition) {
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    slayer.persist();
    execSQL("UPDATE Slayer SET AdvancementClass=2, AdvancementGoalExp=3, Competence=4, CompetenceShape=5, "
            "Sex='FEMALE', MasterEffectColor=7, HairStyle='HAIR_STYLE2', HairColor=9, SkinColor=10, "
            "Phone='1234567', STR=12, STRGoalExp=13, DEX=14, DEXGoalExp=15, INTE=16, INTGoalExp=17, "
            "AdvancedSTR=18, AdvancedDEX=19, AdvancedINT=20, Bonus=21, `Rank`=22, RankGoalExp=23, CurrentHP=24, "
            "HP=25, CurrentMP=26, MP=27, Fame=28, Gold=29, GuildID=30, BladeLevel=31, BladeGoalExp=32, "
            "SwordLevel=33, SwordGoalExp=34, GunLevel=35, GunGoalExp=36, EnchantLevel=37, EnchantGoalExp=38, "
            "HealLevel=39, HealGoalExp=40, ETCLevel=41, ETCGoalExp=42, ZoneID=43, XCoord=44, YCoord=45, Sight=46, "
            "GunBonusExp=47, RifleBonusExp=48, Alignment=-49, StashGold=50, StashNum=51, ResurrectZone=52, "
            "Reward=-53, SMSCharge=54 WHERE Name='" +
            slayer.name + "'");

    SlayerLoadRecord record;
    ASSERT_TRUE(defaultCharacterRepository().loadSlayer(slayer.name, record));
    EXPECT_EQ(slayer.name, record.name);
    EXPECT_EQ(2, record.advancementClass);
    EXPECT_EQ(3, record.advancementGoalExp);
    EXPECT_EQ(4, record.competence);
    EXPECT_EQ(5, record.competenceShape);
    EXPECT_EQ("FEMALE", record.sex);
    EXPECT_EQ(7, record.masterEffectColor);
    EXPECT_EQ("HAIR_STYLE2", record.hairStyle);
    EXPECT_EQ(9, record.hairColor);
    EXPECT_EQ(10, record.skinColor);
    EXPECT_EQ("1234567", record.phone);
    EXPECT_EQ(12, record.str);
    EXPECT_EQ(13, record.strGoalExp);
    EXPECT_EQ(14, record.dex);
    EXPECT_EQ(15, record.dexGoalExp);
    EXPECT_EQ(16, record.inte);
    EXPECT_EQ(17, record.intGoalExp);
    EXPECT_EQ(18, record.advancedSTR);
    EXPECT_EQ(19, record.advancedDEX);
    EXPECT_EQ(20, record.advancedINT);
    EXPECT_EQ(21, record.bonus);
    EXPECT_EQ(22, record.rank);
    EXPECT_EQ(23, record.rankGoalExp);
    EXPECT_EQ(24, record.currentHP);
    EXPECT_EQ(25, record.maxHP);
    EXPECT_EQ(26, record.currentMP);
    EXPECT_EQ(27, record.maxMP);
    EXPECT_EQ(28, record.fame);
    EXPECT_EQ(29, record.gold);
    EXPECT_EQ(30, record.guildID);
    EXPECT_EQ(31, record.bladeLevel);
    EXPECT_EQ(32, record.bladeGoalExp);
    EXPECT_EQ(33, record.swordLevel);
    EXPECT_EQ(34, record.swordGoalExp);
    EXPECT_EQ(35, record.gunLevel);
    EXPECT_EQ(36, record.gunGoalExp);
    EXPECT_EQ(37, record.enchantLevel);
    EXPECT_EQ(38, record.enchantGoalExp);
    EXPECT_EQ(39, record.healLevel);
    EXPECT_EQ(40, record.healGoalExp);
    EXPECT_EQ(41, record.etcLevel);
    EXPECT_EQ(42, record.etcGoalExp);
    EXPECT_EQ(43, record.zoneID);
    EXPECT_EQ(44, record.x);
    EXPECT_EQ(45, record.y);
    EXPECT_EQ(46, record.sight);
    EXPECT_EQ(47, record.gunBonusExp);
    EXPECT_EQ(48, record.rifleBonusExp);
    EXPECT_EQ(-49, record.alignment);
    EXPECT_EQ(50, record.stashGold);
    EXPECT_EQ(51, record.stashNum);
    EXPECT_EQ(52, record.resurrectZone);
    EXPECT_EQ(-53, record.reward);
    EXPECT_EQ(54, record.smsCharge);
}

TEST_F(CharacterMySQL, LoadVampireReturnsEveryColumnInSelectPosition) {
    PlayerFixture vampire = PlayerFixtures::midLevelVampire();
    vampire.persist();
    execSQL("UPDATE Vampire SET AdvancementClass=2, AdvancementGoalExp=3, Sex='FEMALE', MasterEffectColor=5, "
            "BatColor=6, SkinColor=7, STR=8, DEX=9, INTE=10, HP=11, CurrentHP=12, Fame=13, GoalExp=14, Level=15, "
            "Bonus=16, Gold=17, GuildID=18, ZoneID=19, XCoord=20, YCoord=21, Sight=22, Alignment=-23, "
            "StashGold=24, StashNum=25, Competence=26, CompetenceShape=27, ResurrectZone=28, SilverDamage=29, "
            "Reward=-30, SMSCharge=31, `Rank`=32, RankGoalExp=33 WHERE Name='" +
            vampire.name + "'");

    VampireLoadRecord record;
    ASSERT_TRUE(defaultCharacterRepository().loadVampire(vampire.name, record));
    EXPECT_EQ(vampire.name, record.name);
    EXPECT_EQ(2, record.advancementClass);
    EXPECT_EQ(3, record.advancementGoalExp);
    EXPECT_EQ("FEMALE", record.sex);
    EXPECT_EQ(5, record.masterEffectColor);
    EXPECT_EQ(6, record.batColor);
    EXPECT_EQ(7, record.skinColor);
    EXPECT_EQ(8, record.str);
    EXPECT_EQ(9, record.dex);
    EXPECT_EQ(10, record.inte);
    EXPECT_EQ(11, record.maxHP);
    EXPECT_EQ(12, record.currentHP);
    EXPECT_EQ(13, record.fame);
    EXPECT_EQ(14, record.goalExp);
    EXPECT_EQ(15, record.level);
    EXPECT_EQ(16, record.bonus);
    EXPECT_EQ(17, record.gold);
    EXPECT_EQ(18, record.guildID);
    EXPECT_EQ(19, record.zoneID);
    EXPECT_EQ(20, record.x);
    EXPECT_EQ(21, record.y);
    EXPECT_EQ(22, record.sight);
    EXPECT_EQ(-23, record.alignment);
    EXPECT_EQ(24, record.stashGold);
    EXPECT_EQ(25, record.stashNum);
    EXPECT_EQ(26, record.competence);
    EXPECT_EQ(27, record.competenceShape);
    EXPECT_EQ(28, record.resurrectZone);
    EXPECT_EQ(29, record.silverDamage);
    EXPECT_EQ(-30, record.reward);
    EXPECT_EQ(31, record.smsCharge);
    EXPECT_EQ(32, record.rank);
    EXPECT_EQ(33, record.rankGoalExp);
}

TEST_F(CharacterMySQL, LoadOustersReturnsEveryColumnInSelectPosition) {
    PlayerFixture ousters = PlayerFixtures::midLevelOusters();
    ousters.persist();
    execSQL("UPDATE Ousters SET AdvancementClass=2, AdvancementGoalExp=3, Sex='FEMALE', MasterEffectColor=5, "
            "STR=6, DEX=7, INTE=8, HP=9, CurrentHP=10, MP=11, CurrentMP=12, Fame=13, GoalExp=14, Level=15, "
            "Bonus=16, SkillBonus=17, Gold=18, GuildID=19, ZoneID=20, XCoord=21, YCoord=22, Sight=23, "
            "Alignment=-24, StashGold=25, StashNum=26, Competence=27, CompetenceShape=28, ResurrectZone=29, "
            "SilverDamage=30, SMSCharge=31, `Rank`=32, RankGoalExp=33, HairColor=34 WHERE Name='" +
            ousters.name + "'");

    OustersLoadRecord record;
    ASSERT_TRUE(defaultCharacterRepository().loadOusters(ousters.name, record));
    EXPECT_EQ(ousters.name, record.name);
    EXPECT_EQ(2, record.advancementClass);
    EXPECT_EQ(3, record.advancementGoalExp);
    EXPECT_EQ("FEMALE", record.sex);
    EXPECT_EQ(5, record.masterEffectColor);
    EXPECT_EQ(6, record.str);
    EXPECT_EQ(7, record.dex);
    EXPECT_EQ(8, record.inte);
    EXPECT_EQ(9, record.maxHP);
    EXPECT_EQ(10, record.currentHP);
    EXPECT_EQ(11, record.maxMP);
    EXPECT_EQ(12, record.currentMP);
    EXPECT_EQ(13, record.fame);
    EXPECT_EQ(14, record.goalExp);
    EXPECT_EQ(15, record.level);
    EXPECT_EQ(16, record.bonus);
    EXPECT_EQ(17, record.skillBonus);
    EXPECT_EQ(18, record.gold);
    EXPECT_EQ(19, record.guildID);
    EXPECT_EQ(20, record.zoneID);
    EXPECT_EQ(21, record.x);
    EXPECT_EQ(22, record.y);
    EXPECT_EQ(23, record.sight);
    EXPECT_EQ(-24, record.alignment);
    EXPECT_EQ(25, record.stashGold);
    EXPECT_EQ(26, record.stashNum);
    EXPECT_EQ(27, record.competence);
    EXPECT_EQ(28, record.competenceShape);
    EXPECT_EQ(29, record.resurrectZone);
    EXPECT_EQ(30, record.silverDamage);
    EXPECT_EQ(31, record.smsCharge);
    EXPECT_EQ(32, record.rank);
    EXPECT_EQ(33, record.rankGoalExp);
    EXPECT_EQ(34, record.hairColor);
}

TEST_F(CharacterMySQL, LoadsSkipInactiveRowsAndMissingRows) {
    // An INACTIVE row is what the login server leaves behind when a
    // character is deleted; a load must treat it exactly like no row.
    PlayerFixture slayer = PlayerFixtures::lowLevelSlayer();
    PlayerFixture vampire = PlayerFixtures::lowLevelVampire();
    PlayerFixture ousters = PlayerFixtures::lowLevelOusters();
    slayer.persist();
    vampire.persist();
    ousters.persist();
    execSQL("UPDATE Slayer SET Active='INACTIVE' WHERE Name='" + slayer.name + "'");
    execSQL("UPDATE Vampire SET Active='INACTIVE' WHERE Name='" + vampire.name + "'");
    execSQL("UPDATE Ousters SET Active='INACTIVE' WHERE Name='" + ousters.name + "'");

    SlayerLoadRecord slayerRecord;
    VampireLoadRecord vampireRecord;
    OustersLoadRecord oustersRecord;
    EXPECT_FALSE(defaultCharacterRepository().loadSlayer(slayer.name, slayerRecord));
    EXPECT_FALSE(defaultCharacterRepository().loadVampire(vampire.name, vampireRecord));
    EXPECT_FALSE(defaultCharacterRepository().loadOusters(ousters.name, oustersRecord));

    PlayerFixture ghost = PlayerFixtures::highLevelOusters(); // never persisted
    EXPECT_FALSE(defaultCharacterRepository().loadSlayer(ghost.name, slayerRecord));
    EXPECT_FALSE(defaultCharacterRepository().loadVampire(ghost.name, vampireRecord));
    EXPECT_FALSE(defaultCharacterRepository().loadOusters(ghost.name, oustersRecord));
}

TEST_F(CharacterMySQL, LoadSlayerFindsTheTwinRowEveryCharacterGetsAtCreation) {
    // Character creation writes a Slayer row for every race, so a
    // vampire's name loads as an ACTIVE slayer too (in production the
    // twin row carries the character's real stats — CLCreatePCHandler
    // writes STR/DEX/INTE/HP/Sight/Gold into it; only this fixture
    // leaves it at the column defaults). The race chosen at login
    // decides which loader runs — nothing in the rows does.
    PlayerFixture vampire = PlayerFixtures::midLevelVampire();
    vampire.persist();

    SlayerLoadRecord record;
    ASSERT_TRUE(defaultCharacterRepository().loadSlayer(vampire.name, record));
    EXPECT_EQ(2000, record.gold); // the Slayer table's Gold default
    EXPECT_EQ(0, record.swordLevel);
}

// quest/ActionRedistributeAttr's counter: read false for a vampire with no
// row, read the column's value otherwise, and the save scoped to the name.
TEST_F(CharacterMySQL, VampireRedistributeAttrIsReadAndSavedPerName) {
    PlayerFixture vampire = PlayerFixtures::midLevelVampire();
    PlayerFixture other = PlayerFixtures::lowLevelVampire();
    vampire.persist();
    other.persist();
    execSQL("UPDATE Vampire SET RedistributeAttr = 7 WHERE Name = '" + vampire.name + "'");
    execSQL("UPDATE Vampire SET RedistributeAttr = 2 WHERE Name = '" + other.name + "'");

    int value = -1;
    EXPECT_FALSE(defaultCharacterRepository().loadVampireRedistributeAttr("itnobody", value));
    EXPECT_EQ(-1, value); // a miss leaves the out-parameter alone
    ASSERT_TRUE(defaultCharacterRepository().loadVampireRedistributeAttr(vampire.name, value));
    EXPECT_EQ(7, value);

    defaultCharacterRepository().saveVampireRedistributeAttr(value + 1, vampire.name);
    EXPECT_EQ("8", queryScalar("SELECT RedistributeAttr FROM Vampire WHERE Name = '" + vampire.name + "'"));
    EXPECT_EQ("2", queryScalar("SELECT RedistributeAttr FROM Vampire WHERE Name = '" + other.name + "'"));
    ASSERT_TRUE(defaultCharacterRepository().loadVampireRedistributeAttr(vampire.name, value));
    EXPECT_EQ(8, value);
}

// CreatureUtil's GM lookups: the Race text comes from the SLAYER row
// whatever the race (the character index), the guild id from the race's
// own table, and the sex write lands in Slayer AND Vampire (the caller never
// reaches it for an ousters);
// a second character's rows are untouched, a name without rows is false.
TEST_F(CharacterMySQL, RaceTextGuildIDAndSexGoThroughTheRaceTables) {
    CharacterRepository& repository = defaultCharacterRepository();
    PlayerFixture vampire = PlayerFixtures::midLevelVampire();
    PlayerFixture other = PlayerFixtures::lowLevelVampire();
    vampire.persist();
    other.persist();
    execSQL("UPDATE Slayer SET Race = 'VAMPIRE', GuildID = 7, Sex = 'MALE' WHERE Name = '" + vampire.name + "'");
    execSQL("UPDATE Vampire SET GuildID = 5, Sex = 'MALE' WHERE Name = '" + vampire.name + "'");
    execSQL("UPDATE Slayer SET Sex = 'MALE' WHERE Name = '" + other.name + "'");

    std::string race = "untouched";
    EXPECT_FALSE(repository.loadSlayerRaceText("itnobody", race));
    EXPECT_EQ("untouched", race);
    ASSERT_TRUE(repository.loadSlayerRaceText(vampire.name, race));
    EXPECT_EQ("VAMPIRE", race);

    int guildID = -1;
    ASSERT_TRUE(repository.loadGuildID(vampire.name, CHARACTER_RACE_VAMPIRE, guildID));
    EXPECT_EQ(5, guildID);
    ASSERT_TRUE(repository.loadGuildID(vampire.name, CHARACTER_RACE_SLAYER, guildID));
    EXPECT_EQ(7, guildID);
    guildID = -1;
    EXPECT_FALSE(repository.loadGuildID(vampire.name, CHARACTER_RACE_OUSTERS, guildID)); // no Ousters row
    EXPECT_EQ(-1, guildID);

    repository.saveSex(vampire.name, "FEMALE");
    EXPECT_EQ("FEMALE", queryScalar("SELECT Sex FROM Slayer WHERE Name = '" + vampire.name + "'"));
    EXPECT_EQ("FEMALE", queryScalar("SELECT Sex FROM Vampire WHERE Name = '" + vampire.name + "'"));
    EXPECT_EQ("MALE", queryScalar("SELECT Sex FROM Slayer WHERE Name = '" + other.name + "'"));
}

// CGSayHandler's guild-master checks: the six Slayer columns, one race's
// Level from its own table, each false for a name without a row there;
// and the opdeny spelling of the account lookup reaching the same row as
// the whisper spelling.
TEST_F(CharacterMySQL, GuildMasterStatsLevelsAndTheOpdenySpellingReadTheirTables) {
    CharacterRepository& repository = defaultCharacterRepository();
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    PlayerFixture vampire = PlayerFixtures::midLevelVampire();
    PlayerFixture ousters = PlayerFixtures::midLevelOusters();
    slayer.persist();
    vampire.persist();
    ousters.persist();
    execSQL("UPDATE Slayer SET Fame = 777, BladeLevel = 1, SwordLevel = 2, GunLevel = 3, HealLevel = 4, "
            "EnchantLevel = 5, PlayerID = 'it-acct' WHERE Name = '" +
            slayer.name + "'");
    execSQL("UPDATE Vampire SET Level = 66 WHERE Name = '" + vampire.name + "'");
    execSQL("UPDATE Ousters SET Level = 77 WHERE Name = '" + ousters.name + "'");

    SlayerMasterStatsRow stats;
    EXPECT_FALSE(repository.loadSlayerMasterStats("itnobody", stats));
    ASSERT_TRUE(repository.loadSlayerMasterStats(slayer.name, stats));
    EXPECT_EQ(777, stats.fame);
    EXPECT_EQ(1, stats.bladeLevel);
    EXPECT_EQ(2, stats.swordLevel);
    EXPECT_EQ(3, stats.gunLevel);
    EXPECT_EQ(4, stats.healLevel);
    EXPECT_EQ(5, stats.enchantLevel);

    int level = -1;
    // (a persisted slayer has a Vampire row too — creation writes both, as the fixture does)
    EXPECT_FALSE(repository.loadVampireLevel("itnobody", level));
    ASSERT_TRUE(repository.loadVampireLevel(vampire.name, level));
    EXPECT_EQ(66, level);
    EXPECT_FALSE(repository.loadOustersLevel(vampire.name, level)); // no Ousters row for a vampire
    ASSERT_TRUE(repository.loadOustersLevel(ousters.name, level));
    EXPECT_EQ(77, level);

    std::string playerID;
    ASSERT_TRUE(repository.loadSlayerPlayerID(PLAYERID_SPELLING_OPDENY, slayer.name, playerID));
    EXPECT_EQ("it-acct", playerID);
    playerID = "";
    ASSERT_TRUE(repository.loadSlayerPlayerID(PLAYERID_SPELLING_WHISPER, slayer.name, playerID));
    EXPECT_EQ("it-acct", playerID);
    EXPECT_FALSE(repository.loadSlayerPlayerID(PLAYERID_SPELLING_OPDENY, "itnobody", playerID));
}

// --- the character purge against real MySQL ----------------------------------
// One method, 109 statements. The test seeds rows for two names in a
// sample of the tables from every stretch of the list (the race tables, a
// skill save, the rank bonus, two object tables, GQuestSave, CoupleInfo in
// both partner columns, two effect tables, FlagSet, TimeLimitItems,
// EventQuestAdvance), purges one name and checks the other is whole.

class CharacterPurgeMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        clean();
    }
    virtual void TearDown() {
        clean();
    }
    static void clean() {
        const char* byName[] = {"Slayer", "Vampire", "Ousters"};
        for (size_t i = 0; i < 3; i++)
            execSQL(std::string("DELETE FROM ") + byName[i] + " WHERE Name LIKE 'it-p%'");
        const char* byOwner[] = {"SkillSave",       "RankBonusData", "ARObject", "BeltObject",     "GQuestSave",
                                 "EffectAcidTouch", "EnemyErase",    "FlagSet",  "TimeLimitItems", "EventQuestAdvance"};
        for (size_t i = 0; i < sizeof(byOwner) / sizeof(byOwner[0]); i++)
            execSQL(std::string("DELETE FROM ") + byOwner[i] + " WHERE OwnerID LIKE 'it-p%'");
        execSQL("DELETE FROM CoupleInfo WHERE MalePartnerName LIKE 'it-p%' OR FemalePartnerName LIKE 'it-p%'");
    }
    static void seed(const std::string& name, int id) {
        const std::string n = "'" + name + "'";
        const std::string i = std::to_string(id);
        execSQL("INSERT INTO Slayer (Name, Active) VALUES (" + n + ", 'ACTIVE')");
        execSQL("INSERT INTO Vampire (Name, Active) VALUES (" + n + ", 'ACTIVE')");
        execSQL("INSERT INTO Ousters (Name, Active) VALUES (" + n + ", 'ACTIVE')");
        execSQL("INSERT INTO SkillSave (OwnerID, SkillType) VALUES (" + n + ", 1)");
        execSQL("INSERT INTO RankBonusData (OwnerID, Type) VALUES (" + n + ", 1)");
        execSQL("INSERT INTO ARObject (ItemID, OwnerID) VALUES (" + i + ", " + n + ")");
        execSQL("INSERT INTO BeltObject (ItemID, OwnerID) VALUES (" + i + ", " + n + ")");
        execSQL("INSERT INTO GQuestSave (QuestID, OwnerID) VALUES (" + i + ", " + n + ")");
        execSQL("INSERT INTO CoupleInfo (MalePartnerName, FemalePartnerName) VALUES (" + n + ", 'x')");
        execSQL("INSERT INTO CoupleInfo (MalePartnerName, FemalePartnerName) VALUES ('y', " + n + ")");
        execSQL("INSERT INTO EffectAcidTouch (OwnerID) VALUES (" + n + ")");
        execSQL("INSERT INTO EnemyErase (OwnerID) VALUES (" + n + ")");
        execSQL("INSERT INTO FlagSet (OwnerID) VALUES (" + n + ")");
        execSQL("INSERT INTO TimeLimitItems (OwnerID, ItemID) VALUES (" + n + ", " + i + ")");
        execSQL("INSERT INTO EventQuestAdvance (OwnerID, QuestLevel) VALUES (" + n + ", 1)");
    }
    static std::string rowsOf(const std::string& name) {
        const std::string n = "'" + name + "'";
        return queryScalar("SELECT (SELECT COUNT(*) FROM SkillSave WHERE OwnerID = " + n +
                           ") + (SELECT COUNT(*) FROM RankBonusData WHERE OwnerID = " + n +
                           ") + (SELECT COUNT(*) FROM ARObject WHERE OwnerID = " + n +
                           ") + (SELECT COUNT(*) FROM BeltObject WHERE OwnerID = " + n +
                           ") + (SELECT COUNT(*) FROM GQuestSave WHERE OwnerID = " + n +
                           ") + (SELECT COUNT(*) FROM CoupleInfo WHERE MalePartnerName = " + n +
                           " OR FemalePartnerName = " + n +
                           ") + (SELECT COUNT(*) FROM EffectAcidTouch WHERE OwnerID = " + n +
                           ") + (SELECT COUNT(*) FROM EnemyErase WHERE OwnerID = " + n +
                           ") + (SELECT COUNT(*) FROM FlagSet WHERE OwnerID = " + n +
                           ") + (SELECT COUNT(*) FROM TimeLimitItems WHERE OwnerID = " + n +
                           ") + (SELECT COUNT(*) FROM EventQuestAdvance WHERE OwnerID = " + n + ")");
    }
};

TEST_F(CharacterPurgeMySQL, PurgeRetiresTheRaceRowsAndDeletesEveryOtherRowOfThatNameOnly) {
    seed("it-purge", 31000);
    seed("it-pkeep", 31001);
    ASSERT_EQ("12", rowsOf("it-purge"));
    ASSERT_EQ("12", rowsOf("it-pkeep"));

    defaultCharacterPurgeRepository().purgeCharacter("it-purge");

    // The race rows stay, flipped to INACTIVE — all three, whatever the race.
    EXPECT_EQ("INACTIVE", queryScalar("SELECT Active FROM Slayer WHERE Name = 'it-purge'"));
    EXPECT_EQ("INACTIVE", queryScalar("SELECT Active FROM Vampire WHERE Name = 'it-purge'"));
    EXPECT_EQ("INACTIVE", queryScalar("SELECT Active FROM Ousters WHERE Name = 'it-purge'"));
    EXPECT_EQ("0", rowsOf("it-purge"));

    // The other name is whole.
    EXPECT_EQ("ACTIVE", queryScalar("SELECT Active FROM Slayer WHERE Name = 'it-pkeep'"));
    EXPECT_EQ("ACTIVE", queryScalar("SELECT Active FROM Vampire WHERE Name = 'it-pkeep'"));
    EXPECT_EQ("ACTIVE", queryScalar("SELECT Active FROM Ousters WHERE Name = 'it-pkeep'"));
    EXPECT_EQ("12", rowsOf("it-pkeep"));

    // A name with no rows anywhere purges without complaint (every statement
    // simply matches nothing).
    EXPECT_NO_THROW(defaultCharacterPurgeRepository().purgeCharacter("it-pnone"));
}

// --- SkillSave / VampireSkillSave / OustersSkillSave against real MySQL ---
// Like CharacterRepository, a repository with NO fake tier: these tests are the
// net. Every inserted column is read back through the load, every
// update asserts both the columns it writes and the ones it must leave
// alone, and the row order the ORDER-BY-less loads produce is pinned
// against the real server rather than assumed.

class SkillSaveMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        PlayerFixtures::removeAll();
        execSQL(std::string("DELETE FROM SkillSave WHERE OwnerID IN ") + PlayerFixtures::nameList());
        execSQL(std::string("DELETE FROM VampireSkillSave WHERE OwnerID IN ") + PlayerFixtures::nameList());
        execSQL(std::string("DELETE FROM OustersSkillSave WHERE OwnerID IN ") + PlayerFixtures::nameList());
    }

    static SlayerSkillRecord slayerSkill(SkillType_t type, ExpLevel_t level, Exp_t exp, Turn_t delay,
                                         Turn_t castingTime, time_t nextTime) {
        SlayerSkillRecord record;
        record.skillType = type;
        record.skillLevel = level;
        record.skillExp = exp;
        record.delay = delay;
        record.castingTime = castingTime;
        record.nextTime = nextTime;
        return record;
    }

    static VampireSkillRecord vampireSkill(SkillType_t type, Turn_t delay, Turn_t castingTime, time_t nextTime) {
        VampireSkillRecord record;
        record.skillType = type;
        record.delay = delay;
        record.castingTime = castingTime;
        record.nextTime = nextTime;
        return record;
    }

    static OustersSkillRecord oustersSkill(SkillType_t type, ExpLevel_t level, Turn_t delay, Turn_t castingTime,
                                           time_t nextTime) {
        OustersSkillRecord record;
        record.skillType = type;
        record.skillLevel = level;
        record.delay = delay;
        record.castingTime = castingTime;
        record.nextTime = nextTime;
        return record;
    }
};

TEST_F(SkillSaveMySQL, SlayerInsertThenLoadRoundTripsEveryColumn) {
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    slayer.persist();

    defaultSkillSaveRepository().insertSlayerSkill(slayer.name, slayerSkill(101, 2, 3003, 44, 55, 1700000006));

    std::vector<SlayerSkillRow> rows = defaultSkillSaveRepository().loadSlayerSkills(slayer.name);
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(101, rows[0].skillType);
    EXPECT_EQ(2, rows[0].skillLevel);
    EXPECT_EQ(3003, rows[0].skillExp);
    EXPECT_EQ(44, rows[0].delay);
    EXPECT_EQ(55, rows[0].castingTime);
    EXPECT_EQ(1700000006, rows[0].nextTime);
}

TEST_F(SkillSaveMySQL, LoadReturnsEveryRowIncludingDuplicateTypes) {
    // The contract: every row comes back, duplicates of a type included
    // (the keyless table cannot refuse them). Asserted as a multiset —
    // the order is deliberately NOT part of this test, see the next one.
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    slayer.persist();

    defaultSkillSaveRepository().insertSlayerSkill(slayer.name, slayerSkill(7, 1, 0, 11, 0, 0));
    defaultSkillSaveRepository().insertSlayerSkill(slayer.name, slayerSkill(3, 1, 0, 22, 0, 0));
    defaultSkillSaveRepository().insertSlayerSkill(slayer.name, slayerSkill(7, 1, 0, 33, 0, 0));

    std::vector<SlayerSkillRow> rows = defaultSkillSaveRepository().loadSlayerSkills(slayer.name);
    ASSERT_EQ(3u, rows.size());
    int seenType7Delay11 = 0, seenType3Delay22 = 0, seenType7Delay33 = 0;
    for (size_t r = 0; r < rows.size(); r++) {
        if (rows[r].skillType == 7 && rows[r].delay == 11)
            seenType7Delay11++;
        else if (rows[r].skillType == 3 && rows[r].delay == 22)
            seenType3Delay22++;
        else if (rows[r].skillType == 7 && rows[r].delay == 33)
            seenType7Delay33++;
        else
            ADD_FAILURE() << "unexpected row " << rows[r].skillType << "/" << rows[r].delay;
    }
    EXPECT_EQ(1, seenType7Delay11);
    EXPECT_EQ(1, seenType3Delay22);
    EXPECT_EQ(1, seenType7Delay33);
}

TEST_F(SkillSaveMySQL, LoadOrderObservedOnThe57TierIsInsertionOrderNotSkillTypeOrder) {
    // An OBSERVATION, not a contract: no ORDER BY and no primary key, so
    // the row order is whatever access path the optimizer picks. The
    // (OwnerID, SkillType) secondary index suggests SkillType-ascending
    // order, but on this tier's near-empty table — where the WHERE
    // matches essentially every row and the index does not cover the
    // SELECT — the rows come back in insertion order, a scan in
    // hidden-row-id order. A populated table, or MySQL 8 (supported, but
    // not what this tier runs), may reorder. If this test fails on such
    // a configuration, update the observation — do NOT "fix" a loader.
    // Why it matters at all: the vampire/ousters loaders keep whichever
    // duplicate of a type arrives FIRST, so that choice is plan-dependent.
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    slayer.persist();

    defaultSkillSaveRepository().insertSlayerSkill(slayer.name, slayerSkill(7, 1, 0, 11, 0, 0));
    defaultSkillSaveRepository().insertSlayerSkill(slayer.name, slayerSkill(3, 1, 0, 22, 0, 0));
    defaultSkillSaveRepository().insertSlayerSkill(slayer.name, slayerSkill(7, 1, 0, 33, 0, 0));

    std::vector<SlayerSkillRow> rows = defaultSkillSaveRepository().loadSlayerSkills(slayer.name);
    ASSERT_EQ(3u, rows.size());
    EXPECT_EQ(7, rows[0].skillType);
    EXPECT_EQ(11, rows[0].delay);
    EXPECT_EQ(3, rows[1].skillType);
    EXPECT_EQ(22, rows[1].delay);
    EXPECT_EQ(7, rows[2].skillType);
    EXPECT_EQ(33, rows[2].delay);
}

TEST_F(SkillSaveMySQL, SlayerUpdateWritesLevelExpAndDelayOnly) {
    PlayerFixture slayer = PlayerFixtures::highLevelSlayer();
    slayer.persist();
    defaultSkillSaveRepository().insertSlayerSkill(slayer.name, slayerSkill(101, 2, 3003, 44, 55, 1700000006));
    defaultSkillSaveRepository().insertSlayerSkill(slayer.name, slayerSkill(102, 1, 10, 20, 30, 40)); // untouched

    defaultSkillSaveRepository().updateSlayerSkill(slayer.name, 101, 9, 9999, 88);

    std::vector<SlayerSkillRow> rows = defaultSkillSaveRepository().loadSlayerSkills(slayer.name);
    ASSERT_EQ(2u, rows.size());
    // select each row by type — the load order is not a contract
    const SlayerSkillRow& updated = rows[0].skillType == 101 ? rows[0] : rows[1];
    const SlayerSkillRow& untouched = rows[0].skillType == 101 ? rows[1] : rows[0];
    ASSERT_EQ(101, updated.skillType);
    ASSERT_EQ(102, untouched.skillType);
    EXPECT_EQ(9, updated.skillLevel);
    EXPECT_EQ(9999, updated.skillExp);
    EXPECT_EQ(88, updated.delay);
    EXPECT_EQ(55, updated.castingTime);      // not part of the update
    EXPECT_EQ(1700000006, updated.nextTime); // not part of the update
    EXPECT_EQ(1, untouched.skillLevel);
    EXPECT_EQ(10, untouched.skillExp);
    EXPECT_EQ(20, untouched.delay);
}

TEST_F(SkillSaveMySQL, VampireInsertLoadAndUpdateDelayOnly) {
    PlayerFixture vampire = PlayerFixtures::midLevelVampire();
    vampire.persist();

    defaultSkillSaveRepository().insertVampireSkill(vampire.name, vampireSkill(201, 10, 20, 1700000030));

    std::vector<VampireSkillRow> rows = defaultSkillSaveRepository().loadVampireSkills(vampire.name);
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(201, rows[0].skillType);
    EXPECT_EQ(10, rows[0].delay);
    EXPECT_EQ(20, rows[0].castingTime);
    EXPECT_EQ(1700000030, rows[0].nextTime);

    defaultSkillSaveRepository().updateVampireSkill(vampire.name, 201, 77);

    rows = defaultSkillSaveRepository().loadVampireSkills(vampire.name);
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(77, rows[0].delay);
    EXPECT_EQ(20, rows[0].castingTime);
    EXPECT_EQ(1700000030, rows[0].nextTime);
}

TEST_F(SkillSaveMySQL, OustersInsertLandsSkillLevelDespiteItsTrailingPositionInTheInsert) {
    // The ousters INSERT names SkillLevel after NextTime — the column and
    // value lists agree, so the level must land in SkillLevel and not in
    // the neighbouring NextTime.
    PlayerFixture ousters = PlayerFixtures::midLevelOusters();
    ousters.persist();

    defaultSkillSaveRepository().insertOustersSkill(ousters.name, oustersSkill(301, 4, 10, 20, 1700000040));

    std::vector<OustersSkillRow> rows = defaultSkillSaveRepository().loadOustersSkills(ousters.name);
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(301, rows[0].skillType);
    EXPECT_EQ(4, rows[0].skillLevel);
    EXPECT_EQ(10, rows[0].delay);
    EXPECT_EQ(20, rows[0].castingTime);
    EXPECT_EQ(1700000040, rows[0].nextTime);
}

TEST_F(SkillSaveMySQL, OustersUpdateWritesLevelAndDelayOnly) {
    PlayerFixture ousters = PlayerFixtures::highLevelOusters();
    ousters.persist();
    defaultSkillSaveRepository().insertOustersSkill(ousters.name, oustersSkill(301, 4, 10, 20, 1700000040));

    defaultSkillSaveRepository().updateOustersSkill(ousters.name, 301, 6, 66);

    std::vector<OustersSkillRow> rows = defaultSkillSaveRepository().loadOustersSkills(ousters.name);
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(6, rows[0].skillLevel);
    EXPECT_EQ(66, rows[0].delay);
    EXPECT_EQ(20, rows[0].castingTime);
    EXPECT_EQ(1700000040, rows[0].nextTime);
}

TEST_F(SkillSaveMySQL, OustersDeleteRemovesOnlyThatTypeForThatOwner) {
    PlayerFixture ousters = PlayerFixtures::lowLevelOusters();
    PlayerFixture other = PlayerFixtures::midLevelOusters();
    ousters.persist();
    other.persist();
    defaultSkillSaveRepository().insertOustersSkill(ousters.name, oustersSkill(301, 1, 0, 0, 0));
    defaultSkillSaveRepository().insertOustersSkill(ousters.name, oustersSkill(301, 2, 0, 0, 0)); // duplicate
    defaultSkillSaveRepository().insertOustersSkill(ousters.name, oustersSkill(302, 1, 0, 0, 0));
    defaultSkillSaveRepository().insertOustersSkill(other.name, oustersSkill(301, 1, 0, 0, 0));

    defaultSkillSaveRepository().deleteOustersSkill(ousters.name, 301);

    std::vector<OustersSkillRow> rows = defaultSkillSaveRepository().loadOustersSkills(ousters.name);
    ASSERT_EQ(1u, rows.size()); // both 301 rows went, the keyless table has no way to pick one
    EXPECT_EQ(302, rows[0].skillType);
    EXPECT_EQ(1u, defaultSkillSaveRepository().loadOustersSkills(other.name).size());
}

TEST_F(SkillSaveMySQL, WritesAgainstMissingRowsAreSilentNoOps) {
    PlayerFixture ghost = PlayerFixtures::highLevelVampire(); // never persisted

    defaultSkillSaveRepository().updateSlayerSkill(ghost.name, 101, 1, 1, 1);
    defaultSkillSaveRepository().updateVampireSkill(ghost.name, 201, 1);
    defaultSkillSaveRepository().updateOustersSkill(ghost.name, 301, 1, 1);
    defaultSkillSaveRepository().deleteOustersSkill(ghost.name, 301);

    EXPECT_TRUE(defaultSkillSaveRepository().loadSlayerSkills(ghost.name).empty());
    EXPECT_TRUE(defaultSkillSaveRepository().loadVampireSkills(ghost.name).empty());
    EXPECT_TRUE(defaultSkillSaveRepository().loadOustersSkills(ghost.name).empty());
}

// --- the persisted-effect tables against real MySQL -----------------------
// EffectSaveRepository has no fake tier; the format strings are per-table
// data, so every table gets exercised, and the two structural quirks —
// the one keyed table among twelve keyless ones, and EnemyErase's
// owner-wide UPDATE — are pinned against the real server.

const char* const DEADLINE_TABLE_NAMES[DEADLINE_EFFECT_TABLE_MAX] = {"EffectAftermath", "EffectKillAftermath",
                                                                     "EffectMute", "CanEnterGDRLair", "EffectRestore"};
const char* const CREATURE_EFFECT_TABLE_NAMES[CREATURE_EFFECT_TABLE_MAX] = {
    "EffectBloodDrain", "EffectFlare", "EffectLight", "EffectYellowPoisonToCreature"};
const char* const REMAIN_TABLE_NAMES[REMAIN_EFFECT_TABLE_MAX] = {"EffectSafeForceScroll", "EffectBehemothForceScroll",
                                                                 "EffectCarnelianForceScroll"};

class EffectSaveMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        PlayerFixtures::removeAll();
        for (int t = 0; t < DEADLINE_EFFECT_TABLE_MAX; t++)
            execSQL(std::string("DELETE FROM ") + DEADLINE_TABLE_NAMES[t] + " WHERE OwnerID IN " +
                    PlayerFixtures::nameList());
        for (int t = 0; t < REMAIN_EFFECT_TABLE_MAX; t++)
            execSQL(std::string("DELETE FROM ") + REMAIN_TABLE_NAMES[t] + " WHERE OwnerID IN " +
                    PlayerFixtures::nameList());
        execSQL(std::string("DELETE FROM EnemyErase WHERE OwnerID IN ") + PlayerFixtures::nameList());
    }

    static std::string yearTimeOf(const char* table, const std::string& owner) {
        return queryScalar(std::string("SELECT YearTime FROM ") + table + " WHERE OwnerID='" + owner + "'");
    }
};

TEST_F(EffectSaveMySQL, EveryDeadlineTableRoundTripsInsertUpdateLoadDelete) {
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    slayer.persist();
    EffectSaveRepository& repository = defaultEffectSaveRepository();

    for (int t = 0; t < DEADLINE_EFFECT_TABLE_MAX; t++) {
        DeadlineEffectTable table = (DeadlineEffectTable)t;
        SCOPED_TRACE(DEADLINE_TABLE_NAMES[t]);

        repository.insertDeadline(table, slayer.name, 1111, 1700000001);
        std::vector<DWORD> dayTimes = repository.loadDeadlines(table, slayer.name);
        ASSERT_EQ(1u, dayTimes.size());
        EXPECT_EQ(1700000001u, dayTimes[0]);
        EXPECT_EQ("1111", yearTimeOf(DEADLINE_TABLE_NAMES[t], slayer.name));

        repository.updateDeadline(table, slayer.name, 2222, 1700000002);
        dayTimes = repository.loadDeadlines(table, slayer.name);
        ASSERT_EQ(1u, dayTimes.size());
        EXPECT_EQ(1700000002u, dayTimes[0]);
        EXPECT_EQ("2222", yearTimeOf(DEADLINE_TABLE_NAMES[t], slayer.name));

        repository.deleteDeadline(table, slayer.name);
        EXPECT_TRUE(repository.loadDeadlines(table, slayer.name).empty());
    }
}

TEST_F(EffectSaveMySQL, KeylessTablesAccumulateDuplicatesButKillAftermathRefusesThem) {
    // Twelve of the thirteen tables have only an OwnerID index; a second
    // create() for the same owner just adds a row (and the loader then
    // attaches the effect twice). EffectKillAftermath is the one with
    // OwnerID as PRIMARY KEY: its second insert raises ER_DUP_ENTRY.
    PlayerFixture vampire = PlayerFixtures::midLevelVampire();
    vampire.persist();
    EffectSaveRepository& repository = defaultEffectSaveRepository();

    repository.insertDeadline(EFFECT_TABLE_AFTERMATH, vampire.name, 1, 1700000001);
    repository.insertDeadline(EFFECT_TABLE_AFTERMATH, vampire.name, 1, 1700000002);
    EXPECT_EQ(2u, repository.loadDeadlines(EFFECT_TABLE_AFTERMATH, vampire.name).size());

    repository.insertDeadline(EFFECT_TABLE_KILL_AFTERMATH, vampire.name, 1, 1700000001);
    EXPECT_ANY_THROW(repository.insertDeadline(EFFECT_TABLE_KILL_AFTERMATH, vampire.name, 1, 1700000002));
    std::vector<DWORD> dayTimes = repository.loadDeadlines(EFFECT_TABLE_KILL_AFTERMATH, vampire.name);
    ASSERT_EQ(1u, dayTimes.size());
    EXPECT_EQ(1700000001u, dayTimes[0]);
}

TEST_F(EffectSaveMySQL, EveryRemainTableRoundTripsInsertUpdateLoadDelete) {
    PlayerFixture ousters = PlayerFixtures::midLevelOusters();
    ousters.persist();
    EffectSaveRepository& repository = defaultEffectSaveRepository();

    for (int t = 0; t < REMAIN_EFFECT_TABLE_MAX; t++) {
        RemainEffectTable table = (RemainEffectTable)t;
        SCOPED_TRACE(REMAIN_TABLE_NAMES[t]);
        DWORD remainTurn = 0;

        EXPECT_FALSE(repository.loadRemain(table, ousters.name, remainTurn));

        repository.insertRemain(table, ousters.name, 500);
        ASSERT_TRUE(repository.loadRemain(table, ousters.name, remainTurn));
        EXPECT_EQ(500u, remainTurn);

        repository.updateRemain(table, ousters.name, 600);
        ASSERT_TRUE(repository.loadRemain(table, ousters.name, remainTurn));
        EXPECT_EQ(600u, remainTurn);

        repository.deleteRemain(table, ousters.name);
        EXPECT_FALSE(repository.loadRemain(table, ousters.name, remainTurn));
    }
}

TEST_F(EffectSaveMySQL, EnemyEraseDeletesByEnemyButUpdatesEveryRowOfTheOwner) {
    // One row per enemy. The DELETE keys on (OwnerID, EnemyName); the
    // UPDATE keys on OwnerID alone, so saving one enemy-erase effect
    // rewrites every EnemyErase row the owner has to that enemy.
    PlayerFixture slayer = PlayerFixtures::highLevelSlayer();
    slayer.persist();
    EffectSaveRepository& repository = defaultEffectSaveRepository();

    repository.insertEnemyErase(slayer.name, 1, 1700000001, "foeone");
    repository.insertEnemyErase(slayer.name, 2, 1700000002, "foetwo");

    std::vector<EnemyEraseRow> rows = repository.loadEnemyErases(slayer.name);
    ASSERT_EQ(2u, rows.size());
    for (size_t r = 0; r < rows.size(); r++) {
        if (rows[r].enemyName == "foeone")
            EXPECT_EQ(1700000001u, rows[r].dayTime);
        else if (rows[r].enemyName == "foetwo")
            EXPECT_EQ(1700000002u, rows[r].dayTime);
        else
            ADD_FAILURE() << "unexpected enemy " << rows[r].enemyName;
    }

    repository.updateEnemyErase(slayer.name, 3, 1700000003, "foeone");
    rows = repository.loadEnemyErases(slayer.name);
    ASSERT_EQ(2u, rows.size());
    EXPECT_EQ("foeone", rows[0].enemyName);
    EXPECT_EQ("foeone", rows[1].enemyName);
    EXPECT_EQ(1700000003u, rows[0].dayTime);
    EXPECT_EQ(1700000003u, rows[1].dayTime);

    repository.deleteEnemyErase(slayer.name, "foeone");
    EXPECT_TRUE(repository.loadEnemyErases(slayer.name).empty());
}

// --- the per-creature effect tables against real MySQL ---------------------
// The deadline core plus each table's own columns. What is pinned here: the
// per-table column lists (a field the SELECT does not name comes back 0
// while the INSERT still writes the column), the keyless duplicate, and the
// OldSight column that three of the four write and none of them read.

class CreatureEffectMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        PlayerFixtures::removeAll();
        for (int t = 0; t < CREATURE_EFFECT_TABLE_MAX; t++)
            execSQL(std::string("DELETE FROM ") + CREATURE_EFFECT_TABLE_NAMES[t] + " WHERE OwnerID IN " +
                    PlayerFixtures::nameList());
    }

    static std::string columnOf(const char* table, const char* column, const std::string& owner) {
        return queryScalar(std::string("SELECT ") + column + " FROM " + table + " WHERE OwnerID='" + owner + "'");
    }
};

TEST_F(CreatureEffectMySQL, EveryTableRoundTripsInsertUpdateLoadDelete) {
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    slayer.persist();
    EffectSaveRepository& repository = defaultEffectSaveRepository();

    for (int t = 0; t < CREATURE_EFFECT_TABLE_MAX; t++) {
        CreatureEffectTable table = (CreatureEffectTable)t;
        const char* name = CREATURE_EFFECT_TABLE_NAMES[t];
        SCOPED_TRACE(name);
        // EffectBloodDrain's SELECT names (DayTime, Level); the other three
        // name YearTime, and only EffectYellowPoisonToCreature also names Level.
        const bool selectsYearTime = table != CREATURE_EFFECT_BLOOD_DRAIN;
        const bool selectsLevel =
            table == CREATURE_EFFECT_BLOOD_DRAIN || table == CREATURE_EFFECT_YELLOW_POISON_TO_CREATURE;

        repository.insertCreatureEffect(table, slayer.name, 1111, 1700000001, 7, 9);
        std::vector<CreatureEffectRow> rows = repository.loadCreatureEffects(table, slayer.name);
        ASSERT_EQ(1u, rows.size());
        EXPECT_EQ(1700000001u, rows[0].dayTime);
        EXPECT_EQ(selectsYearTime ? 1111u : 0u, rows[0].yearTime);
        EXPECT_EQ(selectsLevel ? 7 : 0, rows[0].level);
        // Every INSERT writes YearTime, including the one whose SELECT
        // leaves it behind.
        EXPECT_EQ("1111", columnOf(name, "YearTime", slayer.name));

        repository.updateCreatureEffect(table, slayer.name, 2222, 1700000002, 8, 10);
        rows = repository.loadCreatureEffects(table, slayer.name);
        ASSERT_EQ(1u, rows.size());
        EXPECT_EQ(1700000002u, rows[0].dayTime);
        EXPECT_EQ(selectsYearTime ? 2222u : 0u, rows[0].yearTime);
        EXPECT_EQ(selectsLevel ? 8 : 0, rows[0].level);
        EXPECT_EQ("2222", columnOf(name, "YearTime", slayer.name));

        repository.deleteCreatureEffect(table, slayer.name);
        EXPECT_TRUE(repository.loadCreatureEffects(table, slayer.name).empty());
    }
}

TEST_F(CreatureEffectMySQL, OldSightIsWrittenByThreeTablesAndReadBackByNone) {
    PlayerFixture vampire = PlayerFixtures::midLevelVampire();
    vampire.persist();
    EffectSaveRepository& repository = defaultEffectSaveRepository();

    repository.insertCreatureEffect(CREATURE_EFFECT_FLARE, vampire.name, 1, 1700000001, 0, 21);
    repository.insertCreatureEffect(CREATURE_EFFECT_LIGHT, vampire.name, 1, 1700000001, 0, 22);
    repository.insertCreatureEffect(CREATURE_EFFECT_YELLOW_POISON_TO_CREATURE, vampire.name, 1, 1700000001, 5, 23);

    // The column is written, and the SELECTs still name it.
    EXPECT_EQ("21", columnOf("EffectFlare", "OldSight", vampire.name));
    EXPECT_EQ("22", columnOf("EffectLight", "OldSight", vampire.name));
    EXPECT_EQ("23", columnOf("EffectYellowPoisonToCreature", "OldSight", vampire.name));

    // EffectFlare's table has a Level column its INSERT does not name, so the
    // row keeps the column default while the load reports 0 as well.
    EXPECT_EQ("0", columnOf("EffectFlare", "Level", vampire.name));
    std::vector<CreatureEffectRow> flare = repository.loadCreatureEffects(CREATURE_EFFECT_FLARE, vampire.name);
    ASSERT_EQ(1u, flare.size());
    EXPECT_EQ(0, flare[0].level);
}

TEST_F(CreatureEffectMySQL, EveryTableIsKeylessSoASecondInsertLeavesTwoRows) {
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    slayer.persist();
    EffectSaveRepository& repository = defaultEffectSaveRepository();

    for (int t = 0; t < CREATURE_EFFECT_TABLE_MAX; t++) {
        CreatureEffectTable table = (CreatureEffectTable)t;
        SCOPED_TRACE(CREATURE_EFFECT_TABLE_NAMES[t]);

        repository.insertCreatureEffect(table, slayer.name, 1, 1700000001, 1, 1);
        repository.insertCreatureEffect(table, slayer.name, 1, 1700000002, 2, 2);
        EXPECT_EQ(2u, repository.loadCreatureEffects(table, slayer.name).size());

        // The DELETE is keyed on OwnerID alone, so it takes both.
        repository.deleteCreatureEffect(table, slayer.name);
        EXPECT_TRUE(repository.loadCreatureEffects(table, slayer.name).empty());
    }
}

TEST_F(CreatureEffectMySQL, LevelComesBackThroughEachTablesOwnGetter) {
    PlayerFixture ousters = PlayerFixtures::midLevelOusters();
    ousters.persist();
    EffectSaveRepository& repository = defaultEffectSaveRepository();

    // EffectYellowPoisonToCreature.Level is int(10) unsigned and its loader
    // read it with getInt, so a value past a byte survives the round trip.
    repository.insertCreatureEffect(CREATURE_EFFECT_YELLOW_POISON_TO_CREATURE, ousters.name, 1, 1700000001, 300, 0);
    std::vector<CreatureEffectRow> rows =
        repository.loadCreatureEffects(CREATURE_EFFECT_YELLOW_POISON_TO_CREATURE, ousters.name);
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(300, rows[0].level);

    // EffectBloodDrain.Level is tinyint(3) unsigned, so the column caps the
    // value at 255 before its getBYTE ever sees it: the two getters cannot
    // actually disagree, and the repository keeps each table's own anyway.
    repository.insertCreatureEffect(CREATURE_EFFECT_BLOOD_DRAIN, ousters.name, 1, 1700000001, 300, 0);
    EXPECT_EQ("255", columnOf("EffectBloodDrain", "Level", ousters.name));
    rows = repository.loadCreatureEffects(CREATURE_EFFECT_BLOOD_DRAIN, ousters.name);
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(255, rows[0].level);
}

TEST_F(CreatureEffectMySQL, WritesAgainstMissingRowsAreSilentNoOps) {
    PlayerFixture ghost = PlayerFixtures::midLevelSlayer();

    EffectSaveRepository& repository = defaultEffectSaveRepository();
    repository.updateCreatureEffect(CREATURE_EFFECT_LIGHT, ghost.name, 1, 1, 1, 1);
    repository.deleteCreatureEffect(CREATURE_EFFECT_LIGHT, ghost.name);
    EXPECT_TRUE(repository.loadCreatureEffects(CREATURE_EFFECT_LIGHT, ghost.name).empty());
}

TEST_F(EffectSaveMySQL, WritesAgainstMissingRowsAreSilentNoOps) {
    PlayerFixture ghost = PlayerFixtures::lowLevelVampire(); // never persisted
    EffectSaveRepository& repository = defaultEffectSaveRepository();

    repository.updateDeadline(EFFECT_TABLE_MUTE, ghost.name, 1, 1);
    repository.deleteDeadline(EFFECT_TABLE_MUTE, ghost.name);
    repository.updateRemain(EFFECT_TABLE_CARNELIAN_FORCE_SCROLL, ghost.name, 1);
    repository.deleteRemain(EFFECT_TABLE_CARNELIAN_FORCE_SCROLL, ghost.name);
    repository.updateEnemyErase(ghost.name, 1, 1, "nobody");
    repository.deleteEnemyErase(ghost.name, "nobody");

    DWORD remainTurn = 0;
    EXPECT_TRUE(repository.loadDeadlines(EFFECT_TABLE_MUTE, ghost.name).empty());
    EXPECT_FALSE(repository.loadRemain(EFFECT_TABLE_CARNELIAN_FORCE_SCROLL, ghost.name, remainTurn));
    EXPECT_TRUE(repository.loadEnemyErases(ghost.name).empty());
}

// --- FlagSet against real MySQL -------------------------------------------

class FlagSetMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        PlayerFixtures::removeAll();
        execSQL(std::string("DELETE FROM FlagSet WHERE OwnerID IN ") + PlayerFixtures::nameList());
    }
};

TEST_F(FlagSetMySQL, InsertLoadUpdateRemoveRoundTrip) {
    PlayerFixture slayer = PlayerFixtures::lowLevelSlayer();
    slayer.persist();
    FlagSetRepository& repository = defaultFlagSetRepository();
    std::string text;

    EXPECT_FALSE(repository.load(slayer.name, text));

    repository.insert(slayer.name, "101010101010101010101010");
    ASSERT_TRUE(repository.load(slayer.name, text));
    EXPECT_EQ("101010101010101010101010", text);

    repository.update(slayer.name, "000000000000000000000001");
    ASSERT_TRUE(repository.load(slayer.name, text));
    EXPECT_EQ("000000000000000000000001", text);

    repository.remove(slayer.name);
    EXPECT_FALSE(repository.load(slayer.name, text));
}

TEST_F(FlagSetMySQL, PrimaryKeyRefusesASecondInsertWhileInsertEmptyIfMissingIsANoOp) {
    PlayerFixture vampire = PlayerFixtures::lowLevelVampire();
    PlayerFixture ghost = PlayerFixtures::highLevelVampire();
    vampire.persist();
    FlagSetRepository& repository = defaultFlagSetRepository();
    std::string text;

    repository.insert(vampire.name, "111100000000000000000000");
    EXPECT_ANY_THROW(repository.insert(vampire.name, "000000000000000000000000"));
    repository.insertEmptyIfMissing(vampire.name); // INSERT IGNORE: keeps the row that is there
    ASSERT_TRUE(repository.load(vampire.name, text));
    EXPECT_EQ("111100000000000000000000", text);

    repository.insertEmptyIfMissing(ghost.name); // no row yet: an empty one appears
    ASSERT_TRUE(repository.load(ghost.name, text));
    EXPECT_EQ("", text);
}

// --- SMSAddressBook against real MySQL ------------------------------------

class SMSAddressMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        PlayerFixtures::removeAll();
        execSQL(std::string("DELETE FROM SMSAddressBook WHERE OwnerID IN ") + PlayerFixtures::nameList());
    }

    static const SMSAddressRow* find(const std::vector<SMSAddressRow>& rows, int eID) {
        for (size_t r = 0; r < rows.size(); r++)
            if (rows[r].eID == eID)
                return &rows[r];
        return NULL;
    }
};

TEST_F(SMSAddressMySQL, InsertLoadRemoveRoundTripScopedToTheOwner) {
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    PlayerFixture other = PlayerFixtures::midLevelOusters();
    slayer.persist();
    other.persist();
    SMSAddressRepository& repository = defaultSMSAddressRepository();

    repository.insert(slayer.name, 1, "friendone", "Custom One", "01011112222");
    repository.insert(slayer.name, 2, "friendtwo", "Custom Two", "01033334444");
    repository.insert(other.name, 1, "someone", "Elsewhere", "01055556666");

    std::vector<SMSAddressRow> rows = repository.load(slayer.name);
    ASSERT_EQ(2u, rows.size());
    const SMSAddressRow* one = find(rows, 1);
    ASSERT_TRUE(one != NULL);
    EXPECT_EQ("friendone", one->characterName);
    EXPECT_EQ("Custom One", one->customName);
    EXPECT_EQ("01011112222", one->number);
    const SMSAddressRow* two = find(rows, 2);
    ASSERT_TRUE(two != NULL);
    EXPECT_EQ("friendtwo", two->characterName);
    EXPECT_EQ("Custom Two", two->customName);
    EXPECT_EQ("01033334444", two->number);

    repository.remove(slayer.name, 1);
    rows = repository.load(slayer.name);
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(2, rows[0].eID);
    EXPECT_EQ(1u, repository.load(other.name).size());
}

TEST_F(SMSAddressMySQL, CompositePrimaryKeyRefusesARepeatedIdForTheSameOwnerOnly) {
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    PlayerFixture other = PlayerFixtures::midLevelOusters();
    slayer.persist();
    other.persist();
    SMSAddressRepository& repository = defaultSMSAddressRepository();

    repository.insert(slayer.name, 5, "a", "b", "c");
    EXPECT_ANY_THROW(repository.insert(slayer.name, 5, "d", "e", "f"));
    repository.insert(other.name, 5, "g", "h", "i"); // same id, other owner: fine

    EXPECT_EQ(1u, repository.load(slayer.name).size());
    EXPECT_EQ(1u, repository.load(other.name).size());
}

// --- GQuestItemObject against real MySQL ----------------------------------

class QuestItemMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        PlayerFixtures::removeAll();
        execSQL(std::string("DELETE FROM GQuestItemObject WHERE OwnerID IN ") + PlayerFixtures::nameList());
    }

    static int count(const std::vector<int>& itemTypes, int itemType) {
        int n = 0;
        for (size_t i = 0; i < itemTypes.size(); i++)
            if (itemTypes[i] == itemType)
                n++;
        return n;
    }
};

TEST_F(QuestItemMySQL, RemoveOneTakesASingleInstanceAndLeavesTheRest) {
    // One row per item instance; the LIMIT 1 on the DELETE is what keeps
    // the second copy of a duplicated item.
    PlayerFixture ousters = PlayerFixtures::lowLevelOusters();
    ousters.persist();
    QuestItemRepository& repository = defaultQuestItemRepository();

    repository.insert(ousters.name, 7);
    repository.insert(ousters.name, 7);
    repository.insert(ousters.name, 9);

    std::vector<int> itemTypes = repository.loadItemTypes(ousters.name);
    ASSERT_EQ(3u, itemTypes.size());
    EXPECT_EQ(2, count(itemTypes, 7));
    EXPECT_EQ(1, count(itemTypes, 9));

    repository.removeOne(ousters.name, 7);
    itemTypes = repository.loadItemTypes(ousters.name);
    ASSERT_EQ(2u, itemTypes.size());
    EXPECT_EQ(1, count(itemTypes, 7));
    EXPECT_EQ(1, count(itemTypes, 9));

    repository.removeOne(ousters.name, 7);
    repository.removeOne(ousters.name, 7); // none left: silent no-op
    itemTypes = repository.loadItemTypes(ousters.name);
    ASSERT_EQ(1u, itemTypes.size());
    EXPECT_EQ(9, itemTypes[0]);
}

TEST_F(QuestItemMySQL, RowsAreScopedToTheOwner) {
    PlayerFixture slayer = PlayerFixtures::lowLevelSlayer();
    PlayerFixture vampire = PlayerFixtures::lowLevelVampire();
    slayer.persist();
    vampire.persist();
    QuestItemRepository& repository = defaultQuestItemRepository();

    repository.insert(slayer.name, 4);
    repository.insert(vampire.name, 5);

    std::vector<int> itemTypes = repository.loadItemTypes(slayer.name);
    ASSERT_EQ(1u, itemTypes.size());
    EXPECT_EQ(4, itemTypes[0]);
    itemTypes = repository.loadItemTypes(vampire.name);
    ASSERT_EQ(1u, itemTypes.size());
    EXPECT_EQ(5, itemTypes[0]);
}

// --- zone configuration against real MySQL --------------------------------
// The config tables ship seeded in initdb/DARKEDEN.sql (zone groups 1
// and 2, the real zones), so these tests add rows with ids far above the
// shipped range and assert on those, never on the seeded set's size.

const int IT_ZONE_GROUP = 31000;
const int IT_ZONE_GROUP_2 = 31001;
const int IT_ZONE = 31000;
const int IT_ZONE_2 = 31001;
const int IT_ZONE_3 = 31003;
const int IT_SERVER = 250; // ServerID is tinyint unsigned

class ZoneInfoMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        execSQL("DELETE FROM ZoneGroupInfo WHERE ZoneGroupID >= 31000");
        execSQL("DELETE FROM ZoneInfo WHERE ZoneID >= 31000");
        execSQL("DELETE FROM ZoneTriggers WHERE ZoneID >= 31000");
        execSQL("DELETE FROM EffectPKZoneRegen WHERE ID >= 31000");
        execSQL("DELETE FROM WayPointInfo WHERE ZoneID >= 31000");
    }

    static bool contains(const std::vector<int>& ids, int id) {
        for (size_t i = 0; i < ids.size(); i++)
            if (ids[i] == id)
                return true;
        return false;
    }
};

TEST_F(ZoneInfoMySQL, ZoneGroupIDsComeBackOrderedOnlyWhenAsked) {
    execSQL("INSERT INTO ZoneGroupInfo (ZoneGroupID, ServerID) VALUES (31001, 1)");
    execSQL("INSERT INTO ZoneGroupInfo (ZoneGroupID, ServerID) VALUES (31000, 1)");

    std::vector<int> ordered = defaultZoneInfoRepository().loadZoneGroupIDs(true);
    ASSERT_GE(ordered.size(), 2u);
    EXPECT_EQ(IT_ZONE_GROUP, ordered[ordered.size() - 2]);
    EXPECT_EQ(IT_ZONE_GROUP_2, ordered[ordered.size() - 1]);

    std::vector<int> unordered = defaultZoneInfoRepository().loadZoneGroupIDs(false);
    EXPECT_EQ(ordered.size(), unordered.size());
    EXPECT_TRUE(contains(unordered, IT_ZONE_GROUP));
    EXPECT_TRUE(contains(unordered, IT_ZONE_GROUP_2));
}

TEST_F(ZoneInfoMySQL, ZoneIDsOfAGroupComeBackOrderedOnlyWhenAsked) {
    execSQL("INSERT INTO ZoneInfo (ZoneID, ZoneGroupID) VALUES (31003, 31000)");
    execSQL("INSERT INTO ZoneInfo (ZoneID, ZoneGroupID) VALUES (31001, 31000)");
    execSQL("INSERT INTO ZoneInfo (ZoneID, ZoneGroupID) VALUES (31000, 31001)"); // another group

    std::vector<int> ordered = defaultZoneInfoRepository().loadZoneIDsOfGroup(IT_ZONE_GROUP, true);
    ASSERT_EQ(2u, ordered.size());
    EXPECT_EQ(IT_ZONE_2, ordered[0]);
    EXPECT_EQ(IT_ZONE_3, ordered[1]);

    std::vector<int> unordered = defaultZoneInfoRepository().loadZoneIDsOfGroup(IT_ZONE_GROUP, false);
    ASSERT_EQ(2u, unordered.size());
    EXPECT_TRUE(contains(unordered, IT_ZONE_2));
    EXPECT_TRUE(contains(unordered, IT_ZONE_3));
}

TEST_F(ZoneInfoMySQL, LoadZoneInfosReturnsEveryColumnInSelectPosition) {
    execSQL("INSERT INTO ZoneInfo (ZoneID, ZoneGroupID, Type, Level, AccessMode, OwnerId, PayPlayZone, PremiumZone, "
            "PKZone, NoPortalZone, HolyLand, Available, OpenLevel, SmpFileName, SsiFileName, FullName, ShortName) "
            "VALUES (31000, 31001, 'NPC_SHOP', 7, 'PRIVATE', 'itowner', 1, 0, 1, 0, 1, 0, 13, 'it.smp', 'it.ssi', "
            "'It Full', 'ItShort')");

    std::vector<ZoneInfoRow> rows = defaultZoneInfoRepository().loadZoneInfos();
    const ZoneInfoRow* found = NULL;
    for (size_t r = 0; r < rows.size(); r++)
        if (rows[r].zoneID == IT_ZONE)
            found = &rows[r];
    ASSERT_TRUE(found != NULL);
    EXPECT_EQ(31001, found->zoneGroupID);
    EXPECT_EQ("NPC_SHOP", found->type);
    EXPECT_EQ(7, found->level);
    EXPECT_EQ("PRIVATE", found->accessMode);
    EXPECT_EQ("itowner", found->ownerID);
    EXPECT_EQ(1, found->payPlayZone);
    EXPECT_EQ(0, found->premiumZone);
    EXPECT_EQ(1, found->pkZone);
    EXPECT_EQ(0, found->noPortalZone);
    EXPECT_EQ(1, found->holyLand);
    EXPECT_EQ(0, found->available);
    EXPECT_EQ(13, found->openLevel);
    EXPECT_EQ("it.smp", found->smpFilename);
    EXPECT_EQ("it.ssi", found->ssiFilename);
    EXPECT_EQ("It Full", found->fullName);
    EXPECT_EQ("ItShort", found->shortName);
}

TEST_F(ZoneInfoMySQL, LoadResurrectLocationsReturnsEveryColumnInSelectPosition) {
    execSQL("INSERT INTO ZoneInfo (ZoneID, SResurrectZoneID, SResurrectX, SResurrectY, VResurrectZoneID, "
            "VResurrectX, VResurrectY, OResurrectZoneID, OResurrectX, OResurrectY) "
            "VALUES (31000, 2, 3, 4, 5, 6, 7, 8, 9, 10)");

    std::vector<ResurrectLocationRow> rows = defaultZoneInfoRepository().loadResurrectLocations();
    const ResurrectLocationRow* found = NULL;
    for (size_t r = 0; r < rows.size(); r++)
        if (rows[r].zoneID == IT_ZONE)
            found = &rows[r];
    ASSERT_TRUE(found != NULL);
    EXPECT_EQ(2, found->slayerZoneID);
    EXPECT_EQ(3, found->slayerX);
    EXPECT_EQ(4, found->slayerY);
    EXPECT_EQ(5, found->vampireZoneID);
    EXPECT_EQ(6, found->vampireX);
    EXPECT_EQ(7, found->vampireY);
    EXPECT_EQ(8, found->oustersZoneID);
    EXPECT_EQ(9, found->oustersX);
    EXPECT_EQ(10, found->oustersY);
}

TEST_F(ZoneInfoMySQL, TriggersRegenRectsAndWayPointsAreScopedToTheZoneAndRace) {
    execSQL("INSERT INTO ZoneTriggers (TriggerID, ZoneID, X1, Y1, X2, Y2, Conditions, Actions, CounterActions) "
            "VALUES (31000, 31000, 1, 2, 3, 4, '', '', '')");
    execSQL("INSERT INTO ZoneTriggers (TriggerID, ZoneID, X1, Y1, X2, Y2, Conditions, Actions, CounterActions) "
            "VALUES (31001, 31001, 9, 9, 9, 9, '', '', '')");
    execSQL(
        "INSERT INTO EffectPKZoneRegen (ID, ZoneID, LeftX, TopY, RightX, BottomY) VALUES (31000, 31000, 5, 6, 7, 8)");
    execSQL(
        "INSERT INTO EffectPKZoneRegen (ID, ZoneID, LeftX, TopY, RightX, BottomY) VALUES (31001, 31001, 9, 9, 9, 9)");
    execSQL("INSERT INTO WayPointInfo (ZoneID, X, Y, Race) VALUES (31000, 10, 11, 2)");
    execSQL("INSERT INTO WayPointInfo (ZoneID, X, Y, Race) VALUES (31000, 12, 13, 0)"); // other race
    execSQL("INSERT INTO WayPointInfo (ZoneID, X, Y, Race) VALUES (31001, 14, 15, 2)"); // other zone

    std::vector<ZoneRectRow> triggers = defaultZoneInfoRepository().loadTriggerRects(IT_ZONE);
    ASSERT_EQ(1u, triggers.size());
    EXPECT_EQ(1, triggers[0].left);
    EXPECT_EQ(2, triggers[0].top);
    EXPECT_EQ(3, triggers[0].right);
    EXPECT_EQ(4, triggers[0].bottom);

    // quest/TriggerManager's follow-up read of one rectangle's scripts: the
    // five columns, scoped to the zone AND all four coordinates — a
    // same-zone row differing in exactly one coordinate is seeded for each
    // of X1, Y1, X2 and Y2, so dropping any one clause would return two rows
    // (and the other zone's 9,9,9,9 row stays out).
    execSQL("INSERT INTO ZoneTriggers (TriggerID, TriggerType, ZoneID, X1, Y1, X2, Y2, Conditions, Actions, "
            "CounterActions) VALUES (31002, 'MONSTER', 31000, 1, 2, 3, 5, ' c2 ', 'a2', 'ca2')");
    execSQL("INSERT INTO ZoneTriggers (TriggerID, TriggerType, ZoneID, X1, Y1, X2, Y2, Conditions, Actions, "
            "CounterActions) VALUES (31003, 'MONSTER', 31000, 0, 2, 3, 4, '', '', '')");
    execSQL("INSERT INTO ZoneTriggers (TriggerID, TriggerType, ZoneID, X1, Y1, X2, Y2, Conditions, Actions, "
            "CounterActions) VALUES (31004, 'MONSTER', 31000, 1, 0, 3, 4, '', '', '')");
    execSQL("INSERT INTO ZoneTriggers (TriggerID, TriggerType, ZoneID, X1, Y1, X2, Y2, Conditions, Actions, "
            "CounterActions) VALUES (31005, 'MONSTER', 31000, 1, 2, 0, 4, '', '', '')");
    std::vector<ZoneTriggerRow> scripts = defaultZoneInfoRepository().loadZoneTriggers(IT_ZONE, 1, 2, 3, 4);
    ASSERT_EQ(1u, scripts.size());
    EXPECT_EQ(31000, scripts[0].triggerID);
    EXPECT_EQ("QUEST", scripts[0].triggerType); // the column's default
    EXPECT_EQ("", scripts[0].conditions);
    scripts = defaultZoneInfoRepository().loadZoneTriggers(IT_ZONE, 1, 2, 3, 5);
    ASSERT_EQ(1u, scripts.size());
    EXPECT_EQ(31002, scripts[0].triggerID);
    EXPECT_EQ("MONSTER", scripts[0].triggerType);
    EXPECT_EQ(" c2 ", scripts[0].conditions); // untrimmed: the caller trims
    EXPECT_EQ("a2", scripts[0].actions);
    EXPECT_EQ("ca2", scripts[0].counterActions);
    EXPECT_TRUE(defaultZoneInfoRepository().loadZoneTriggers(IT_ZONE_2, 1, 2, 3, 4).empty());

    std::vector<ZoneRectRow> regens = defaultZoneInfoRepository().loadPKZoneRegenRects(IT_ZONE);
    ASSERT_EQ(1u, regens.size());
    EXPECT_EQ(5, regens[0].left);
    EXPECT_EQ(6, regens[0].top);
    EXPECT_EQ(7, regens[0].right);
    EXPECT_EQ(8, regens[0].bottom);

    std::vector<ZonePointRow> points = defaultZoneInfoRepository().loadWayPoints(IT_ZONE, 2);
    ASSERT_EQ(1u, points.size());
    EXPECT_EQ(10, points[0].x);
    EXPECT_EQ(11, points[0].y);

    std::vector<WayPointRow> all = defaultZoneInfoRepository().loadAllWayPoints();
    int seen = 0;
    for (size_t w = 0; w < all.size(); w++) {
        if (all[w].zoneID == IT_ZONE && all[w].x == 10 && all[w].y == 11 && all[w].race == 2)
            seen++;
        if (all[w].zoneID == IT_ZONE && all[w].x == 12 && all[w].y == 13 && all[w].race == 0)
            seen++;
        if (all[w].zoneID == IT_ZONE_2 && all[w].x == 14 && all[w].y == 15 && all[w].race == 2)
            seen++;
    }
    EXPECT_EQ(3, seen);
}

// --- Messages against real MySQL ------------------------------------------

class MessageMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        PlayerFixtures::removeAll();
        execSQL(std::string("DELETE FROM Messages WHERE Receiver IN ") + PlayerFixtures::nameList());
    }
};


TEST_F(MessageMySQL, TheThreeUnionNoticeSpellingsAreOneStatementWithThreeTexts) {
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    slayer.persist();
    MessageRepository& repository = defaultMessageRepository();

    // NOTE: this test proves the three spellings are EQUIVALENT. It cannot
    // prove the mapping is right — swap two enumerators and every
    // assertion below still passes, because MySQL cannot tell the
    // spellings apart.
    //
    // The union handlers issue this INSERT three ways — backticked or not,
    // with or without a space before the VALUES list. MySQL ignores both
    // differences, so all three land the same shape of row; the
    // repository keeps them apart to send each caller's exact bytes, not
    // because they behave differently. This test is what says so.
    repository.insertUnionNotice(UNION_NOTICE_PLAIN, slayer.name, "plain");
    repository.insertUnionNotice(UNION_NOTICE_QUOTED_SPACED, slayer.name, "quoted spaced");
    repository.insertUnionNotice(UNION_NOTICE_QUOTED, slayer.name, "quoted");

    EXPECT_EQ(3u, repository.loadMessages(slayer.name).size());

    const std::string where = "FROM Messages WHERE Receiver='" + slayer.name + "' AND Message=";
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) " + where + "'plain'"));
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) " + where + "'quoted spaced'"));
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) " + where + "'quoted'"));

    // None of the three names Sender, so it defaults to '' — as
    // insertMessage's own row does. Asserted for all three, since "the
    // same shape of row" is the claim.
    EXPECT_EQ("", queryScalar("SELECT Sender " + where + "'plain'"));
    EXPECT_EQ("", queryScalar("SELECT Sender " + where + "'quoted spaced'"));
    EXPECT_EQ("", queryScalar("SELECT Sender " + where + "'quoted'"));

    // And they drain through the same reader the zone already uses.
    repository.deleteMessages(slayer.name);
    EXPECT_TRUE(repository.loadMessages(slayer.name).empty());
}

TEST_F(MessageMySQL, InsertLoadAndDeleteAreScopedToTheReceiver) {
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    PlayerFixture other = PlayerFixtures::midLevelOusters();
    slayer.persist();
    other.persist();
    MessageRepository& repository = defaultMessageRepository();

    repository.insertMessage(slayer.name, "first");
    repository.insertMessage(slayer.name, "first"); // keyless: a repeat is a second row
    repository.insertMessage(other.name, "elsewhere");

    std::vector<std::string> messages = repository.loadMessages(slayer.name);
    ASSERT_EQ(2u, messages.size());
    EXPECT_EQ("first", messages[0]);
    EXPECT_EQ("first", messages[1]);

    repository.deleteMessages(slayer.name);
    EXPECT_TRUE(repository.loadMessages(slayer.name).empty());
    EXPECT_EQ(1u, repository.loadMessages(other.name).size());
}

// --- Event200501 against real MySQL (the dist connection) -----------------

class ComebackEventMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        execSQL("DELETE FROM Event200501Main WHERE PlayerID = 'itaccount'");
        execSQL("DELETE FROM Event200501Recommend WHERE PlayerID = 'itaccount'");
        execSQL("DELETE FROM DonationPersonal200501 WHERE Name LIKE 'it-%'");
        execSQL("DELETE FROM DonationGuild200501 WHERE Name LIKE 'it-%'");
    }
};

TEST_F(ComebackEventMySQL, ThePredicatesFollowTheZeroDateColumns) {
    ComebackEventRepository& repository = defaultComebackEventRepository();

    EXPECT_FALSE(repository.hasUnclaimedItem("itaccount"));
    EXPECT_FALSE(repository.hasUnclaimedPremiumItem("itaccount"));
    EXPECT_FALSE(repository.hasUnclaimedRecommendItem("itaccount"));

    execSQL("INSERT INTO Event200501Main (PlayerID) VALUES ('itaccount')"); // every date at the zero default
    EXPECT_TRUE(repository.hasUnclaimedItem("itaccount"));
    EXPECT_FALSE(repository.hasUnclaimedPremiumItem("itaccount")); // never paid

    execSQL("UPDATE Event200501Main SET PayPremiumDate = '2005-01-02' WHERE PlayerID = 'itaccount'");
    EXPECT_TRUE(repository.hasUnclaimedPremiumItem("itaccount"));

    execSQL("UPDATE Event200501Main SET RecvItemDate = '2005-01-03', RecvPremiumItemDate = '2005-01-03' "
            "WHERE PlayerID = 'itaccount'");
    EXPECT_FALSE(repository.hasUnclaimedItem("itaccount"));
    EXPECT_FALSE(repository.hasUnclaimedPremiumItem("itaccount"));

    execSQL("INSERT INTO Event200501Recommend (PlayerID, Recommender) VALUES ('itaccount', 'friend')");
    EXPECT_TRUE(repository.hasUnclaimedRecommendItem("itaccount"));
    execSQL("UPDATE Event200501Recommend SET RecvItemDate = '2005-01-04' WHERE PlayerID = 'itaccount'");
    EXPECT_FALSE(repository.hasUnclaimedRecommendItem("itaccount"));
}

// CGGetEventItemHandler's hand-out: the date columns come back as the text
// the handler compares to '0000-00-00' (the zero default first, then what
// each stamp wrote — today's date, since the stamps write now() into a
// DATE column); each read is false for an account without a row, and the
// recommend read takes a row by PlayerID while its stamp keys on UniqueID.
TEST_F(ComebackEventMySQL, HandOutReadsTheDateTextsAndTheStampsWriteToday) {
    ComebackEventRepository& repository = defaultComebackEventRepository();
    std::string recv, pay, recvPremium;
    int uniqueID = -1;

    EXPECT_FALSE(repository.loadMainRecvItemDate("itaccount", recv));
    EXPECT_FALSE(repository.loadMainPremiumDates("itaccount", pay, recvPremium));
    EXPECT_FALSE(repository.loadRecommendRow("itaccount", uniqueID, recv));
    EXPECT_EQ(-1, uniqueID);

    execSQL("INSERT INTO Event200501Main (PlayerID, PayPremiumDate) VALUES ('itaccount', '2005-01-02')");
    ASSERT_TRUE(repository.loadMainRecvItemDate("itaccount", recv));
    EXPECT_EQ("0000-00-00", recv);
    ASSERT_TRUE(repository.loadMainPremiumDates("itaccount", pay, recvPremium));
    EXPECT_EQ("2005-01-02", pay);
    EXPECT_EQ("0000-00-00", recvPremium);

    const std::string today = queryScalar("SELECT CURDATE()");
    repository.markMainItemReceived("itaccount");
    ASSERT_TRUE(repository.loadMainRecvItemDate("itaccount", recv));
    EXPECT_EQ(today, recv);
    EXPECT_FALSE(repository.hasUnclaimedItem("itaccount")); // the zone's predicate agrees
    ASSERT_TRUE(repository.loadMainPremiumDates("itaccount", pay, recvPremium));
    EXPECT_EQ("0000-00-00", recvPremium); // the other stamp is untouched
    EXPECT_TRUE(repository.hasUnclaimedPremiumItem("itaccount"));
    repository.markMainPremiumItemReceived("itaccount");
    ASSERT_TRUE(repository.loadMainPremiumDates("itaccount", pay, recvPremium));
    EXPECT_EQ(today, recvPremium);
    EXPECT_EQ("2005-01-02", pay);
    EXPECT_FALSE(repository.hasUnclaimedPremiumItem("itaccount")); // the zone's predicate agrees

    execSQL("INSERT INTO Event200501Recommend (PlayerID, Recommender) VALUES ('itaccount', 'friend')");
    ASSERT_TRUE(repository.loadRecommendRow("itaccount", uniqueID, recv));
    EXPECT_EQ("0000-00-00", recv);
    EXPECT_EQ(atoi(queryScalar("SELECT UniqueID FROM Event200501Recommend WHERE PlayerID = 'itaccount'").c_str()),
              uniqueID);
    repository.markRecommendItemReceived(uniqueID);
    ASSERT_TRUE(repository.loadRecommendRow("itaccount", uniqueID, recv));
    EXPECT_EQ(today, recv);
    EXPECT_FALSE(repository.hasUnclaimedRecommendItem("itaccount"));
}

// CGDonationMoneyHandler's record: the two positional INSERTs land every
// value in the column its position names, the counts are scoped to the
// name AND the world id (a row of the same name in another world is not
// counted), and a name with no rows counts 0.
TEST_F(ComebackEventMySQL, DonationsAreRecordedPositionallyAndCountedPerNameAndWorld) {
    ComebackEventRepository& repository = defaultComebackEventRepository();

    EXPECT_EQ(0, repository.countPersonalDonations("it-char", 4));
    EXPECT_EQ(0, repository.countGuildDonations("it-char", 4));

    repository.insertPersonalDonation("it-acct", "it-char", 4, 1234567u);
    repository.insertPersonalDonation("it-acct", "it-char", 5, 1u); // another world
    repository.insertGuildDonation(31000, "it-guild", "it-acct", "it-char", 4, 99u);

    EXPECT_EQ(1, repository.countPersonalDonations("it-char", 4));
    EXPECT_EQ(1, repository.countPersonalDonations("it-char", 5));
    EXPECT_EQ(1, repository.countGuildDonations("it-char", 4));
    EXPECT_EQ(0, repository.countGuildDonations("it-char", 5));

    const std::string personal = " FROM DonationPersonal200501 WHERE Name = 'it-char' AND WorldID = 4";
    EXPECT_EQ("it-acct", queryScalar("SELECT PlayerID" + personal));
    EXPECT_EQ("1234567", queryScalar("SELECT Amount" + personal));
    EXPECT_EQ("1", queryScalar("SELECT DonationDateTime > '2026-01-01'" + personal));

    const std::string guild = " FROM DonationGuild200501 WHERE Name = 'it-char'";
    EXPECT_EQ("31000", queryScalar("SELECT GuildID" + guild));
    EXPECT_EQ("it-guild", queryScalar("SELECT GuildName" + guild));
    EXPECT_EQ("it-acct", queryScalar("SELECT PlayerID" + guild));
    EXPECT_EQ("4", queryScalar("SELECT WorldID" + guild));
    EXPECT_EQ("99", queryScalar("SELECT Amount" + guild));
    EXPECT_EQ("1", queryScalar("SELECT DonationDateTime > '2026-01-01'" + guild));
}

// --- BulletinBoardObject against real MySQL -------------------------------

class BulletinBoardMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        execSQL("DELETE FROM BulletinBoardObject WHERE ServerID = 250");
    }
};

TEST_F(BulletinBoardMySQL, InsertLoadForZoneAndRemove) {
    BulletinBoardRepository& repository = defaultBulletinBoardRepository();

    EXPECT_EQ(1u, repository.insert(IT_SERVER, IT_ZONE, 40, 50, "hello board", 673, "2030-01-02 03:04:05"));
    EXPECT_EQ(1u, repository.insert(IT_SERVER, IT_ZONE_2, 1, 1, "other zone", 673, "2030-01-02 03:04:05"));

    std::vector<BulletinBoardRow> rows = repository.loadForZone(IT_SERVER, IT_ZONE);
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(40, rows[0].x);
    EXPECT_EQ(50, rows[0].y);
    EXPECT_EQ("hello board", rows[0].message);
    EXPECT_EQ(673, rows[0].type);
    EXPECT_EQ("2030-01-02 03:04:05", rows[0].timeLimit);
    EXPECT_TRUE(repository.loadForZone(IT_SERVER + 1, IT_ZONE).empty()); // other server

    repository.remove(rows[0].id);
    EXPECT_TRUE(repository.loadForZone(IT_SERVER, IT_ZONE).empty());
    EXPECT_EQ(1u, repository.loadForZone(IT_SERVER, IT_ZONE_2).size());
}

// --- RegenZonePosition against real MySQL ---------------------------------

class RegenZoneMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        execSQL("DELETE FROM RegenZonePosition WHERE ID >= 31000");
    }
};

TEST_F(RegenZoneMySQL, LoadPositionsReturnsEveryColumnInSelectPosition) {
    execSQL("INSERT INTO RegenZonePosition (ID, ZoneID, ZoneX, ZoneY, Owner) VALUES (31000, 31000, 20, 30, 2)");

    std::vector<RegenZoneRow> rows = defaultRegenZoneRepository().loadPositions();
    const RegenZoneRow* found = NULL;
    for (size_t r = 0; r < rows.size(); r++)
        if (rows[r].id == 31000)
            found = &rows[r];
    ASSERT_TRUE(found != NULL);
    EXPECT_EQ(IT_ZONE, found->zoneID);
    EXPECT_EQ(20, found->zoneX);
    EXPECT_EQ(30, found->zoneY);
    EXPECT_EQ(2, found->owner);
}

// --- the balance and game-info tables against real MySQL ------------------
// Every one of these tables ships seeded in initdb/DARKEDEN.sql and is
// read-only for the gameserver, so the tests assert on the shipped data's
// shape (a maximum exists, the rows stay within it, the lists are not
// empty — exactly what the boot-time loaders require) rather than on
// rows they insert. The one write-free quirk worth pinning is the MAX()
// probe over nothing: MySQL answers with one NULL row; the repository
// reports "no maximum".

TEST(BalanceInfoMySQL, EveryLadderHasAMaximumAndItsRowsStayWithinIt) {
    BalanceInfoRepository& repository = defaultBalanceInfoRepository();

    for (int t = 0; t < LEVEL_EXP_TABLE_MAX; t++) {
        LevelExpTable table = (LevelExpTable)t;
        SCOPED_TRACE(t);
        int maxLevel = -1;
        ASSERT_TRUE(repository.loadMaxLevel(table, maxLevel));
        EXPECT_GT(maxLevel, 0);

        std::vector<LevelExpRow> rows = repository.loadLevels(table);
        ASSERT_FALSE(rows.empty());
        for (size_t r = 0; r < rows.size(); r++) {
            EXPECT_LE(rows[r].level, maxLevel);
            if (table != LEVEL_EXP_TABLE_OUSTERS_EXP)
                EXPECT_EQ(0, rows[r].skillPointBonus); // not selected for the other four tables
        }
    }
}

TEST(BalanceInfoMySQL, RankLaddersExistPerRankTypeAndAMissingTypeHasNoMaximum) {
    BalanceInfoRepository& repository = defaultBalanceInfoRepository();

    for (int rankType = 0; rankType < 3; rankType++) {
        SCOPED_TRACE(rankType);
        int maxLevel = -1;
        ASSERT_TRUE(repository.loadMaxRankLevel(rankType, maxLevel));
        EXPECT_GT(maxLevel, 0);
        std::vector<LevelExpRow> rows = repository.loadRankLevels(rankType);
        ASSERT_FALSE(rows.empty());
        for (size_t r = 0; r < rows.size(); r++)
            EXPECT_LE(rows[r].level, maxLevel);
    }

    // MAX() over a WHERE that matches nothing is ONE row holding NULL —
    // reported as "no maximum", where the inline loader would have
    // called atoi(NULL) on it.
    int maxLevel = -1;
    EXPECT_FALSE(repository.loadMaxRankLevel(99, maxLevel));
    EXPECT_TRUE(repository.loadRankLevels(99).empty());
}

TEST(BalanceInfoMySQL, DomainLaddersExistAndAMissingDomainHasNoMaximum) {
    BalanceInfoRepository& repository = defaultBalanceInfoRepository();

    int maxLevel = -1;
    ASSERT_TRUE(repository.loadMaxDomainLevel(0, maxLevel));
    EXPECT_GT(maxLevel, 0);
    std::vector<DomainLevelRow> rows = repository.loadDomainLevels(0);
    ASSERT_FALSE(rows.empty());
    for (size_t r = 0; r < rows.size(); r++) {
        EXPECT_EQ(0, rows[r].domainType);
        EXPECT_LE(rows[r].level, maxLevel);
    }

    EXPECT_FALSE(repository.loadMaxDomainLevel(99, maxLevel));
    EXPECT_TRUE(repository.loadDomainLevels(99).empty());
}

TEST(BalanceInfoMySQL, TheFameLimitTableIsNotInTheShippedSchema) {
    // FameLimitInfoManager is never constructed by the gameserver, and
    // its table is absent from initdb/DARKEDEN.sql: the dead loader would
    // fail on its first query. Pinned so a schema that grows the table
    // (or a boot that revives the manager) shows up here.
    int maxLevel = -1;
    EXPECT_ANY_THROW(defaultBalanceInfoRepository().loadMaxFameLevel(0, maxLevel));
}

TEST(BalanceInfoMySQL, ThePetTablesLoad) {
    BalanceInfoRepository& repository = defaultBalanceInfoRepository();

    EXPECT_FALSE(repository.loadPetExp().empty());
    EXPECT_FALSE(repository.loadPetAttrBalance().empty());
    EXPECT_FALSE(repository.loadPetAttrRatios().empty());
}

TEST(GameInfoMySQL, EveryMaximumExistsAndEveryListIsNonEmpty) {
    GameInfoRepository& repository = defaultGameInfoRepository();
    int maximum = -1;

    ASSERT_TRUE(repository.loadMaxSkillType(maximum));
    EXPECT_GT(maximum, 0);
    std::vector<SkillParentRow> tree = repository.loadSkillTree();
    ASSERT_FALSE(tree.empty());
    for (size_t r = 0; r < tree.size(); r++)
        EXPECT_LE(tree[r].skillType, maximum);

    ASSERT_TRUE(repository.loadMaxRankBonusType(maximum));
    std::vector<RankBonusInfoRow> bonuses = repository.loadRankBonusInfos();
    ASSERT_FALSE(bonuses.empty());
    for (size_t r = 0; r < bonuses.size(); r++)
        EXPECT_LE(bonuses[r].type, maximum);

    ASSERT_TRUE(repository.loadMaxPetType(maximum));
    std::vector<PetTypeRow> pets = repository.loadPetTypes();
    ASSERT_FALSE(pets.empty());
    for (size_t r = 0; r < pets.size(); r++)
        EXPECT_LE(pets[r].petType, maximum);

    ASSERT_TRUE(repository.loadMaxWorldID(maximum));
    std::vector<GameServerGroupRow> groups = repository.loadGameServerGroups();
    ASSERT_FALSE(groups.empty());
    for (size_t r = 0; r < groups.size(); r++)
        EXPECT_LE(groups[r].worldID, maximum);

    ASSERT_TRUE(repository.loadMaxBloodBibleBonusType(maximum));
    std::vector<BloodBibleBonusRow> bibles = repository.loadBloodBibleBonuses();
    ASSERT_FALSE(bibles.empty());
    for (size_t r = 0; r < bibles.size(); r++)
        EXPECT_LE(bibles[r].type, maximum);
}

TEST(GameInfoMySQL, EveryMonsterNameListIsNonEmpty) {
    // MonsterNameManager::init throws on an empty list, so the shipped
    // data must carry all four — including the event monsters' last
    // names.
    for (int l = 0; l < MONSTER_NAME_LIST_MAX; l++) {
        SCOPED_TRACE(l);
        EXPECT_FALSE(defaultGameInfoRepository().loadMonsterNames((MonsterNameList)l).empty());
    }
}

// --- the remaining config loaders ----------------------------------------
// All read-only, all shipped seeded: the tests assert the shape the boot
// requires (the weather and grade tables' exact row counts, the others
// non-empty and internally consistent) plus the two per-zone reads
// against rows they insert with ids from 31000 up.

TEST(ConfigLoadersMySQL, WeatherAndGradeTablesHaveTheirFixedRowCounts) {
    // WeatherInfoManager asserts 12 rows, ItemGradeManager 10 — the shipped
    // data must satisfy both or the boot would assert-fail.
    std::vector<WeatherRow> weather = defaultGameInfoRepository().loadWeather();
    ASSERT_EQ(12u, weather.size());
    for (size_t r = 0; r < weather.size(); r++)
        EXPECT_TRUE(weather[r].month >= 1 && weather[r].month <= 12);

    std::vector<ItemGradeRatioRow> grades = defaultGameInfoRepository().loadItemGradeRatios();
    ASSERT_EQ(10u, grades.size());
    for (size_t r = 0; r < grades.size(); r++)
        EXPECT_TRUE(grades[r].grade >= 1 && grades[r].grade <= 10);
}

// The shop actions' per-NPC read: only the NPC's rows, and nothing for an
// NPC without rows. The SELECT has no ORDER BY, so the order the actions
// append the ids in is the optimizer's choice (the table's only index is
// the unique key on ID); the test checks membership, not order.
TEST(ConfigLoadersMySQL, ShopTemplateIDsAreReadPerNPC) {
    execSQL("DELETE FROM ShopTemplate WHERE ID >= 31000");
    execSQL("INSERT INTO ShopTemplate (ID, ShopType, ItemClass, MinItemType, MaxItemType, MinOptionLevel, "
            "MaxOptionLevel, NPCID) VALUES (31001, 1, 2, 3, 4, 5, 6, 31000)");
    execSQL("INSERT INTO ShopTemplate (ID, ShopType, ItemClass, MinItemType, MaxItemType, MinOptionLevel, "
            "MaxOptionLevel, NPCID) VALUES (31000, 1, 2, 3, 4, 5, 6, 31000)");
    execSQL("INSERT INTO ShopTemplate (ID, ShopType, ItemClass, MinItemType, MaxItemType, MinOptionLevel, "
            "MaxOptionLevel, NPCID) VALUES (31002, 1, 2, 3, 4, 5, 6, 31001)");

    std::vector<int> ids = defaultGameInfoRepository().loadShopTemplateIDsOfNPC(31000);
    ASSERT_EQ(2u, ids.size());
    EXPECT_TRUE((ids[0] == 31000 && ids[1] == 31001) || (ids[0] == 31001 && ids[1] == 31000))
        << ids[0] << " " << ids[1];
    EXPECT_TRUE(defaultGameInfoRepository().loadShopTemplateIDsOfNPC(31002).empty());
    execSQL("DELETE FROM ShopTemplate WHERE ID >= 31000");
}

TEST(ConfigLoadersMySQL, EveryWholeTableConfigListIsNonEmpty) {
    GameInfoRepository& repository = defaultGameInfoRepository();
    EXPECT_FALSE(repository.loadStrings().empty());
    EXPECT_FALSE(repository.loadShopTemplates().empty());
    EXPECT_FALSE(repository.loadLevelNicks().empty());
    EXPECT_FALSE(repository.loadItemMines().empty());
    EXPECT_FALSE(repository.loadDefaultOptionSets().empty());
    EXPECT_FALSE(repository.loadDarkLight().empty());
    EXPECT_FALSE(repository.loadCastleSkills().empty());
    EXPECT_FALSE(repository.loadCastleShrines().empty());
    EXPECT_FALSE(repository.loadLogUserNames().empty());

    std::vector<PKZoneRow> pkZones = defaultZoneInfoRepository().loadPKZones();
    EXPECT_FALSE(pkZones.empty());
    std::vector<EventZoneRow> eventZones = defaultZoneInfoRepository().loadEventZones();
    EXPECT_FALSE(eventZones.empty());
    std::vector<LevelWarZoneRow> levelWarZones = defaultZoneInfoRepository().loadLevelWarZones();
    EXPECT_FALSE(levelWarZones.empty());
}

class GoodsListMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        execSQL("DELETE FROM GoodsListInfo WHERE GoodsID >= 31000");
    }
    virtual void TearDown() {
        execSQL("DELETE FROM GoodsListInfo WHERE GoodsID >= 31000");
    }
};

TEST_F(GoodsListMySQL, GoodsComeFromTheDistConnectionWithoutTheSetKind) {
    // GoodsListInfo is read on the thread's dist connection (same schema
    // in this stack) and the SELECT excludes Kind 'SET'; Limited+0 is the
    // enum ordinal, 1..3. The shipped seed has neither a 'SET' nor a
    // 'FOREVER' row, so both are inserted here: without them the filter
    // and the ordinal's upper bound would pass unexercised.
    execSQL("INSERT INTO GoodsListInfo (GoodsID, Name, Description, Limited, Kind) "
            "VALUES (31000, 'it-set', '', 'LIMITED', 'SET')");
    execSQL("INSERT INTO GoodsListInfo (GoodsID, Name, Description, Limited, Kind) "
            "VALUES (31001, 'it-forever', '', 'FOREVER', 'ETC')");

    std::vector<GoodsInfoRow> goods = defaultGameInfoRepository().loadGoods();
    ASSERT_FALSE(goods.empty());
    bool sawSet = false, sawForever = false;
    for (size_t r = 0; r < goods.size(); r++) {
        EXPECT_TRUE(goods[r].limited >= 1 && goods[r].limited <= 3);
        EXPECT_NE("SET",
                  queryScalar("SELECT Kind FROM GoodsListInfo WHERE GoodsID=" + std::to_string(goods[r].goodsID)));
        if (goods[r].goodsID == 31000)
            sawSet = true;
        if (goods[r].goodsID == 31001) {
            sawForever = true;
            EXPECT_EQ(3, goods[r].limited);
        }
    }
    EXPECT_FALSE(sawSet);
    EXPECT_TRUE(sawForever);
}

class ZoneConfigMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        execSQL("DELETE FROM ZoneInfo WHERE ZoneID >= 31000");
        execSQL("DELETE FROM ZoneEffectInfo WHERE ZoneID >= 31000");
    }
    virtual void TearDown() {
        execSQL("DELETE FROM ZoneInfo WHERE ZoneID >= 31000");
        execSQL("DELETE FROM ZoneEffectInfo WHERE ZoneID >= 31000");
    }
};

TEST_F(ZoneConfigMySQL, ZoneEffectRectsAreScopedToZoneAndEffect) {
    execSQL("INSERT INTO ZoneEffectInfo (ZoneID, EffectID, LeftX, TopY, RightX, BottomY, Value1, Value2, Value3) "
            "VALUES (31000, 7, 1, 2, 3, 4, 5, 6, 7)");
    execSQL("INSERT INTO ZoneEffectInfo (ZoneID, EffectID, LeftX, TopY, RightX, BottomY, Value1, Value2, Value3) "
            "VALUES (31000, 8, 9, 9, 9, 9, 9, 9, 9)"); // other effect
    execSQL("INSERT INTO ZoneEffectInfo (ZoneID, EffectID, LeftX, TopY, RightX, BottomY, Value1, Value2, Value3) "
            "VALUES (31001, 7, 9, 9, 9, 9, 9, 9, 9)"); // other zone

    std::vector<ZoneEffectRow> rows = defaultZoneInfoRepository().loadZoneEffectRects(IT_ZONE, 7);
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(1, rows[0].left);
    EXPECT_EQ(2, rows[0].top);
    EXPECT_EQ(3, rows[0].right);
    EXPECT_EQ(4, rows[0].bottom);
    EXPECT_EQ(5, rows[0].value1);
    EXPECT_EQ(6, rows[0].value2);
    EXPECT_EQ(7, rows[0].value3);

    // EffectDarkness's statement reads the same rows without the values.
    std::vector<ZoneEffectBoundsRow> bounds = defaultZoneInfoRepository().loadZoneEffectBounds(IT_ZONE, 7);
    ASSERT_EQ(1u, bounds.size());
    EXPECT_EQ(1, bounds[0].left);
    EXPECT_EQ(2, bounds[0].top);
    EXPECT_EQ(3, bounds[0].right);
    EXPECT_EQ(4, bounds[0].bottom);
    // Scoped the same way: the other effect's row in this zone, the other zone's
    // row for this effect, and nothing for an effect with no rows at all.
    EXPECT_EQ(1u, defaultZoneInfoRepository().loadZoneEffectBounds(IT_ZONE, 8).size());
    EXPECT_EQ(1u, defaultZoneInfoRepository().loadZoneEffectBounds(IT_ZONE_2, 7).size());
    EXPECT_TRUE(defaultZoneInfoRepository().loadZoneEffectBounds(IT_ZONE, 9).empty());
}

TEST_F(ZoneConfigMySQL, MonsterListsComeFromTheZoneRowOrReportNoRow) {
    execSQL("INSERT INTO ZoneInfo (ZoneID, MonsterList, EventMonsterList) VALUES (31000, 'a:1', 'b:2')");

    std::string monsters, eventMonsters;
    ASSERT_TRUE(defaultZoneInfoRepository().loadMonsterLists(IT_ZONE, monsters, eventMonsters));
    EXPECT_EQ("a:1", monsters);
    EXPECT_EQ("b:2", eventMonsters);

    EXPECT_FALSE(defaultZoneInfoRepository().loadMonsterLists(IT_ZONE_2, monsters, eventMonsters));
}

// --- BloodBibleSignObject against real MySQL ------------------------------

class BloodBibleSignMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        PlayerFixtures::removeAll();
        execSQL("DELETE FROM BloodBibleSignObject WHERE OwnerID = 'itvamphi'");
    }
};

TEST_F(BloodBibleSignMySQL, LoadReturnsItemTypeAscending) {
    PlayerFixture vampire = PlayerFixtures::highLevelVampire();
    vampire.persist();
    // Rows arrive from OUTSIDE the server process (the interface is
    // read-only on purpose) — seeded here the way the web side would.
    execSQL("INSERT INTO BloodBibleSignObject (ItemType, OwnerID) VALUES (5, '" + vampire.name + "'), (2, '" +
            vampire.name + "'), (9, '" + vampire.name + "')");

    std::vector<ItemType_t> itemTypes = defaultBloodBibleSignRepository().loadItemTypes(vampire.name);
    ASSERT_EQ(3u, itemTypes.size());
    EXPECT_EQ(2, itemTypes[0]);
    EXPECT_EQ(5, itemTypes[1]);
    EXPECT_EQ(9, itemTypes[2]);
}

TEST_F(BloodBibleSignMySQL, DuplicateItemTypesSurvive) {
    PlayerFixture vampire = PlayerFixtures::highLevelVampire();
    vampire.persist();
    execSQL("INSERT INTO BloodBibleSignObject (ItemType, OwnerID) VALUES (5, '" + vampire.name + "'), (5, '" +
            vampire.name + "')");

    EXPECT_EQ(2u, defaultBloodBibleSignRepository().loadItemTypes(vampire.name).size());
}

// --- GoodsListObject against real MySQL -----------------------------------

class GoodsMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        PlayerFixtures::removeAll();
        execSQL("DELETE FROM GoodsListObject WHERE PlayerID = 'itaccount'");
    }

    static void seed(const std::string& id, int world, const std::string& name, int num, const std::string& status) {
        char sql[240];
        snprintf(sql, sizeof(sql),
                 "INSERT INTO GoodsListObject (BuyID, ID, World, PlayerID, Name, GoodsID, Num, Status) "
                 "VALUES ('itbuy', %s, %d, 'itaccount', '%s', 5000, %d, '%s')",
                 id.c_str(), world, name.c_str(), num, status.c_str());
        execSQL(sql);
    }
};

TEST_F(GoodsMySQL, PendingIsFilteredByWorldPlayerNameAndStatus) {
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    PlayerFixture ousters = PlayerFixtures::lowLevelOusters();
    slayer.persist();
    ousters.persist();

    seed("9101", 1, slayer.name, 2, "NOT");
    seed("9102", 2, slayer.name, 2, "NOT");  // other world
    seed("9103", 1, ousters.name, 2, "NOT"); // other character
    seed("9104", 1, slayer.name, 2, "GET");  // already taken

    std::vector<GoodsRecord> records = defaultGoodsRepository().loadPending(1, "itaccount", slayer.name);
    ASSERT_EQ(1u, records.size());
    EXPECT_EQ("9101", records[0].id);
    EXPECT_EQ(5000u, records[0].goodsID);
    EXPECT_EQ(2, records[0].num);
}

TEST_F(GoodsMySQL, TakeOneDecrementsAndFlipsStatusOnTheLastUnit) {
    // One UPDATE, left-to-right SET evaluation: the IF() reads the
    // already-decremented Num, so the last unit flips Status to 'GET' in
    // the statement that takes it.
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    slayer.persist();
    seed("9101", 1, slayer.name, 2, "NOT");

    EXPECT_TRUE(defaultGoodsRepository().takeOne("9101"));
    std::vector<GoodsRecord> records = defaultGoodsRepository().loadPending(1, "itaccount", slayer.name);
    ASSERT_EQ(1u, records.size());
    EXPECT_EQ(1, records[0].num);

    EXPECT_TRUE(defaultGoodsRepository().takeOne("9101"));
    EXPECT_TRUE(defaultGoodsRepository().loadPending(1, "itaccount", slayer.name).empty());
    EXPECT_EQ("GET", queryScalar("SELECT Status FROM GoodsListObject WHERE ID=9101"));
    EXPECT_EQ("0", queryScalar("SELECT Num FROM GoodsListObject WHERE ID=9101"));
}

TEST_F(GoodsMySQL, TakingAZeroCountRowFailsAndLeavesItUntouched) {
    // Num - 1 on the UNSIGNED column raises ER_DATA_OUT_OF_RANGE (1690)
    // regardless of strict mode; the row is untouched and the error
    // escapes as an exception (through END_DB, as a DatabaseError) —
    // the stuck-item path documented on MySQLGoodsRepository.
    PlayerFixture slayer = PlayerFixtures::midLevelSlayer();
    slayer.persist();
    seed("9101", 1, slayer.name, 0, "NOT");

    EXPECT_ANY_THROW(defaultGoodsRepository().takeOne("9101"));

    EXPECT_EQ("0", queryScalar("SELECT Num FROM GoodsListObject WHERE ID=9101"));
    EXPECT_EQ("NOT", queryScalar("SELECT Status FROM GoodsListObject WHERE ID=9101"));
}

TEST_F(GoodsMySQL, TakeOneOfAnUnknownIdIsFalse) {
    EXPECT_FALSE(defaultGoodsRepository().takeOne("999999"));
}

// --- NicknameBook against real MySQL (the 3.2 pilot) ----------------------

class NicknameMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        PlayerFixtures::removeAll();
        execSQL("DELETE FROM NicknameBook WHERE OwnerID = 'itoustmid'");
    }
};

TEST_F(NicknameMySQL, LoadReturnsNIDAscendingNotInsertionOrder) {
    // The SELECT has no ORDER BY, but the secondary index IDX_OwnerID
    // carries the primary key (nID, OwnerID) as its suffix, so the ref
    // scan returns nID ascending, not insertion order.
    PlayerFixture ousters = PlayerFixtures::midLevelOusters();
    ousters.persist();

    NicknameRepository& repository = defaultNicknameRepository();
    repository.insert(ousters.name, 10001, NicknameInfo::NICK_CUSTOM, "second-id-first");
    repository.insert(ousters.name, 10000, NicknameInfo::NICK_CUSTOM, "first-id-second");

    std::vector<NicknameRecord> records = repository.load(ousters.name);
    ASSERT_EQ(2u, records.size());
    EXPECT_EQ(10000, records[0].id);
    EXPECT_EQ(10001, records[1].id);
}

// CGSayHandler's forced nickname: REPLACE creates the id-100 row with
// NickIndex 0 and a server-side Time, a second REPLACE overwrites it in
// place (still one row), the other rows of the owner are untouched, and
// the DELETE removes id 100 alone.
TEST_F(NicknameMySQL, ForcedNicknameIsReplacedInPlaceAndDeletedAlone) {
    PlayerFixture ousters = PlayerFixtures::midLevelOusters();
    ousters.persist();
    NicknameRepository& repository = defaultNicknameRepository();
    repository.insert(ousters.name, 7, NicknameInfo::NICK_CUSTOM, "kept");

    repository.replaceForcedNickname(ousters.name, NicknameInfo::NICK_CUSTOM_FORCED, "forced one");
    const std::string forced = " FROM NicknameBook WHERE OwnerID = '" + ousters.name + "' AND nID = 100";
    EXPECT_EQ("forced one", queryScalar("SELECT Nickname" + forced));
    EXPECT_EQ(std::to_string((int)NicknameInfo::NICK_CUSTOM_FORCED), queryScalar("SELECT NickType" + forced));
    EXPECT_EQ("0", queryScalar("SELECT NickIndex" + forced));
    EXPECT_EQ("1", queryScalar("SELECT Time > '2026-01-01'" + forced));

    repository.replaceForcedNickname(ousters.name, NicknameInfo::NICK_CUSTOM_FORCED, "forced two");
    EXPECT_EQ("forced two", queryScalar("SELECT Nickname" + forced));
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*)" + forced));
    EXPECT_EQ("2", queryScalar("SELECT COUNT(*) FROM NicknameBook WHERE OwnerID = '" + ousters.name + "'"));

    repository.deleteForcedNickname(ousters.name);
    EXPECT_EQ("0", queryScalar("SELECT COUNT(*)" + forced));
    EXPECT_EQ("kept",
              queryScalar("SELECT Nickname FROM NicknameBook WHERE OwnerID = '" + ousters.name + "' AND nID = 7"));
}

// --- the race-war cluster against real MySQL -------------------------------
// One repository mixing boot-time reads with runtime writes.
// Every table is seeded; the tests work on rows they insert (ids from
// 31000 up where the column allows it, 250 up for the BYTE-typed sweeper
// bonus Type) and clean them in SetUp/TearDown.

class WarInfoMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        clean();
    }
    virtual void TearDown() {
        clean();
    }
    static void clean() {
        execSQL("DELETE FROM ShrineInfo WHERE ID >= 31000");
        execSQL("DELETE FROM CastleInfo WHERE ServerID >= 31000");
        execSQL("DELETE FROM SweeperBonusInfo WHERE Type >= 250");
        execSQL("DELETE FROM SweeperSetInfo WHERE ID >= 31000");
        execSQL("DELETE FROM SweeperOwnerInfo WHERE SweeperType >= 31000");
        execSQL("DELETE FROM LevelWarHistory WHERE Level >= 31000");
        execSQL("DELETE FROM MasterLairInfo WHERE ZoneID >= 31000");
        execSQL("DELETE FROM GuildWarHistory WHERE WarID >= 31000");
        execSQL("DELETE FROM RaceWarHistory WHERE RaceWarID LIKE 'it-%'");
        execSQL("DELETE FROM RaceWarPCLimit WHERE ID >= 31000");
        execSQL("DELETE FROM ReinforceRegisterInfo WHERE WarID >= 31000");
        execSQL("DELETE FROM WarScheduleInfo WHERE WarID >= 31000");
        execSQL("DELETE FROM RaceWarPCList");
        execSQL("DROP TABLE IF EXISTS RaceWarPCLimitIT");
    }
};

TEST_F(WarInfoMySQL, SeededWholeTableReadsAreNonEmptyAndTheMaxProbeMatchesTheTable) {
    WarInfoRepository& repository = defaultWarInfoRepository();
    EXPECT_FALSE(repository.loadShrines().empty());
    EXPECT_FALSE(repository.loadShrineOwners().empty());
    EXPECT_FALSE(repository.loadSweeperBonuses().empty());
    EXPECT_FALSE(repository.loadMasterLairs().empty());

    int maxType = -1;
    ASSERT_TRUE(repository.loadMaxSweeperBonusType(maxType));
    EXPECT_EQ(atoi(queryScalar("SELECT MAX(Type) FROM SweeperBonusInfo").c_str()), maxType);
}

TEST_F(WarInfoMySQL, ShrineOwnerSaveRewritesOnlyThatShrine) {
    execSQL("INSERT INTO ShrineInfo (ID, Name, OwnerRace) VALUES (31000, 'it-shrine-a', 0)");
    execSQL("INSERT INTO ShrineInfo (ID, Name, OwnerRace) VALUES (31001, 'it-shrine-b', 0)");

    defaultWarInfoRepository().saveShrineOwner(2, 31000);

    EXPECT_EQ("2", queryScalar("SELECT OwnerRace FROM ShrineInfo WHERE ID=31000"));
    EXPECT_EQ("0", queryScalar("SELECT OwnerRace FROM ShrineInfo WHERE ID=31001"));

    std::vector<ShrineOwnerRow> owners = defaultWarInfoRepository().loadShrineOwners();
    bool seen = false;
    for (size_t r = 0; r < owners.size(); r++) {
        if (owners[r].id == 31000) {
            seen = true;
            EXPECT_EQ(2, owners[r].ownerRace);
        }
    }
    EXPECT_TRUE(seen);
}

TEST_F(WarInfoMySQL, CastlesAreScopedToTheServerAndTheSavesKeyOnServerAndZone) {
    execSQL("INSERT INTO CastleInfo (ServerID, ZoneID, ShrineID, Name, GuildID, Race, ItemTaxRatio, EntranceFee, "
            "TaxBalance, BonusOptionType, FirstResurrectZoneID, FirstResurrectX, FirstResurrectY, "
            "SecondResurrectZoneID, SecondResurrectX, SecondResurrectY, ThirdResurrectZoneID, ThirdResurrectX, "
            "ThirdResurrectY, ZoneIDList) VALUES (31000, 31000, 5, 'it-castle', 7, 1, 10, 100, 1000, '1,2', "
            "31000, 3, 4, 31001, 5, 6, 31002, 7, 8, '31000,31001')");
    execSQL("INSERT INTO CastleInfo (ServerID, ZoneID, Name, BonusOptionType, ZoneIDList) "
            "VALUES (31001, 31000, 'other-server', '', '')");

    std::vector<CastleRow> rows = defaultWarInfoRepository().loadCastles(31000);
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(31000, rows[0].zoneID);
    EXPECT_EQ(5, rows[0].shrineID);
    EXPECT_EQ(7, rows[0].guildID);
    EXPECT_EQ("it-castle", rows[0].name);
    EXPECT_EQ(1, rows[0].race);
    EXPECT_EQ(10, rows[0].itemTaxRatio);
    EXPECT_EQ(100, rows[0].entranceFee);
    EXPECT_EQ(1000, rows[0].taxBalance);
    EXPECT_EQ("1,2", rows[0].bonusOptionType);
    EXPECT_EQ(31000, rows[0].firstResurrectZoneID);
    EXPECT_EQ(3, rows[0].firstResurrectX);
    EXPECT_EQ(4, rows[0].firstResurrectY);
    EXPECT_EQ(31001, rows[0].secondResurrectZoneID);
    EXPECT_EQ(5, rows[0].secondResurrectX);
    EXPECT_EQ(6, rows[0].secondResurrectY);
    EXPECT_EQ(31002, rows[0].thirdResurrectZoneID);
    EXPECT_EQ(7, rows[0].thirdResurrectX);
    EXPECT_EQ(8, rows[0].thirdResurrectY);
    EXPECT_EQ("31000,31001", rows[0].zoneIDList);

    // The owner change writes the owner, fee and ratio and moves the balance
    // by its delta rather than overwriting it.
    CastleOwnerRecord record;
    record.guildID = 8;
    record.race = 2;
    record.itemTaxRatio = 20;
    record.entranceFee = 200;
    record.taxBalanceDelta = -1000;
    EXPECT_TRUE(defaultWarInfoRepository().saveCastleOwner(31000, 31000, record));

    EXPECT_EQ("8", queryScalar("SELECT GuildID FROM CastleInfo WHERE ServerID=31000 AND ZoneID=31000"));
    EXPECT_EQ("it-castle", queryScalar("SELECT Name FROM CastleInfo WHERE ServerID=31000 AND ZoneID=31000"));
    EXPECT_EQ("2", queryScalar("SELECT Race FROM CastleInfo WHERE ServerID=31000 AND ZoneID=31000"));
    EXPECT_EQ("20", queryScalar("SELECT ItemTaxRatio FROM CastleInfo WHERE ServerID=31000 AND ZoneID=31000"));
    EXPECT_EQ("200", queryScalar("SELECT EntranceFee FROM CastleInfo WHERE ServerID=31000 AND ZoneID=31000"));
    EXPECT_EQ("0", queryScalar("SELECT TaxBalance FROM CastleInfo WHERE ServerID=31000 AND ZoneID=31000"));
    EXPECT_EQ("other-server", queryScalar("SELECT Name FROM CastleInfo WHERE ServerID=31001 AND ZoneID=31000"));
    // Writing the same owner again with nothing taken changes no row, which
    // is what keeps modifyCastleOwner from announcing it.
    record.taxBalanceDelta = 0;
    EXPECT_FALSE(defaultWarInfoRepository().saveCastleOwner(31000, 31000, record));

    // tinysave applies the caller's SET fragment verbatim and reports
    // whether a row changed.
    EXPECT_TRUE(defaultWarInfoRepository().tinysaveCastle("ItemTaxRatio=1", 31000, 31000));
    EXPECT_EQ("1", queryScalar("SELECT ItemTaxRatio FROM CastleInfo WHERE ServerID=31000 AND ZoneID=31000"));
    EXPECT_FALSE(defaultWarInfoRepository().tinysaveCastle("ItemTaxRatio=1", 31002, 31000)); // no such zone
}

// Two threads' changes to one balance may save in either order. The saves
// are relative, so both orders leave the same row -- the one the balance in
// memory holds -- even when the first to land takes the row below zero.
TEST_F(WarInfoMySQL, TaxBalanceDeltasCommuteAndMayPassThroughANegativeRow) {
    execSQL("INSERT INTO CastleInfo (ServerID, ZoneID, Name, TaxBalance, BonusOptionType, ZoneIDList) "
            "VALUES (31000, 31000, 'it-a', 0, '', ''), (31000, 31001, 'it-b', 0, '', ''), "
            "(31001, 31000, 'other-server', 7, '', '')");
    WarInfoRepository& repository = defaultWarInfoRepository();

    // In memory: a credit of 500, then a withdrawal of 300 drawn on it.
    // Castle A's saves land in that order, castle B's the other way round.
    EXPECT_TRUE(repository.addCastleTaxBalance(31000, 31000, 500));
    EXPECT_TRUE(repository.addCastleTaxBalance(31000, 31000, -300));

    EXPECT_TRUE(repository.addCastleTaxBalance(31000, 31001, -300));
    EXPECT_EQ("-300", queryScalar("SELECT TaxBalance FROM CastleInfo WHERE ServerID=31000 AND ZoneID=31001"));
    EXPECT_TRUE(repository.addCastleTaxBalance(31000, 31001, 500));

    EXPECT_EQ("200", queryScalar("SELECT TaxBalance FROM CastleInfo WHERE ServerID=31000 AND ZoneID=31000"));
    EXPECT_EQ("200", queryScalar("SELECT TaxBalance FROM CastleInfo WHERE ServerID=31000 AND ZoneID=31001"));

    // An owner change that took the 200 and a credit of 50 made just after
    // it, landing in either order, leave the credit.
    CastleOwnerRecord record;
    record.guildID = 9;
    record.race = 1;
    record.itemTaxRatio = 10;
    record.entranceFee = 100;
    record.taxBalanceDelta = -200;

    EXPECT_TRUE(repository.saveCastleOwner(31000, 31000, record));
    EXPECT_TRUE(repository.addCastleTaxBalance(31000, 31000, 50));

    EXPECT_TRUE(repository.addCastleTaxBalance(31000, 31001, 50));
    EXPECT_TRUE(repository.saveCastleOwner(31000, 31001, record));

    EXPECT_EQ("50", queryScalar("SELECT TaxBalance FROM CastleInfo WHERE ServerID=31000 AND ZoneID=31000"));
    EXPECT_EQ("50", queryScalar("SELECT TaxBalance FROM CastleInfo WHERE ServerID=31000 AND ZoneID=31001"));

    // Keyed on server and zone; the load reads the signed value back.
    EXPECT_FALSE(repository.addCastleTaxBalance(31000, 31002, 50)); // no such zone
    EXPECT_EQ("7", queryScalar("SELECT TaxBalance FROM CastleInfo WHERE ServerID=31001 AND ZoneID=31000"));

    EXPECT_TRUE(repository.addCastleTaxBalance(31000, 31000, -80));
    std::vector<CastleRow> rows = repository.loadCastles(31000);
    ASSERT_EQ(2u, rows.size());
    for (size_t r = 0; r < rows.size(); ++r) {
        if (rows[r].zoneID == 31000)
            EXPECT_EQ(-30, rows[r].taxBalance);
        else
            EXPECT_EQ(50, rows[r].taxBalance);
    }
}

TEST_F(WarInfoMySQL, SweeperBonusOwnersAreFilteredByLevelAndSavedByType) {
    // Type is a BYTE at the callers (SweeperBonusType_t), so the rows sit
    // at 250/251; the seed uses 0..11 with levels 0..3.
    execSQL("INSERT INTO SweeperBonusInfo (Type, Name, OptionList, OwnerRace, Level) "
            "VALUES (250, 'it-a', 'ATTR+2', 0, 200)");
    execSQL("INSERT INTO SweeperBonusInfo (Type, Name, OptionList, OwnerRace, Level) "
            "VALUES (251, 'it-b', 'DAM+3', 1, 201)");

    int maxType = 0;
    ASSERT_TRUE(defaultWarInfoRepository().loadMaxSweeperBonusType(maxType));
    EXPECT_EQ(251, maxType);

    std::vector<SweeperBonusRow> all = defaultWarInfoRepository().loadSweeperBonuses();
    bool seen = false;
    for (size_t r = 0; r < all.size(); r++) {
        if (all[r].type == 250) {
            seen = true;
            EXPECT_EQ("it-a", all[r].name);
            EXPECT_EQ("ATTR+2", all[r].optionList);
            EXPECT_EQ(0, all[r].ownerRace);
            EXPECT_EQ(200, all[r].level);
        }
    }
    EXPECT_TRUE(seen);

    std::vector<SweeperBonusOwnerRow> owners = defaultWarInfoRepository().loadSweeperBonusOwners(200);
    ASSERT_EQ(1u, owners.size());
    EXPECT_EQ(250, owners[0].type);
    EXPECT_EQ(0, owners[0].ownerRace);

    defaultWarInfoRepository().saveSweeperBonusOwner((Race_t)2, (SweeperBonusType_t)250);
    EXPECT_EQ("2", queryScalar("SELECT OwnerRace FROM SweeperBonusInfo WHERE Type=250"));
    EXPECT_EQ("1", queryScalar("SELECT OwnerRace FROM SweeperBonusInfo WHERE Type=251"));
}

TEST_F(WarInfoMySQL, SweeperSetsAndOwnersAreScopedToTheZoneAndTheOwnerSaveKeysOnTypeAlone) {
    execSQL("INSERT INTO SweeperSetInfo (ID, Name, ZoneID, ItemType, SlayerX, SlayerY, SlayerMType, VampireX, "
            "VampireY, VampireMType, OustersX, OustersY, OustersMType, DefaultX, DefaultY, DefaultMType) "
            "VALUES (31000, 'it-set', 31000, 3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)");
    execSQL("INSERT INTO SweeperSetInfo (ID, Name, ZoneID, ItemType) VALUES (31001, 'other-zone', 31001, 4)");
    execSQL("INSERT INTO SweeperOwnerInfo (SweeperType, ZoneID, OwnerRace, SweeperSafeType) "
            "VALUES (31000, 31000, 1, 3)");
    execSQL("INSERT INTO SweeperOwnerInfo (SweeperType, ZoneID, OwnerRace, SweeperSafeType) "
            "VALUES (31001, 31001, 0, 4)");

    std::vector<SweeperSetRow> sets = defaultWarInfoRepository().loadSweeperSets(IT_ZONE);
    ASSERT_EQ(1u, sets.size());
    EXPECT_EQ(3, sets[0].itemType);
    EXPECT_EQ(1, sets[0].slayerX);
    EXPECT_EQ(2, sets[0].slayerY);
    EXPECT_EQ(3, sets[0].slayerMonsterType);
    EXPECT_EQ(4, sets[0].vampireX);
    EXPECT_EQ(5, sets[0].vampireY);
    EXPECT_EQ(6, sets[0].vampireMonsterType);
    EXPECT_EQ(7, sets[0].oustersX);
    EXPECT_EQ(8, sets[0].oustersY);
    EXPECT_EQ(9, sets[0].oustersMonsterType);
    EXPECT_EQ(10, sets[0].defaultX);
    EXPECT_EQ(11, sets[0].defaultY);
    EXPECT_EQ(12, sets[0].defaultMonsterType);
    EXPECT_EQ("it-set", sets[0].name);

    std::vector<SweeperOwnerRow> owners = defaultWarInfoRepository().loadSweeperOwners(IT_ZONE);
    ASSERT_EQ(1u, owners.size());
    EXPECT_EQ(31000, owners[0].sweeperType);
    EXPECT_EQ(1, owners[0].ownerRace);
    EXPECT_EQ(3, owners[0].sweeperSafeType);

    std::vector<SweeperBonusOwnerRow> races = defaultWarInfoRepository().loadSweeperOwnerRaces(IT_ZONE);
    ASSERT_EQ(1u, races.size());
    EXPECT_EQ(31000, races[0].type);
    EXPECT_EQ(1, races[0].ownerRace);

    // The UPDATE keys on SweeperType alone (the table's PK), not on ZoneID.
    defaultWarInfoRepository().saveSweeperOwner(2, 5, 31000u);
    EXPECT_EQ("2", queryScalar("SELECT OwnerRace FROM SweeperOwnerInfo WHERE SweeperType=31000"));
    EXPECT_EQ("5", queryScalar("SELECT SweeperSafeType FROM SweeperOwnerInfo WHERE SweeperType=31000"));
    EXPECT_EQ("0", queryScalar("SELECT OwnerRace FROM SweeperOwnerInfo WHERE SweeperType=31001"));
}

TEST_F(WarInfoMySQL, LevelWarHistoryInsertThenUpdateFillsOnlyThatWarsNewColumns) {
    WarInfoRepository& repository = defaultWarInfoRepository();
    repository.insertLevelWarHistory(31000, "it-a", "1|", "2|", "3|", "4|");
    repository.insertLevelWarHistory(31000, "it-b", "", "", "", "");

    EXPECT_EQ("2", queryScalar("SELECT COUNT(*) FROM LevelWarHistory WHERE Level=31000"));
    EXPECT_EQ("1|",
              queryScalar("SELECT SlayerOldSweeper FROM LevelWarHistory WHERE Level=31000 AND LevelWarID='it-a'"));
    EXPECT_EQ("4|",
              queryScalar("SELECT DefaultOldSweeper FROM LevelWarHistory WHERE Level=31000 AND LevelWarID='it-a'"));
    EXPECT_EQ("", queryScalar("SELECT SlayerSweeper FROM LevelWarHistory WHERE Level=31000 AND LevelWarID='it-a'"));

    repository.updateLevelWarHistory("5|", "6|", "7|", "8|", 31000, "it-a");

    EXPECT_EQ("5|", queryScalar("SELECT SlayerSweeper FROM LevelWarHistory WHERE Level=31000 AND LevelWarID='it-a'"));
    EXPECT_EQ("6|", queryScalar("SELECT VampireSweeper FROM LevelWarHistory WHERE Level=31000 AND LevelWarID='it-a'"));
    EXPECT_EQ("7|", queryScalar("SELECT OustersSweeper FROM LevelWarHistory WHERE Level=31000 AND LevelWarID='it-a'"));
    EXPECT_EQ("8|", queryScalar("SELECT DefaultSweeper FROM LevelWarHistory WHERE Level=31000 AND LevelWarID='it-a'"));
    EXPECT_EQ("1|",
              queryScalar("SELECT SlayerOldSweeper FROM LevelWarHistory WHERE Level=31000 AND LevelWarID='it-a'"));
    EXPECT_EQ("", queryScalar("SELECT SlayerSweeper FROM LevelWarHistory WHERE Level=31000 AND LevelWarID='it-b'"));
}


TEST_F(WarInfoMySQL, GuildWarHistoryStartIgnoresARepeatAndTheEndFillsTheWinner) {
    WarInfoRepository& repository = defaultWarInfoRepository();

    repository.insertGuildWarHistory(31000, "it-gw-a", 7, "it-castle", 11, "it-defense", 12, "it-attack");
    // WarID is the PRIMARY KEY and the statement is an INSERT IGNORE, so a
    // second start for the same war is dropped rather than raising.
    repository.insertGuildWarHistory(31000, "it-gw-b", 8, "it-other", 21, "it-other-d", 22, "it-other-a");

    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM GuildWarHistory WHERE WarID=31000"));
    EXPECT_EQ("it-gw-a", queryScalar("SELECT GuildWarID FROM GuildWarHistory WHERE WarID=31000"));
    EXPECT_EQ("it-castle", queryScalar("SELECT CastleName FROM GuildWarHistory WHERE WarID=31000"));
    EXPECT_EQ("11", queryScalar("SELECT DefenseGuildID FROM GuildWarHistory WHERE WarID=31000"));
    EXPECT_EQ("it-attack", queryScalar("SELECT AttackGuildName FROM GuildWarHistory WHERE WarID=31000"));
    EXPECT_EQ("0", queryScalar("SELECT WinnerGuildID FROM GuildWarHistory WHERE WarID=31000"));

    repository.insertGuildWarHistory(31001, "it-gw-c", 7, "it-castle2", 31, "it-d2", 32, "it-a2");
    repository.updateGuildWarWinner(12, "it-attack", 31000);

    EXPECT_EQ("12", queryScalar("SELECT WinnerGuildID FROM GuildWarHistory WHERE WarID=31000"));
    EXPECT_EQ("it-attack", queryScalar("SELECT WinnerGuildName FROM GuildWarHistory WHERE WarID=31000"));
    EXPECT_EQ("0", queryScalar("SELECT WinnerGuildID FROM GuildWarHistory WHERE WarID=31001"));
}

TEST_F(WarInfoMySQL, RaceWarHistoryStartDuplicatesAndTheEndRewritesEveryRowOfThatWarID) {
    WarInfoRepository& repository = defaultWarInfoRepository();

    // RaceWarHistory is keyless and the start is a plain INSERT, so a repeat
    // leaves a second row — where the guild war's INSERT IGNORE drops it.
    repository.insertRaceWarHistory("it-rw-a", 1, 2, 3, "1|", "2|", "3|");
    repository.insertRaceWarHistory("it-rw-a", 4, 5, 6, "4|", "5|", "6|");
    repository.insertRaceWarHistory("it-rw-b", 7, 8, 9, "7|", "8|", "9|");

    EXPECT_EQ("2", queryScalar("SELECT COUNT(*) FROM RaceWarHistory WHERE RaceWarID='it-rw-a'"));
    EXPECT_EQ("1", queryScalar("SELECT SlayerNum FROM RaceWarHistory WHERE RaceWarID='it-rw-a' ORDER BY SlayerNum"));
    EXPECT_EQ("3|", queryScalar("SELECT OustersOldBloodBible FROM RaceWarHistory WHERE RaceWarID='it-rw-a' ORDER BY "
                                "SlayerNum"));

    // The end keys on RaceWarID alone, so both rows of that war take it.
    repository.updateRaceWarBloodBibles("a|", "b|", "c|", "it-rw-a");

    EXPECT_EQ("2", queryScalar("SELECT COUNT(*) FROM RaceWarHistory WHERE RaceWarID='it-rw-a' AND SlayerBloodBible="
                               "'a|' AND VampireBloodBible='b|' AND OustersBloodBible='c|'"));
    EXPECT_EQ("", queryScalar("SELECT SlayerBloodBible FROM RaceWarHistory WHERE RaceWarID='it-rw-b'"));
    // The "Old" columns the start wrote are untouched.
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM RaceWarHistory WHERE RaceWarID='it-rw-a' AND "
                               "SlayerOldBloodBible='1|'"));
}

TEST_F(WarInfoMySQL, RaceWarCurrentNumsAreSummedPerRace) {
    WarInfoRepository& repository = defaultWarInfoRepository();

    execSQL("INSERT INTO RaceWarPCLimit (ID, Race, MinLevel, MaxLevel, LimitNum, CurrentNum) VALUES "
            "(31000, 0, 1, 10, 100, 3)");
    execSQL("INSERT INTO RaceWarPCLimit (ID, Race, MinLevel, MaxLevel, LimitNum, CurrentNum) VALUES "
            "(31001, 0, 11, 20, 100, 4)");
    execSQL("INSERT INTO RaceWarPCLimit (ID, Race, MinLevel, MaxLevel, LimitNum, CurrentNum) VALUES "
            "(31002, 2, 1, 10, 100, 5)");

    std::vector<RaceCurrentNumRow> rows = repository.loadRaceWarCurrentNums();
    // The statement has no WHERE, so seeded rows may join these; find ours by
    // race and check the sums include what we inserted.
    int slayer = 0;
    int ousters = 0;
    for (size_t r = 0; r < rows.size(); r++) {
        if (rows[r].race == 0)
            slayer = rows[r].currentNum;
        else if (rows[r].race == 2)
            ousters = rows[r].currentNum;
    }
    EXPECT_EQ(std::atoi(queryScalar("SELECT SUM(CurrentNum) FROM RaceWarPCLimit WHERE Race=0").c_str()), slayer);
    EXPECT_EQ(std::atoi(queryScalar("SELECT SUM(CurrentNum) FROM RaceWarPCLimit WHERE Race=2").c_str()), ousters);
    EXPECT_GE(slayer, 7);
    EXPECT_GE(ousters, 5);
}

TEST_F(WarInfoMySQL, ReinforceRegistrationsAreScopedToTheWarAndServerThroughTheirWholeLifecycle) {
    WarInfoRepository& repository = defaultWarInfoRepository();
    const WarID_t war = 31000;
    const WarID_t otherWar = 31001;
    const int server = 7;
    const int otherServer = 8;

    GuildID_t none = 77;
    EXPECT_EQ(0, repository.countWaitingReinforceRegistrations(war, server));
    EXPECT_FALSE(repository.loadWaitingReinforceGuild(war, server, none));
    EXPECT_EQ(77, none); // untouched when there is no row: the caller keeps its own default

    repository.insertReinforceRegistration(war, server, 101);
    repository.insertReinforceRegistration(war, server, 102);
    repository.insertReinforceRegistration(war, otherServer, 103);
    repository.insertReinforceRegistration(otherWar, server, 104);

    EXPECT_EQ(2, repository.countWaitingReinforceRegistrations(war, server));
    EXPECT_EQ(1, repository.countWaitingReinforceRegistrations(war, otherServer));
    EXPECT_EQ(1, repository.countWaitingReinforceRegistrations(otherWar, server));

    GuildID_t waiting = 0;
    ASSERT_TRUE(repository.loadWaitingReinforceGuild(war, server, waiting));
    EXPECT_TRUE(waiting == 101 || waiting == 102);

    // Accepting reports whether a row changed, and only that guild's row moves.
    EXPECT_TRUE(repository.acceptReinforceRegistration(war, server, 101));
    EXPECT_FALSE(repository.acceptReinforceRegistration(war, server, 999));
    EXPECT_EQ("ACCEPT", queryScalar("SELECT Status FROM ReinforceRegisterInfo WHERE WarID=31000 AND ServerID=7 AND "
                                    "ReinforceGuildID=101"));
    EXPECT_EQ(1, repository.countWaitingReinforceRegistrations(war, server));

    EXPECT_TRUE(repository.denyReinforceRegistration(war, server, 102));
    EXPECT_FALSE(repository.denyReinforceRegistration(war, server, 999));
    EXPECT_EQ(1, repository.countDeniedReinforceRegistrations(war, server, 102));
    EXPECT_EQ(0, repository.countDeniedReinforceRegistrations(war, server, 101));
    EXPECT_EQ(0, repository.countDeniedReinforceRegistrations(war, otherServer, 102));
    EXPECT_EQ(0, repository.countWaitingReinforceRegistrations(war, server));

    // The DELETE takes the whole (war, server) pair and nothing else.
    repository.deleteReinforceRegistrations(war, server);
    EXPECT_EQ("0", queryScalar("SELECT COUNT(*) FROM ReinforceRegisterInfo WHERE WarID=31000 AND ServerID=7"));
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM ReinforceRegisterInfo WHERE WarID=31000 AND ServerID=8"));
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM ReinforceRegisterInfo WHERE WarID=31001 AND ServerID=7"));
}


TEST_F(WarInfoMySQL, EveryWaitingAndStartedGuildScheduleOfTheZoneComesBackInStartTimeOrder) {
    WarInfoRepository& repository = defaultWarInfoRepository();

    // Three wars in the zone we ask about, deliberately inserted out of
    // StartTime order, plus three the query must not see: another zone,
    // another server, and one already ENDed.
    repository.replaceWarSchedule(31002, 7, 21, "GUILD", 2, 201, 202, 0, 0, 0, 500, "2026-09-03 12:00:00", "WAIT",
                                  "SIEGE");
    repository.replaceWarSchedule(31000, 7, 21, "GUILD", 5, 101, 102, 103, 104, 105, 400, "2026-09-03 10:00:00",
                                  "START", "SIEGE");
    repository.replaceWarSchedule(31001, 7, 21, "GUILD", 1, 301, 0, 0, 0, 0, 600, "2026-09-03 11:00:00", "WAIT",
                                  "GUILD");
    repository.replaceWarSchedule(31003, 7, 22, "GUILD", 1, 401, 0, 0, 0, 0, 700, "2026-09-03 09:00:00", "WAIT",
                                  "SIEGE");
    repository.replaceWarSchedule(31004, 8, 21, "GUILD", 1, 501, 0, 0, 0, 0, 800, "2026-09-03 09:00:00", "WAIT",
                                  "SIEGE");
    repository.replaceWarSchedule(31005, 7, 21, "GUILD", 1, 601, 0, 0, 0, 0, 900, "2026-09-03 09:00:00", "END",
                                  "SIEGE");

    std::vector<WarScheduleRow> rows = repository.loadWarSchedules(7, 21);
    ASSERT_EQ(3u, rows.size());

    // ORDER BY StartTime, so the START row comes first even though it was
    // inserted second.
    EXPECT_EQ(31000, rows[0].warID);
    EXPECT_EQ(31001, rows[1].warID);
    EXPECT_EQ(31002, rows[2].warID);

    // Eleven columns, read positionally in SELECT order.
    EXPECT_EQ("GUILD", rows[0].warType);
    EXPECT_EQ(5, rows[0].attackerCount);
    EXPECT_EQ(101, rows[0].attackGuildID[0]);
    EXPECT_EQ(105, rows[0].attackGuildID[4]);
    EXPECT_EQ(400, rows[0].warFee);
    EXPECT_EQ("2026-09-03 10:00:00", rows[0].startTime);

    EXPECT_EQ(2, rows[2].attackerCount);
    EXPECT_EQ(202, rows[2].attackGuildID[1]);
    EXPECT_EQ(0, rows[2].attackGuildID[4]);

    // Both castle war classes carry WarType 'GUILD'; the kind is what tells
    // the loader which one to rebuild, and it survives the round trip
    // independently of the attacker count -- 31001 is a guild war with the
    // same single attacker a one-guild siege would have.
    EXPECT_EQ("SIEGE", rows[0].castleWarKind);
    EXPECT_EQ("GUILD", rows[1].castleWarKind);
    EXPECT_EQ(1, rows[1].attackerCount);
    EXPECT_EQ(301, rows[1].attackGuildID[0]);
    EXPECT_EQ("SIEGE", rows[2].castleWarKind);

    // The three excluded rows really are in the table.
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM WarScheduleInfo WHERE WarID=31003"));
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM WarScheduleInfo WHERE WarID=31004"));
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM WarScheduleInfo WHERE WarID=31005"));
    EXPECT_TRUE(repository.loadWarSchedules(7, 99).empty());
}

TEST_F(WarInfoMySQL, AGuildWarSavedOverItsRowRewritesWhatItsCreateWroteAndReloadsAsAGuildWar) {
    WarInfoRepository& repository = defaultWarInfoRepository();

    const std::string row = "SELECT CONCAT_WS('|', WarID, ServerID, ZoneID, WarType, AttackerCount, AttackGuildID, "
                            "AttackGuildID2, AttackGuildID3, AttackGuildID4, AttackGuildID5, WarFee, StartTime, "
                            "Status, CastleWarKind) FROM WarScheduleInfo WHERE WarID=31010";

    // WarSchedule::create for a guild war: the one attacker, its fee and the
    // kind, with AttackerCount and the other four slots left to their
    // defaults.
    ASSERT_TRUE(repository.insertWarSchedule(31010, 7, 21, "GUILD", 301, 600, "2026-09-03 11:00:00", "WAIT", "GUILD"));
    const std::string created = queryScalar(row);
    EXPECT_EQ("31010|7|21|GUILD|1|301|0|0|0|0|600|2026-09-03 11:00:00|WAIT|GUILD", created);

    // WarSchedule::save for the same war, through War's attacker virtuals: a
    // war with a single attacker counts one and fills the first slot. The
    // REPLACE writes back exactly the row the INSERT wrote.
    EXPECT_TRUE(repository.replaceWarSchedule(31010, 7, 21, "GUILD", 1, 301, 0, 0, 0, 0, 600, "2026-09-03 11:00:00",
                                              "WAIT", "GUILD"));
    EXPECT_EQ(created, queryScalar(row));

    // What WarScheduler::load reads back: the 'GUILD' kind, which it rebuilds
    // as a GuildWar, with the first slot's guild as its challenger.
    std::vector<WarScheduleRow> rows = repository.loadWarSchedules(7, 21);
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(31010, rows[0].warID);
    EXPECT_EQ("GUILD", rows[0].castleWarKind);
    EXPECT_EQ(1, rows[0].attackerCount);
    EXPECT_EQ(301, rows[0].attackGuildID[0]);
    EXPECT_EQ(0, rows[0].attackGuildID[1]);
    EXPECT_EQ(600, rows[0].warFee);
}

TEST_F(WarInfoMySQL, TheAcceptedReinforceReadIsScopedToTheWarIdAlone) {
    WarInfoRepository& repository = defaultWarInfoRepository();

    int guildID = -1;
    EXPECT_FALSE(repository.loadAcceptedReinforceGuild(31000, guildID));
    EXPECT_EQ(-1, guildID);

    // A WAIT and a DENY registration are not an ACCEPT.
    repository.insertReinforceRegistration(31000, 7, 900);
    EXPECT_FALSE(repository.loadAcceptedReinforceGuild(31000, guildID));
    repository.denyReinforceRegistration(31000, 7, 900);
    EXPECT_FALSE(repository.loadAcceptedReinforceGuild(31000, guildID));

    // This statement carries no ServerID, unlike loadWaitingReinforceGuild:
    // a row registered under a different server still answers.
    repository.insertReinforceRegistration(31000, 8, 901);
    repository.acceptReinforceRegistration(31000, 8, 901);
    EXPECT_TRUE(repository.loadAcceptedReinforceGuild(31000, guildID));
    EXPECT_EQ(901, guildID);

    // Another war's ACCEPT is out of scope.
    guildID = -1;
    EXPECT_FALSE(repository.loadAcceptedReinforceGuild(31001, guildID));
    EXPECT_EQ(-1, guildID);
}

TEST_F(WarInfoMySQL, CancellingGuildSchedulesSpansTheTabsAndSparesEveryOtherRow) {
    WarInfoRepository& repository = defaultWarInfoRepository();

    repository.replaceWarSchedule(31000, 7, 21, "GUILD", 1, 101, 0, 0, 0, 0, 400, "2026-09-03 10:00:00", "WAIT",
                                  "SIEGE");
    repository.replaceWarSchedule(31001, 7, 21, "GUILD", 1, 102, 0, 0, 0, 0, 400, "2026-09-03 11:00:00", "START",
                                  "SIEGE");
    repository.replaceWarSchedule(31002, 7, 21, "GUILD", 1, 103, 0, 0, 0, 0, 400, "2026-09-03 12:00:00", "END",
                                  "SIEGE");
    repository.replaceWarSchedule(31003, 7, 21, "RACE", 1, 104, 0, 0, 0, 0, 400, "2026-09-03 13:00:00", "WAIT",
                                  "SIEGE");
    repository.replaceWarSchedule(31004, 7, 22, "GUILD", 1, 105, 0, 0, 0, 0, 400, "2026-09-03 14:00:00", "WAIT",
                                  "SIEGE");

    // The literal carries four tabs spliced in by a backslash-continued
    // source line; MySQL treats them as whitespace, so the statement runs
    // and the WarType clause after them still applies.
    repository.cancelGuildWarSchedules(7, 21);

    EXPECT_EQ("CANCEL", queryScalar("SELECT Status FROM WarScheduleInfo WHERE WarID=31000"));
    EXPECT_EQ("CANCEL", queryScalar("SELECT Status FROM WarScheduleInfo WHERE WarID=31001"));
    EXPECT_EQ("END", queryScalar("SELECT Status FROM WarScheduleInfo WHERE WarID=31002"));
    EXPECT_EQ("WAIT", queryScalar("SELECT Status FROM WarScheduleInfo WHERE WarID=31003"));
    EXPECT_EQ("WAIT", queryScalar("SELECT Status FROM WarScheduleInfo WHERE WarID=31004"));

    // The cancelled wars drop out of the load. The RACE row survives both
    // statements: the cancel filters on WarType, the load does not — its
    // caller skips RACE rows itself.
    std::vector<WarScheduleRow> rows = repository.loadWarSchedules(7, 21);
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(31003, rows[0].warID);
    EXPECT_EQ("RACE", rows[0].warType);
}

TEST_F(WarInfoMySQL, CancellingOneWaitingWarLeavesEveryOtherRowAndEveryStartedWarAlone) {
    WarInfoRepository& repository = defaultWarInfoRepository();

    repository.replaceWarSchedule(31000, 7, 21, "GUILD", 1, 101, 0, 0, 0, 0, 400, "2026-09-03 10:00:00", "WAIT",
                                  "SIEGE");
    repository.replaceWarSchedule(31001, 7, 21, "GUILD", 1, 102, 0, 0, 0, 0, 400, "2026-09-03 11:00:00", "WAIT",
                                  "GUILD");
    repository.replaceWarSchedule(31002, 7, 21, "GUILD", 1, 103, 0, 0, 0, 0, 400, "2026-09-03 12:00:00", "START",
                                  "SIEGE");
    repository.replaceWarSchedule(31003, 8, 21, "GUILD", 1, 104, 0, 0, 0, 0, 400, "2026-09-03 13:00:00", "WAIT",
                                  "SIEGE");

    // One war by id, and the neighbours of that castle are untouched -- what
    // separates this from cancelGuildWarSchedules, which takes them all.
    EXPECT_TRUE(repository.cancelWarSchedule(31000, 7));
    EXPECT_EQ("CANCEL", queryScalar("SELECT Status FROM WarScheduleInfo WHERE WarID=31000"));
    EXPECT_EQ("WAIT", queryScalar("SELECT Status FROM WarScheduleInfo WHERE WarID=31001"));

    // Already cancelled, so nothing changes and the caller is told so.
    EXPECT_FALSE(repository.cancelWarSchedule(31000, 7));

    // A war under way belongs to the zone thread running it.
    EXPECT_FALSE(repository.cancelWarSchedule(31002, 7));
    EXPECT_EQ("START", queryScalar("SELECT Status FROM WarScheduleInfo WHERE WarID=31002"));

    // The server id is part of the key: the same war id under another server
    // is a different row.
    EXPECT_FALSE(repository.cancelWarSchedule(31003, 7));
    EXPECT_EQ("WAIT", queryScalar("SELECT Status FROM WarScheduleInfo WHERE WarID=31003"));
    EXPECT_TRUE(repository.cancelWarSchedule(31003, 8));
    EXPECT_EQ("CANCEL", queryScalar("SELECT Status FROM WarScheduleInfo WHERE WarID=31003"));

    // The cancelled war drops out of the load; the started one does not.
    std::vector<WarScheduleRow> rows = repository.loadWarSchedules(7, 21);
    ASSERT_EQ(2u, rows.size());
    EXPECT_EQ(31001, rows[0].warID);
    EXPECT_EQ(31002, rows[1].warID);
}

TEST_F(WarInfoMySQL, RaceWarLimitStatementsRunAgainstTheTableNameTheCallerHandsIn) {
    WarInfoRepository& repository = defaultWarInfoRepository();

    // A copy of the real table, so the three statements can be exercised —
    // clearRaceWarCurrentNums covers every row of its table — without
    // rewriting the seeded rows the other tests read.
    execSQL("DROP TABLE IF EXISTS RaceWarPCLimitIT");
    execSQL("CREATE TABLE RaceWarPCLimitIT LIKE RaceWarPCLimit");
    execSQL("INSERT INTO RaceWarPCLimitIT (ID, Race, MinLevel, MaxLevel, LimitNum, CurrentNum) VALUES "
            "(31010, 1, 10, 20, 100, 5)");
    execSQL("INSERT INTO RaceWarPCLimitIT (ID, Race, MinLevel, MaxLevel, LimitNum, CurrentNum) VALUES "
            "(31011, 1, 21, 30, 200, 6)");
    execSQL("INSERT INTO RaceWarPCLimitIT (ID, Race, MinLevel, MaxLevel, LimitNum, CurrentNum) VALUES "
            "(31012, 2, 10, 20, 300, 7)");

    // The %s really is the table name — the load reads the copy, not the
    // table the same statement hits when the caller's getTableName()
    // answers "RaceWarPCLimit".
    std::vector<RaceWarLimitRow> rows = repository.loadRaceWarLimits("RaceWarPCLimitIT", 1);
    ASSERT_EQ(2u, rows.size());

    // Five columns in SELECT order, every one through getInt. The SELECT
    // carries no ORDER BY, so indexing the rows leans on the scan order of
    // a keyless InnoDB copy being the insertion order — the same
    // dependence SkillSaveMySQL's load-order test names.
    EXPECT_EQ(31010, rows[0].id);
    EXPECT_EQ(10, rows[0].minLevel);
    EXPECT_EQ(20, rows[0].maxLevel);
    EXPECT_EQ(100, rows[0].limitNum);
    EXPECT_EQ(5, rows[0].currentNum);
    EXPECT_EQ(31011, rows[1].id);
    EXPECT_EQ(6, rows[1].currentNum);

    // The Race=2 row is out of scope of that read but not of the writes.
    EXPECT_EQ(1u, repository.loadRaceWarLimits("RaceWarPCLimitIT", 2).size());

    // saveCurrent is keyed on the row's own ID, so it lands on exactly one.
    repository.saveRaceWarCurrentNum("RaceWarPCLimitIT", 42, 31010);
    EXPECT_EQ("42", queryScalar("SELECT CurrentNum FROM RaceWarPCLimitIT WHERE ID=31010"));
    EXPECT_EQ("6", queryScalar("SELECT CurrentNum FROM RaceWarPCLimitIT WHERE ID=31011"));
    EXPECT_EQ("7", queryScalar("SELECT CurrentNum FROM RaceWarPCLimitIT WHERE ID=31012"));

    // clearCurrent carries no WHERE at all: every row of the table goes to
    // zero, the other races' included.
    repository.clearRaceWarCurrentNums("RaceWarPCLimitIT");
    EXPECT_EQ("0", queryScalar("SELECT SUM(CurrentNum) FROM RaceWarPCLimitIT"));

    execSQL("DROP TABLE RaceWarPCLimitIT");
}

TEST_F(WarInfoMySQL, ParticipantEntriesAreInsertIgnoredCountedAndDeletedByName) {
    WarInfoRepository& repository = defaultWarInfoRepository();

    EXPECT_EQ(0, repository.countRaceWarPCListEntries("itracewar1"));

    repository.insertRaceWarPCListEntry("itracewar1", 1);
    EXPECT_EQ(1, repository.countRaceWarPCListEntries("itracewar1"));
    EXPECT_EQ("1", queryScalar("SELECT Race FROM RaceWarPCList WHERE Name='itracewar1'"));

    // Name is the PRIMARY KEY and the write is an INSERT IGNORE, so
    // re-joining is dropped rather than failing — and the stored race does
    // not change.
    repository.insertRaceWarPCListEntry("itracewar1", 2);
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM RaceWarPCList WHERE Name='itracewar1'"));
    EXPECT_EQ("1", queryScalar("SELECT Race FROM RaceWarPCList WHERE Name='itracewar1'"));

    repository.insertRaceWarPCListEntry("itracewar2", 2);
    repository.deleteRaceWarPCListEntry("itracewar1");
    EXPECT_EQ(0, repository.countRaceWarPCListEntries("itracewar1"));
    EXPECT_EQ(1, repository.countRaceWarPCListEntries("itracewar2"));
}

TEST_F(WarInfoMySQL, TheParticipantListReadTakesItsRaceFromTheNameColumn) {
    WarInfoRepository& repository = defaultWarInfoRepository();

    execSQL("DELETE FROM RaceWarPCList");
    execSQL("INSERT INTO RaceWarPCList (Name, Race) VALUES ('itrw1', 1)");
    execSQL("INSERT INTO RaceWarPCList (Name, Race) VALUES ('7abc', 2)");

    std::vector<RaceWarPCListRow> rows = repository.loadRaceWarPCList();
    ASSERT_EQ(2u, rows.size());

    int named = -1;
    int numeric = -1;
    for (size_t r = 0; r < rows.size(); r++) {
        if (rows[r].name == "itrw1")
            named = rows[r].race;
        else if (rows[r].name == "7abc")
            numeric = rows[r].race;
    }

    // The Race column holds 1 and 2. The loader reports 0 and 7, because it
    // hands getInt column 1 — Name — and getInt is atoi. The caller then
    // uses that value to index a
    // three-element array: "itrw1" parses to 0 and merely lands in the
    // wrong bucket, while "7abc" parses to 7 and writes outside the array.
    // Pinned, not fixed: correcting it is a behaviour change of its own.
    EXPECT_EQ("1", queryScalar("SELECT Race FROM RaceWarPCList WHERE Name='itrw1'"));
    EXPECT_EQ("2", queryScalar("SELECT Race FROM RaceWarPCList WHERE Name='7abc'"));
    EXPECT_EQ(0, named);
    EXPECT_EQ(7, numeric);

    // The companion statement empties the table outright.
    repository.deleteRaceWarPCList();
    EXPECT_EQ("0", queryScalar("SELECT COUNT(*) FROM RaceWarPCList"));
    EXPECT_TRUE(repository.loadRaceWarPCList().empty());
}

TEST_F(WarInfoMySQL, WarScheduleWritesReportWhetherARowChangedAndTheProbesSeeThem) {
    WarInfoRepository& repository = defaultWarInfoRepository();

    const int before = repository.countWarSchedules();

    // The create path is an INSERT IGNORE against a table whose PRIMARY KEY is
    // WarID, so the first write lands and the second is dropped — and the
    // caller learns which from the affected-row count.
    EXPECT_TRUE(repository.insertWarSchedule(31000, 7, 21, "GUILD", 101, 500, "2026-09-03 10:00:00", "WAIT", "GUILD"));
    EXPECT_FALSE(
        repository.insertWarSchedule(31000, 8, 22, "GUILD", 102, 600, "2026-09-03 11:00:00", "START", "SIEGE"));

    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM WarScheduleInfo WHERE WarID=31000"));
    EXPECT_EQ("7", queryScalar("SELECT ServerID FROM WarScheduleInfo WHERE WarID=31000"));
    EXPECT_EQ("21", queryScalar("SELECT ZoneID FROM WarScheduleInfo WHERE WarID=31000"));
    EXPECT_EQ("GUILD", queryScalar("SELECT WarType FROM WarScheduleInfo WHERE WarID=31000"));
    EXPECT_EQ("101", queryScalar("SELECT AttackGuildID FROM WarScheduleInfo WHERE WarID=31000"));
    EXPECT_EQ("500", queryScalar("SELECT WarFee FROM WarScheduleInfo WHERE WarID=31000"));
    EXPECT_EQ("2026-09-03 10:00:00", queryScalar("SELECT StartTime FROM WarScheduleInfo WHERE WarID=31000"));
    EXPECT_EQ("WAIT", queryScalar("SELECT Status FROM WarScheduleInfo WHERE WarID=31000"));
    // The INSERT names no AttackerCount, so the column keeps its default of 1.
    EXPECT_EQ("1", queryScalar("SELECT AttackerCount FROM WarScheduleInfo WHERE WarID=31000"));
    // The kind the war was registered as, which is all that tells a
    // single-challenger siege from a guild war on a reload.
    EXPECT_EQ("GUILD", queryScalar("SELECT CastleWarKind FROM WarScheduleInfo WHERE WarID=31000"));

    // The save path is a REPLACE, so it overwrites the same key and fills the
    // five challenger columns.
    EXPECT_TRUE(repository.replaceWarSchedule(31000, 9, 23, "GUILD", 3, 201, 202, 203, 0, 0, 700, "2026-09-03 12:00:00",
                                              "START", "SIEGE"));
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM WarScheduleInfo WHERE WarID=31000"));
    EXPECT_EQ("9", queryScalar("SELECT ServerID FROM WarScheduleInfo WHERE WarID=31000"));
    EXPECT_EQ("3", queryScalar("SELECT AttackerCount FROM WarScheduleInfo WHERE WarID=31000"));
    EXPECT_EQ("203", queryScalar("SELECT AttackGuildID3 FROM WarScheduleInfo WHERE WarID=31000"));
    EXPECT_EQ("START", queryScalar("SELECT Status FROM WarScheduleInfo WHERE WarID=31000"));
    // The REPLACE rewrites the kind too, so a war cannot keep a stale one.
    EXPECT_EQ("SIEGE", queryScalar("SELECT CastleWarKind FROM WarScheduleInfo WHERE WarID=31000"));

    // The probes count and maximise over the whole table.
    EXPECT_EQ(before + 1, repository.countWarSchedules());
    EXPECT_GE(repository.loadMaxWarID(), 31000u);

    repository.insertWarSchedule(31001, 9, 23, "GUILD", 301, 800, "2026-09-03 13:00:00", "WAIT", "SIEGE");
    EXPECT_EQ(before + 2, repository.countWarSchedules());
    EXPECT_GE(repository.loadMaxWarID(), 31001u);
}

TEST_F(WarInfoMySQL, WarScheduleTinysaveAppliesTheFragmentToOneWarAndServer) {
    WarInfoRepository& repository = defaultWarInfoRepository();

    repository.insertWarSchedule(31000, 7, 21, "GUILD", 101, 500, "2026-09-03 10:00:00", "WAIT", "SIEGE");
    repository.insertWarSchedule(31001, 8, 21, "GUILD", 102, 500, "2026-09-03 10:00:00", "WAIT", "SIEGE");

    repository.tinysaveWarSchedule("Status='END'", 31000, 7);

    EXPECT_EQ("END", queryScalar("SELECT Status FROM WarScheduleInfo WHERE WarID=31000"));
    EXPECT_EQ("WAIT", queryScalar("SELECT Status FROM WarScheduleInfo WHERE WarID=31001"));

    // A fragment can set several columns, and a mismatched server id is a
    // silent no-op — the statement carries no affected-row check.
    repository.tinysaveWarSchedule("Status='CANCEL', WarFee=999", 31000, 7);
    EXPECT_EQ("CANCEL", queryScalar("SELECT Status FROM WarScheduleInfo WHERE WarID=31000"));
    EXPECT_EQ("999", queryScalar("SELECT WarFee FROM WarScheduleInfo WHERE WarID=31000"));

    repository.tinysaveWarSchedule("WarFee=1", 31000, 99);
    EXPECT_EQ("999", queryScalar("SELECT WarFee FROM WarScheduleInfo WHERE WarID=31000"));
}

TEST_F(WarInfoMySQL, MasterLairRowsCarryAllTwentyFiveColumns) {
    execSQL("INSERT INTO MasterLairInfo (ZoneID, MasterNotReadyMonsterType, MasterMonsterType, MasterRemainNotReady, "
            "MasterX, MasterY, MasterDir, MaxPassPlayer, SummonX, SummonY, FirstRegenDelay, RegenDelay, StartDelay, "
            "EndDelay, KickOutDelay, KickZoneID, KickZoneX, KickZoneY, LairAttackTick, LairAttackMinNumber, "
            "LairAttackMaxNumber, MasterSummonSay, MasterDeadSlayerSay, MasterDeadVampireSay, MasterNotDeadSay) "
            "VALUES (31000, 1, 2, 1, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, "
            "'\"s\"', '\"ds\"', '\"dv\"', '\"nd\"')");

    std::vector<MasterLairRow> rows = defaultWarInfoRepository().loadMasterLairs();
    const MasterLairRow* mine = NULL;
    for (size_t r = 0; r < rows.size(); r++)
        if (rows[r].zoneID == 31000)
            mine = &rows[r];
    ASSERT_TRUE(mine != NULL);
    EXPECT_EQ(1, mine->masterNotReadyMonsterType);
    EXPECT_EQ(2, mine->masterMonsterType);
    EXPECT_EQ(1, mine->masterRemainNotReady);
    EXPECT_EQ(4, mine->masterX);
    EXPECT_EQ(5, mine->masterY);
    EXPECT_EQ(6, mine->masterDir);
    EXPECT_EQ(7, mine->maxPassPlayer);
    EXPECT_EQ(8, mine->summonX);
    EXPECT_EQ(9, mine->summonY);
    EXPECT_EQ(10, mine->firstRegenDelay);
    EXPECT_EQ(11, mine->regenDelay);
    EXPECT_EQ(12, mine->startDelay);
    EXPECT_EQ(13, mine->endDelay);
    EXPECT_EQ(14, mine->kickOutDelay);
    EXPECT_EQ(15, mine->kickZoneID);
    EXPECT_EQ(16, mine->kickZoneX);
    EXPECT_EQ(17, mine->kickZoneY);
    EXPECT_EQ(18, mine->lairAttackTick);
    EXPECT_EQ(19, mine->lairAttackMinNumber);
    EXPECT_EQ(20, mine->lairAttackMaxNumber);
    EXPECT_EQ("\"s\"", mine->masterSummonSay);
    EXPECT_EQ("\"ds\"", mine->masterDeadSlayerSay);
    EXPECT_EQ("\"dv\"", mine->masterDeadVampireSay);
    EXPECT_EQ("\"nd\"", mine->masterNotDeadSay);
}

// --- the item bookkeeping cluster against real MySQL --------------------
// ItemRepository plus four option-table loads on GameInfoRepository. The
// tests work on rows they insert (ids from 31000 up; ItemClass 250 for
// UniqueItemInfo's tinyint key; 'it-' owners) and clean them in
// SetUp/TearDown. PotionObject stands in for the per-class item-object
// tables whose NAME the repository takes as data.

class ItemMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        clean();
    }
    virtual void TearDown() {
        clean();
    }
    static void clean() {
        execSQL("DELETE FROM ItemTraceLog WHERE OwnerID LIKE 'it-%'");
        execSQL("DELETE FROM OpCreate WHERE OpName LIKE 'it-%'");
        execSQL("DELETE FROM MoneyTraceLog WHERE OwnerID LIKE 'it-%'");
        execSQL("DELETE FROM EventQuestRewardSchedule WHERE RewardID >= 31000");
        execSQL("DELETE FROM EventQuestRewardRecord WHERE PlayerID LIKE 'it-%'");
        execSQL("DELETE FROM EventStarObject WHERE ItemID >= 31000");
        execSQL("DELETE FROM UniqueItemInfo WHERE ItemClass >= 250");
        execSQL("DELETE FROM TimeLimitItems WHERE OwnerID LIKE 'it-%'");
        execSQL("DELETE FROM CardCount WHERE CARDKIND >= 31000");
        execSQL("DELETE FROM LuckyBagCount WHERE BAGKIND >= 31000");
        execSQL("DELETE FROM GiftBoxCount WHERE BOXKIND >= 31000");
        execSQL("DELETE FROM EventItemCount WHERE ItemClass >= 31000");
        execSQL("DELETE FROM ResurrectItemCount WHERE Count >= 31000");
        execSQL("DELETE FROM PotionObject WHERE ItemID >= 31000");
    }
};

// CGSayHandler's opcreate log: the three texts in their columns (DateTime
// is a varchar the caller formats, not a server-side time).
TEST_F(ItemMySQL, OpCreateLogRowCarriesTheThreeTexts) {
    defaultItemRepository().insertOpCreateLog("it-gm", "2026-09-06 10:00:00", "Sword(31000)");
    const std::string where = " FROM OpCreate WHERE OpName = 'it-gm'";
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*)" + where));
    EXPECT_EQ("2026-09-06 10:00:00", queryScalar("SELECT DateTime" + where));
    EXPECT_EQ("Sword(31000)", queryScalar("SELECT ItemDesc" + where));
}

TEST_F(ItemMySQL, TraceLogsAreInsertedWithTheirEnumTextsAndAServerSideTime) {
    ItemTraceRecord record;
    record.itemID = 31000;
    record.itemClass = "it-class";
    record.itemType = 7;
    record.optionName = "1,2";
    record.preOwner = "it-pre";
    record.owner = "it-own";
    record.logType = "TRADE";
    record.detailType = "PICKUP";
    defaultItemRepository().insertItemTraceLog(record);

    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM ItemTraceLog WHERE ItemID=31000 AND OwnerID='it-own'"));
    EXPECT_EQ("it-class", queryScalar("SELECT ItemClass FROM ItemTraceLog WHERE ItemID=31000"));
    EXPECT_EQ("7", queryScalar("SELECT ItemType FROM ItemTraceLog WHERE ItemID=31000"));
    EXPECT_EQ("1,2", queryScalar("SELECT OptionType FROM ItemTraceLog WHERE ItemID=31000"));
    EXPECT_EQ("it-pre", queryScalar("SELECT PreOwnerID FROM ItemTraceLog WHERE ItemID=31000"));
    EXPECT_EQ("TRADE", queryScalar("SELECT LogType FROM ItemTraceLog WHERE ItemID=31000"));
    EXPECT_EQ("PICKUP", queryScalar("SELECT DetailType FROM ItemTraceLog WHERE ItemID=31000"));
    EXPECT_NE("0000-00-00 00:00:00", queryScalar("SELECT Time FROM ItemTraceLog WHERE ItemID=31000"));

    defaultItemRepository().insertMoneyTraceLog("it-pre", "it-own", "TRADE", "DROP", 12345);

    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM MoneyTraceLog WHERE OwnerID='it-own'"));
    EXPECT_EQ("it-pre", queryScalar("SELECT PreOwnerID FROM MoneyTraceLog WHERE OwnerID='it-own'"));
    EXPECT_EQ("TRADE", queryScalar("SELECT LogType FROM MoneyTraceLog WHERE OwnerID='it-own'"));
    EXPECT_EQ("DROP", queryScalar("SELECT DetailType FROM MoneyTraceLog WHERE OwnerID='it-own'"));
    EXPECT_EQ("12345", queryScalar("SELECT Amount FROM MoneyTraceLog WHERE OwnerID='it-own'"));
    EXPECT_NE("0000-00-00 00:00:00", queryScalar("SELECT Time FROM MoneyTraceLog WHERE OwnerID='it-own'"));
}

TEST_F(ItemMySQL, UniqueItemNumbersAreReadAndCountedPerClassAndType) {
    execSQL("INSERT INTO UniqueItemInfo (ItemClass, ItemType, LimitNumber, CurrentNumber, ItemClassName) "
            "VALUES (250, 1, 3, 2, 'it-a')");
    execSQL("INSERT INTO UniqueItemInfo (ItemClass, ItemType, LimitNumber, CurrentNumber, ItemClassName) "
            "VALUES (250, 2, 0, 9, 'it-b')");
    execSQL("INSERT INTO UniqueItemInfo (ItemClass, ItemType, LimitNumber, CurrentNumber, ItemClassName) "
            "VALUES (250, 3, 1, 0, 'it-c')");

    ItemRepository& repository = defaultItemRepository();

    std::vector<UniqueItemRow> rows = repository.loadUniqueItems();
    int seen = 0;
    for (size_t r = 0; r < rows.size(); r++)
        if (rows[r].itemClass == 250)
            seen++;
    EXPECT_EQ(3, seen);

    int limit = -1, current = -1;
    ASSERT_TRUE(repository.loadUniqueItemNumbers(250, 1, limit, current));
    EXPECT_EQ(3, limit);
    EXPECT_EQ(2, current);
    EXPECT_FALSE(repository.loadUniqueItemNumbers(250, 4, limit, current)); // no such row

    repository.incrementUniqueItemCount(250, 1);
    EXPECT_EQ("3", queryScalar("SELECT CurrentNumber FROM UniqueItemInfo WHERE ItemClass=250 AND ItemType=1"));
    repository.decrementUniqueItemCount(250, 1);
    repository.decrementUniqueItemCount(250, 1);
    EXPECT_EQ("1", queryScalar("SELECT CurrentNumber FROM UniqueItemInfo WHERE ItemClass=250 AND ItemType=1"));
    EXPECT_EQ("9", queryScalar("SELECT CurrentNumber FROM UniqueItemInfo WHERE ItemClass=250 AND ItemType=2"));

    // CurrentNumber is UNSIGNED: a decrement at 0 is an out-of-range error
    // (ER 1690), not a clamp — the statement fails, the row is untouched,
    // and the failure surfaces as END_DB's rethrow. Pinned as observed.
    EXPECT_ANY_THROW(repository.decrementUniqueItemCount(250, 3));
    EXPECT_EQ("0", queryScalar("SELECT CurrentNumber FROM UniqueItemInfo WHERE ItemClass=250 AND ItemType=3"));
}

TEST_F(ItemMySQL, TimeLimitItemsAreLoadedByOwnerAndStatusAndUpdatedByOwnerClassAndId) {
    ItemRepository& repository = defaultItemRepository();
    repository.insertTimeLimitItem("it-owner", 5, 31000u, "2030-01-02 03:04:05");
    repository.insertTimeLimitItem("it-owner", 6, 31001u, "2031-01-02 03:04:05");
    repository.insertTimeLimitItem("it-other", 5, 31002u, "2032-01-02 03:04:05");

    std::vector<TimeLimitItemRow> rows = repository.loadTimeLimitItems("it-owner", 0);
    ASSERT_EQ(2u, rows.size());
    bool seen31000 = false;
    for (size_t r = 0; r < rows.size(); r++) {
        if (rows[r].itemID == 31000) {
            seen31000 = true;
            EXPECT_EQ(5, rows[r].itemClass);
            EXPECT_EQ("2030-01-02 03:04:05", rows[r].limitDateTime);
        }
    }
    EXPECT_TRUE(seen31000);

    EXPECT_TRUE(repository.updateTimeLimitItemStatus(1, "it-owner", 5, 31000u));
    EXPECT_FALSE(repository.updateTimeLimitItemStatus(1, "it-owner", 5, 31009u)); // no such item
    EXPECT_FALSE(repository.updateTimeLimitItemStatus(1, "it-owner", 6, 31000u)); // wrong class

    EXPECT_EQ("1", queryScalar("SELECT Status FROM TimeLimitItems WHERE OwnerID='it-owner' AND ItemID=31000"));
    EXPECT_EQ("0", queryScalar("SELECT Status FROM TimeLimitItems WHERE OwnerID='it-owner' AND ItemID=31001"));

    rows = repository.loadTimeLimitItems("it-owner", 0);
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(31001, rows[0].itemID);
    EXPECT_EQ(1u, repository.loadTimeLimitItems("it-other", 0).size());
}

// quest/ActionGiveCommonEventItem's counter: (Race, ItemIndex)-keyed, the
// race a BYTE. Race 250 is outside the game's three races; its rows are
// this test's and are removed by value.
TEST_F(ItemMySQL, EventItemCount2IncrementsOnlyItsRaceAndIndex) {
    execSQL("DELETE FROM EventItemCount2 WHERE Race = 250");
    execSQL("INSERT INTO EventItemCount2 (Race, ItemIndex, Count) VALUES (250, 1, 5)");
    execSQL("INSERT INTO EventItemCount2 (Race, ItemIndex, Count) VALUES (250, 2, 5)");

    defaultItemRepository().incrementEventItemCount2((Race_t)250, 1);

    EXPECT_EQ("6", queryScalar("SELECT Count FROM EventItemCount2 WHERE Race=250 AND ItemIndex=1"));
    EXPECT_EQ("5", queryScalar("SELECT Count FROM EventItemCount2 WHERE Race=250 AND ItemIndex=2"));
    execSQL("DELETE FROM EventItemCount2 WHERE Race = 250");
}

// CGLotterySelectHandler: the win record — the character name in PlayerID,
// the account id in RealPlayerID, the DWORD reward id, a server-side Time.
TEST_F(ItemMySQL, EventQuestRewardRecordIsInsertedWithNameAndAccount) {
    defaultItemRepository().insertEventQuestRewardRecord("it-char", 31000, "it-acct");
    const std::string where = " FROM EventQuestRewardRecord WHERE PlayerID = 'it-char'";
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*)" + where));
    EXPECT_EQ("31000", queryScalar("SELECT RewardID" + where));
    EXPECT_EQ("it-acct", queryScalar("SELECT RealPlayerID" + where));
    EXPECT_EQ("1", queryScalar("SELECT Time > '2026-01-01'" + where));
}

// CGDissectionCorpseHandler's black-star cap: the sum of Num over the
// ItemType 0 rows of EventStarObject, other types left out. initdb/ leaves
// the table empty, but the ItemObject round-trip tests write to it too,
// so the test measures against what stands before it adds rows.
TEST_F(ItemMySQL, BlackStarCountSumsTheTypeZeroRowsOnly) {
    int before = -1;
    ASSERT_TRUE(defaultItemRepository().loadBlackStarCount(before));
    EXPECT_EQ(atoi(queryScalar("SELECT ifnull(sum(Num),0) FROM EventStarObject WHERE ItemType=0").c_str()), before);

    execSQL("INSERT INTO EventStarObject (ItemID, ObjectID, ItemType, OwnerID, Storage, StorageID, X, Y, Num, "
            "ItemFlag) VALUES (31000, 1, 0, 'it-owner', 0, 0, 0, 0, 3, 0)");
    execSQL("INSERT INTO EventStarObject (ItemID, ObjectID, ItemType, OwnerID, Storage, StorageID, X, Y, Num, "
            "ItemFlag) VALUES (31001, 2, 0, 'it-owner', 0, 0, 0, 0, 4, 0)");
    execSQL("INSERT INTO EventStarObject (ItemID, ObjectID, ItemType, OwnerID, Storage, StorageID, X, Y, Num, "
            "ItemFlag) VALUES (31002, 3, 1, 'it-owner', 0, 0, 0, 0, 9, 0)"); // other type

    int after = -1;
    ASSERT_TRUE(defaultItemRepository().loadBlackStarCount(after));
    EXPECT_EQ(before + 7, after);
}

TEST_F(ItemMySQL, EventCountersIncrementOnlyTheirRowExceptTheKeylessResurrectCount) {
    execSQL("INSERT INTO CardCount (CARDKIND, CARDCOUNT) VALUES (31000, 5)");
    execSQL("INSERT INTO CardCount (CARDKIND, CARDCOUNT) VALUES (31001, 5)");
    execSQL("INSERT INTO LuckyBagCount (BAGKIND, BAGCOUNT) VALUES (31000, 5)");
    execSQL("INSERT INTO LuckyBagCount (BAGKIND, BAGCOUNT) VALUES (31001, 5)");
    execSQL("INSERT INTO GiftBoxCount (BOXKIND, BOXCOUNT) VALUES (31000, 5)");
    execSQL("INSERT INTO GiftBoxCount (BOXKIND, BOXCOUNT) VALUES (31001, 5)");
    execSQL("INSERT INTO EventItemCount (ItemClass, ItemType, Count) VALUES (31000, 1, 5)");
    execSQL("INSERT INTO EventItemCount (ItemClass, ItemType, Count) VALUES (31000, 2, 5)");

    ItemRepository& repository = defaultItemRepository();
    repository.incrementCardCount(31000);
    repository.incrementLuckyBagCount(31000);
    repository.incrementGiftBoxCount(31000);
    repository.incrementEventItemCount(31000u, 1u);

    EXPECT_EQ("6", queryScalar("SELECT CARDCOUNT FROM CardCount WHERE CARDKIND=31000"));
    EXPECT_EQ("5", queryScalar("SELECT CARDCOUNT FROM CardCount WHERE CARDKIND=31001"));
    EXPECT_EQ("6", queryScalar("SELECT BAGCOUNT FROM LuckyBagCount WHERE BAGKIND=31000"));
    EXPECT_EQ("5", queryScalar("SELECT BAGCOUNT FROM LuckyBagCount WHERE BAGKIND=31001"));
    EXPECT_EQ("6", queryScalar("SELECT BOXCOUNT FROM GiftBoxCount WHERE BOXKIND=31000"));
    EXPECT_EQ("5", queryScalar("SELECT BOXCOUNT FROM GiftBoxCount WHERE BOXKIND=31001"));
    EXPECT_EQ("6", queryScalar("SELECT Count FROM EventItemCount WHERE ItemClass=31000 AND ItemType=1"));
    EXPECT_EQ("5", queryScalar("SELECT Count FROM EventItemCount WHERE ItemClass=31000 AND ItemType=2"));

    // ResurrectItemCount has no key column: the UPDATE touches every row.
    // The seed has one row, so a second is inserted (and cleaned by value)
    // to tell an every-row UPDATE from a one-row one.
    execSQL("INSERT INTO ResurrectItemCount (Count) VALUES (31000)");
    int before = atoi(queryScalar("SELECT SUM(Count) FROM ResurrectItemCount").c_str());
    int rowCount = atoi(queryScalar("SELECT COUNT(*) FROM ResurrectItemCount").c_str());
    ASSERT_GE(rowCount, 2);
    repository.incrementResurrectItemCount();
    EXPECT_EQ(before + rowCount, atoi(queryScalar("SELECT SUM(Count) FROM ResurrectItemCount").c_str()));
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM ResurrectItemCount WHERE Count=31001"));
}

TEST_F(ItemMySQL, EventQuestRewardIsTakenOncePerCountAndOnlyWhenDue) {
    execSQL("INSERT INTO EventQuestRewardSchedule (RewardID, QuestLevel, Count, Time) "
            "VALUES (31000, 3, 1, '2000-01-01 00:00:00')");
    execSQL("INSERT INTO EventQuestRewardSchedule (RewardID, QuestLevel, Count, Time) "
            "VALUES (31001, 3, 5, '2099-01-01 00:00:00')"); // not due yet

    ItemRepository& repository = defaultItemRepository();
    EXPECT_TRUE(repository.takeEventQuestReward(31000, 3));
    EXPECT_EQ("0", queryScalar("SELECT Count FROM EventQuestRewardSchedule WHERE RewardID=31000"));
    EXPECT_FALSE(repository.takeEventQuestReward(31000, 3)); // exhausted
    EXPECT_FALSE(repository.takeEventQuestReward(31000, 4)); // other level
    EXPECT_FALSE(repository.takeEventQuestReward(31001, 3)); // not due
    EXPECT_EQ("5", queryScalar("SELECT Count FROM EventQuestRewardSchedule WHERE RewardID=31001"));
}

TEST_F(ItemMySQL, ItemRowPositionReadAndDeleteWorkOnTheNamedObjectTable) {
    execSQL("INSERT INTO PotionObject (ItemID, ObjectID, ItemType, OwnerID, Storage, StorageID, X, Y, Num) "
            "VALUES (31000, 77, 2, 'it-owner', 1, 5, 3, 4, 1)");

    ItemRepository& repository = defaultItemRepository();
    ItemPositionRow row;
    ASSERT_TRUE(repository.loadItemPosition("PotionObject", 31000, row));
    EXPECT_EQ("it-owner", row.ownerID);
    EXPECT_EQ(1, row.storage);
    EXPECT_EQ(5, row.storageID);
    EXPECT_EQ(3, row.x);
    EXPECT_EQ(4, row.y);
    EXPECT_EQ(77, row.objectID);
    EXPECT_FALSE(repository.loadItemPosition("PotionObject", 31001, row));

    EXPECT_TRUE(repository.deleteItemRow("PotionObject", 31000));
    EXPECT_FALSE(repository.deleteItemRow("PotionObject", 31000)); // already gone
    EXPECT_EQ("0", queryScalar("SELECT COUNT(*) FROM PotionObject WHERE ItemID=31000"));
}

TEST_F(ItemMySQL, ItemRowCountAndHighestIdAreReadFromTheNamedObjectTable) {
    ItemRepository& repository = defaultItemRepository();
    int before = atoi(queryScalar("SELECT COUNT(*) FROM PotionObject").c_str());
    EXPECT_EQ(before, (int)repository.countItemRows("PotionObject"));

    execSQL("INSERT INTO PotionObject (ItemID, ObjectID, ItemType, OwnerID, Storage, StorageID, X, Y, Num) "
            "VALUES (31005, 77, 2, 'it-owner', 1, 5, 3, 4, 1), (31007, 78, 2, 'it-owner', 1, 5, 4, 4, 1)");
    EXPECT_EQ(before + 2, (int)repository.countItemRows("PotionObject"));

    // The dump's own rows (if any) sit below the test ids, so the two inserts
    // decide the maximum; the scalar read pins the repository against the table.
    DWORD highest = repository.loadMaxItemID("PotionObject");
    EXPECT_EQ(queryScalar("SELECT MAX(ItemID) FROM PotionObject"), std::to_string(highest));
    EXPECT_EQ(31007u, highest);
}

// --- the gear item classes' object and info tables ---------------------------
// The gear families (slayer, vampire and ousters gear, and the six with their
// own Info shapes) share one method set; the table (and its literal quirks) is
// picked by GearTable.

namespace {
struct GearTableName {
    GearTable table;
    const char* name;
};
const GearTableName kGearTables[] = {
    {GEAR_RING, "RingObject"},
    {GEAR_BRACELET, "BraceletObject"},
    {GEAR_NECKLACE, "NecklaceObject"},
    {GEAR_COAT, "CoatObject"},
    {GEAR_TROUSER, "TrouserObject"},
    {GEAR_SHOES, "ShoesObject"},
    {GEAR_GLOVE, "GloveObject"},
    {GEAR_HELM, "HelmObject"},
    {GEAR_SHIELD, "ShieldObject"},
    {GEAR_VAMPIRE_RING, "VampireRingObject"},
    {GEAR_VAMPIRE_BRACELET, "VampireBraceletObject"},
    {GEAR_VAMPIRE_NECKLACE, "VampireNecklaceObject"},
    {GEAR_OUSTERS_RING, "OustersRingObject"},
    {GEAR_OUSTERS_COAT, "OustersCoatObject"},
    {GEAR_OUSTERS_CIRCLET, "OustersCircletObject"},
    {GEAR_OUSTERS_PENDENT, "OustersPendentObject"},
    {GEAR_OUSTERS_BOOTS, "OustersBootsObject"},
    {GEAR_VAMPIRE_COAT, "VampireCoatObject"},
    {GEAR_OUSTERS_STONE, "OustersStoneObject"},
    {GEAR_VAMPIRE_EARRING, "VampireEarringObject"},
    {GEAR_VAMPIRE_WEAPON, "VampireWeaponObject"},
    {GEAR_OUSTERS_CHAKRAM, "OustersChakramObject"},
    {GEAR_OUSTERS_WRISTLET, "OustersWristletObject"},
};
const GearTableName kGearInfoTables[] = {
    {GEAR_RING, "RingInfo"},
    {GEAR_BRACELET, "BraceletInfo"},
    {GEAR_NECKLACE, "NecklaceInfo"},
    {GEAR_COAT, "CoatInfo"},
    {GEAR_TROUSER, "TrouserInfo"},
    {GEAR_SHOES, "ShoesInfo"},
    {GEAR_GLOVE, "GloveInfo"},
    {GEAR_HELM, "HelmInfo"},
    {GEAR_SHIELD, "ShieldInfo"},
    {GEAR_VAMPIRE_RING, "VampireRingInfo"},
    {GEAR_VAMPIRE_BRACELET, "VampireBraceletInfo"},
    {GEAR_VAMPIRE_NECKLACE, "VampireNecklaceInfo"},
    {GEAR_OUSTERS_RING, "OustersRingInfo"},
    {GEAR_OUSTERS_COAT, "OustersCoatInfo"},
    {GEAR_OUSTERS_CIRCLET, "OustersCircletInfo"},
    {GEAR_OUSTERS_PENDENT, "OustersPendentInfo"},
    {GEAR_OUSTERS_BOOTS, "OustersBootsInfo"},
    {GEAR_VAMPIRE_EARRING, "VampireEarringInfo"},
};
const GearTableName kSilverTables[] = {
    {GEAR_SWORD, "SwordObject"},
    {GEAR_BLADE, "BladeObject"},
    {GEAR_CROSS, "CrossObject"},
    {GEAR_MACE, "MaceObject"},
};
const GearTableName kGunTables[] = {
    {GEAR_AR, "ARObject"},
    {GEAR_SG, "SGObject"},
    {GEAR_SMG, "SMGObject"},
    {GEAR_SR, "SRObject"},
};
const GearTableName kNumTables[] = {
    {GEAR_EVENT_ITEM, "EventItemObject"},   {GEAR_EVENT_TREE, "EventTreeObject"},
    {GEAR_LUCKY_BAG, "LuckyBagObject"},     {GEAR_MOON_CARD, "MoonCardObject"},
    {GEAR_EVENT_ETC, "EventETCObject"},     {GEAR_RESURRECT_ITEM, "ResurrectItemObject"},
    {GEAR_DYE_POTION, "DyePotionObject"},   {GEAR_EVENT_STAR, "EventStarObject"},
    {GEAR_EFFECT_ITEM, "EffectItemObject"}, {GEAR_PET_ENCHANT_ITEM, "PetEnchantItemObject"},
};
const GearTableName kNumOnlyTables[] = {
    {GEAR_ETC, "ETCObject"},       {GEAR_SERUM, "SerumObject"},          {GEAR_VAMPIRE_ETC, "VampireETCObject"},
    {GEAR_WATER, "WaterObject"},   {GEAR_HOLY_WATER, "HolyWaterObject"}, {GEAR_MAGAZINE, "MagazineObject"},
    {GEAR_PUPA, "PupaObject"},     {GEAR_LARVA, "LarvaObject"},          {GEAR_COMPOS_MEI, "ComposMeiObject"},
    {GEAR_POTION, "PotionObject"},
};
const GearTableName kNumZoneVariantTables[] = {
    {GEAR_SKULL, "SkullObject"},
    {GEAR_BOMB, "BombObject"},
    {GEAR_BOMB_MATERIAL, "BombMaterialObject"},
    {GEAR_MINE, "MineObject"},
};
const GearTableName kFlagTables[] = {
    {GEAR_QUEST_ITEM, "QuestItemObject"},
    {GEAR_SMSITEM, "SMSItemObject"},
    {GEAR_SUB_INVENTORY, "SubInventoryObject"},
    {GEAR_TRAP_ITEM, "TrapItemObject"},
};
const GearTableName kPlainTables[] = {
    {GEAR_EVENT_GIFT_BOX, "EventGiftBoxObject"},
    {GEAR_LEARNING_ITEM, "LearningItemObject"},
};
const GearTableName kNumIntTables[] = {
    {GEAR_MIXING_ITEM, "MixingItemObject"},
    {GEAR_PET_FOOD, "PetFoodObject"},
};
const GearTableName kChargeTables[] = {
    {GEAR_OUSTERS_SUMMON_ITEM, "OustersSummonItemObject"},
    {GEAR_SLAYER_PORTAL_ITEM, "SlayerPortalItemObject"},
};
const GearTableName kCoupleRingTables[] = {
    {GEAR_COUPLE_RING, "CoupleRingObject"},
    {GEAR_VAMPIRE_COUPLE_RING, "VampireCoupleRingObject"},
};
const GearTableName kPocketTables[] = {
    {GEAR_BELT, "BeltObject"},
    {GEAR_OUSTERS_ARMSBAND, "OustersArmsbandObject"},
};
// Gear objects whose Loader::load(Zone*) holds no SQL: no zone literal, no zone load.
const GearTableName kOneLoaderGearTables[] = {
    {GEAR_MITTEN, "MittenObject"},
    {GEAR_SHOULDER_ARMOR, "ShoulderArmorObject"},
    {GEAR_PERSONA, "PersonaObject"},
};
const GearTableName kOptionGradeTables[] = {
    {GEAR_DERMIS, "DermisObject"},
    {GEAR_FASCIA, "FasciaObject"},
    {GEAR_CARRYING_RECEIVER, "CarryingReceiverObject"},
};
// The war items: no owner SELECT at all - their creature loader deletes the owner's rows.
const GearTableName kWarTables[] = {
    {GEAR_BLOOD_BIBLE, "BloodBibleObject"},
    {GEAR_CASTLE_SYMBOL, "CastleSymbolObject"},
    {GEAR_SWEEPER, "SweeperObject"},
    {GEAR_RELIC, "RelicObject"},
};
} // namespace

class ItemObjectMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        clean();
    }
    virtual void TearDown() {
        clean();
    }
    static void clean() {
        for (size_t i = 0; i < sizeof(kGearTables) / sizeof(kGearTables[0]); i++)
            execSQL(std::string("DELETE FROM ") + kGearTables[i].name + " WHERE ItemID >= 31000");
        for (size_t i = 0; i < sizeof(kSilverTables) / sizeof(kSilverTables[0]); i++)
            execSQL(std::string("DELETE FROM ") + kSilverTables[i].name + " WHERE ItemID >= 31000");
        for (size_t i = 0; i < sizeof(kGunTables) / sizeof(kGunTables[0]); i++)
            execSQL(std::string("DELETE FROM ") + kGunTables[i].name + " WHERE ItemID >= 31000");
        for (size_t i = 0; i < sizeof(kNumTables) / sizeof(kNumTables[0]); i++)
            execSQL(std::string("DELETE FROM ") + kNumTables[i].name + " WHERE ItemID >= 31000");
        for (size_t i = 0; i < sizeof(kNumOnlyTables) / sizeof(kNumOnlyTables[0]); i++)
            execSQL(std::string("DELETE FROM ") + kNumOnlyTables[i].name + " WHERE ItemID >= 31000");
        for (size_t i = 0; i < sizeof(kNumZoneVariantTables) / sizeof(kNumZoneVariantTables[0]); i++)
            execSQL(std::string("DELETE FROM ") + kNumZoneVariantTables[i].name + " WHERE ItemID >= 31000");
        for (size_t i = 0; i < sizeof(kFlagTables) / sizeof(kFlagTables[0]); i++)
            execSQL(std::string("DELETE FROM ") + kFlagTables[i].name + " WHERE ItemID >= 31000");
        for (size_t i = 0; i < sizeof(kPlainTables) / sizeof(kPlainTables[0]); i++)
            execSQL(std::string("DELETE FROM ") + kPlainTables[i].name + " WHERE ItemID >= 31000");
        for (size_t i = 0; i < sizeof(kNumIntTables) / sizeof(kNumIntTables[0]); i++)
            execSQL(std::string("DELETE FROM ") + kNumIntTables[i].name + " WHERE ItemID >= 31000");
        execSQL("DELETE FROM KeyObject WHERE ItemID >= 31000");
        for (size_t i = 0; i < sizeof(kChargeTables) / sizeof(kChargeTables[0]); i++)
            execSQL(std::string("DELETE FROM ") + kChargeTables[i].name + " WHERE ItemID >= 31000");
        execSQL("DELETE FROM MoneyObject WHERE ItemID >= 31000");
        for (size_t i = 0; i < sizeof(kCoupleRingTables) / sizeof(kCoupleRingTables[0]); i++)
            execSQL(std::string("DELETE FROM ") + kCoupleRingTables[i].name + " WHERE ItemID >= 31000");
        execSQL("DELETE FROM VampirePortalItemObject WHERE ItemID >= 31000");
        execSQL("DELETE FROM VampireAmuletObject WHERE ItemID >= 31000");
        execSQL("DELETE FROM CoreZapObject WHERE ItemID >= 31000");
        for (size_t i = 0; i < sizeof(kPocketTables) / sizeof(kPocketTables[0]); i++)
            execSQL(std::string("DELETE FROM ") + kPocketTables[i].name + " WHERE ItemID >= 31000");
        for (size_t i = 0; i < sizeof(kOneLoaderGearTables) / sizeof(kOneLoaderGearTables[0]); i++)
            execSQL(std::string("DELETE FROM ") + kOneLoaderGearTables[i].name + " WHERE ItemID >= 31000");
        for (size_t i = 0; i < sizeof(kOptionGradeTables) / sizeof(kOptionGradeTables[0]); i++)
            execSQL(std::string("DELETE FROM ") + kOptionGradeTables[i].name + " WHERE ItemID >= 31000");
        for (size_t i = 0; i < sizeof(kWarTables) / sizeof(kWarTables[0]); i++)
            execSQL(std::string("DELETE FROM ") + kWarTables[i].name + " WHERE ItemID >= 31000");
        execSQL("DELETE FROM MotorcycleObject WHERE ItemID >= 31000");
        execSQL("DELETE FROM CodeSheetObject WHERE ItemID >= 31000");
        execSQL("DELETE FROM WarItemObject WHERE ItemID >= 31000");
        execSQL("DELETE FROM PetItemObject WHERE ItemID >= 31000");
    }
};

TEST_F(ItemObjectMySQL, GearRowsRoundTripThroughEveryTablesOwnLiterals) {
    ItemObjectRepository& repository = defaultItemObjectRepository();

    for (size_t i = 0; i < sizeof(kGearTables) / sizeof(kGearTables[0]); i++) {
        const GearTable table = kGearTables[i].table;
        const std::string name = kGearTables[i].name;
        const std::string where = std::string(" FROM ") + name + " WHERE ItemID=";

        // Both rows belong to it-owner; Storage 1 is in the owner load's
        // IN(0, 1, 2, 3, 4, 9) and 5 (STORAGE_ZONE) is not, so the owner load's
        // single row pins the Storage clause, not the owner.
        repository.insertGear(table, 31000 + i, 77, 3, "it-owner", 1, 5, 2, 4, "opt", 10, 6, 1);
        repository.insertGear(table, 31100 + i, 78, 3, "it-owner", 5, 31000, 7, 8, "", 9, 6, 0);
        EXPECT_EQ("2", queryScalar("SELECT COUNT(*) FROM " + name + " WHERE ItemID >= 31000")) << name;
        EXPECT_EQ("it-owner", queryScalar("SELECT OwnerID" + where + std::to_string(31000 + i))) << name;
        EXPECT_EQ("1", queryScalar("SELECT ItemFlag" + where + std::to_string(31000 + i))) << name;

        std::vector<GearObjectRow> owned = repository.loadGearOfOwner(table, "it-owner");
        ASSERT_EQ(1u, owned.size()) << name;
        EXPECT_EQ(31000 + i, owned[0].itemID);
        EXPECT_EQ(77u, owned[0].objectID);
        EXPECT_EQ(3u, owned[0].itemType);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(5u, owned[0].storageID);
        EXPECT_EQ(2, owned[0].x);
        EXPECT_EQ(4, owned[0].y);
        EXPECT_EQ("opt", owned[0].optionField);
        EXPECT_EQ(10, owned[0].durability);
        EXPECT_EQ(6, owned[0].grade);
        EXPECT_EQ(0, owned[0].enchantLevel); // the INSERT names no EnchantLevel: the column default
        EXPECT_EQ(1, owned[0].createType);

        std::vector<GearZoneObjectRow> inZone = repository.loadGearInZone(table, 5, 31000);
        ASSERT_EQ(1u, inZone.size()) << name;
        EXPECT_EQ((int)(31100 + i), inZone[0].itemID);
        EXPECT_EQ(78, inZone[0].objectID);
        EXPECT_EQ(5, inZone[0].storage);
        EXPECT_EQ(31000, inZone[0].storageID);
        EXPECT_EQ(7, inZone[0].x);
        EXPECT_EQ(8, inZone[0].y);
        EXPECT_EQ(9, inZone[0].durability);
        EXPECT_TRUE(repository.loadGearInZone(table, 5, 31001).empty()) << name;

        repository.updateGear(table, 79, 4, "it-owner", 2, 6, 3, 5, "opt2", 11, 7, 2, 31000 + i);
        owned = repository.loadGearOfOwner(table, "it-owner");
        ASSERT_EQ(1u, owned.size()) << name;
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(4u, owned[0].itemType);
        EXPECT_EQ(2, owned[0].storage);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(3, owned[0].x);
        EXPECT_EQ(5, owned[0].y);
        EXPECT_EQ("opt2", owned[0].optionField);
        EXPECT_EQ(11, owned[0].durability);
        EXPECT_EQ(7, owned[0].grade);
        EXPECT_EQ(2, owned[0].enchantLevel);

        repository.tinysaveGear(table, "Durability=3", 31000 + i);
        EXPECT_EQ("3", queryScalar("SELECT Durability" + where + std::to_string(31000 + i))) << name;
    }
}

TEST_F(ItemObjectMySQL, GearInfoTablesLoadWithTheirHighestType) {
    ItemObjectRepository& repository = defaultItemObjectRepository();

    for (size_t i = 0; i < sizeof(kGearInfoTables) / sizeof(kGearInfoTables[0]); i++) {
        const GearTable table = kGearInfoTables[i].table;
        const std::string name = kGearInfoTables[i].name;

        int maxType = repository.loadMaxGearType(table);
        EXPECT_GT(maxType, 0) << name;
        EXPECT_EQ(queryScalar("SELECT MAX(ItemType) FROM " + name), std::to_string(maxType)) << name;

        std::vector<GearInfoRow> infos = repository.loadGearInfos(table);
        ASSERT_FALSE(infos.empty()) << name;
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM " + name).c_str()), (int)infos.size()) << name;
        std::string id = std::to_string(infos[0].itemType);
        EXPECT_EQ(infos[0].name, queryScalar("SELECT Name FROM " + name + " WHERE ItemType=" + id)) << name;
        EXPECT_EQ(std::to_string(infos[0].nextItemType),
                  queryScalar("SELECT NextItemType FROM " + name + " WHERE ItemType=" + id))
            << name;
        EXPECT_EQ(std::to_string(infos[0].downgradeRatio),
                  queryScalar("SELECT DowngradeRatio FROM " + name + " WHERE ItemType=" + id))
            << name;
    }
}

TEST_F(ItemObjectMySQL, GearInfoVariantsLoadTheirOwnColumnsAndRefuseTheWrongLoader) {
    ItemObjectRepository& repository = defaultItemObjectRepository();

    std::vector<GearInfoNoRatioRow> coats = repository.loadGearInfosNoRatio(GEAR_VAMPIRE_COAT);
    ASSERT_FALSE(coats.empty());
    EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM VampireCoatInfo").c_str()), (int)coats.size());
    std::string id = std::to_string(coats[0].itemType);
    EXPECT_EQ(coats[0].name, queryScalar("SELECT Name FROM VampireCoatInfo WHERE ItemType=" + id));
    EXPECT_EQ(std::to_string(coats[0].nextItemType),
              queryScalar("SELECT NextItemType FROM VampireCoatInfo WHERE ItemType=" + id));

    std::vector<GearInfoElementalRow> stones = repository.loadGearInfosElemental(GEAR_OUSTERS_STONE);
    ASSERT_FALSE(stones.empty());
    EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM OustersStoneInfo").c_str()), (int)stones.size());
    id = std::to_string(stones[0].gear.itemType);
    EXPECT_EQ(std::to_string(stones[0].gear.downgradeRatio),
              queryScalar("SELECT DowngradeRatio FROM OustersStoneInfo WHERE ItemType=" + id));
    EXPECT_EQ(std::to_string(stones[0].elemental),
              queryScalar("SELECT Elemental FROM OustersStoneInfo WHERE ItemType=" + id));

    const GearTable weaponTables[] = {GEAR_VAMPIRE_WEAPON, GEAR_OUSTERS_CHAKRAM};
    const char* weaponNames[] = {"VampireWeaponInfo", "OustersChakramInfo"};
    for (int w = 0; w < 2; w++) {
        std::vector<WeaponInfoRow> weapons = repository.loadWeaponInfos(weaponTables[w]);
        const std::string name = weaponNames[w];
        ASSERT_FALSE(weapons.empty()) << name;
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM " + name).c_str()), (int)weapons.size()) << name;
        id = std::to_string(weapons[0].itemType);
        EXPECT_EQ(std::to_string(weapons[0].minDamage),
                  queryScalar("SELECT minDamage FROM " + name + " WHERE ItemType=" + id))
            << name;
        EXPECT_EQ(std::to_string(weapons[0].criticalBonus),
                  queryScalar("SELECT CriticalBonus FROM " + name + " WHERE ItemType=" + id))
            << name;
    }

    std::vector<WeaponInfoElementalRow> wristlets = repository.loadWeaponInfosElemental(GEAR_OUSTERS_WRISTLET);
    ASSERT_FALSE(wristlets.empty());
    EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM OustersWristletInfo").c_str()), (int)wristlets.size());
    id = std::to_string(wristlets[0].weapon.itemType);
    EXPECT_EQ(std::to_string(wristlets[0].weapon.maxDamage),
              queryScalar("SELECT maxDamage FROM OustersWristletInfo WHERE ItemType=" + id));
    EXPECT_EQ(std::to_string(wristlets[0].elementalType),
              queryScalar("SELECT ElementalType FROM OustersWristletInfo WHERE ItemType=" + id));

    // VampireEarring's MAX literal is ifnull(MAX(ItemType),0); the shared loader's
    // next(); getInt(1) reads it like any other (the seeded table is not empty, so
    // the ifnull never fires here).
    EXPECT_EQ(queryScalar("SELECT MAX(ItemType) FROM VampireEarringInfo"),
              std::to_string(repository.loadMaxGearType(GEAR_VAMPIRE_EARRING)));

    // The shape guard: a loader refuses a table of another Info shape.
    EXPECT_THROW(repository.loadGearInfos(GEAR_VAMPIRE_COAT), Error);
    EXPECT_THROW(repository.loadGearInfosNoRatio(GEAR_RING), Error);
    EXPECT_THROW(repository.loadWeaponInfos(GEAR_OUSTERS_WRISTLET), Error);
}

TEST_F(ItemObjectMySQL, SilverWeaponRowsRoundTripWithTheirSilverColumn) {
    ItemObjectRepository& repository = defaultItemObjectRepository();

    for (size_t i = 0; i < sizeof(kSilverTables) / sizeof(kSilverTables[0]); i++) {
        const GearTable table = kSilverTables[i].table;
        const std::string name = kSilverTables[i].name;
        const std::string where = std::string(" FROM ") + name + " WHERE ItemID=";

        // Gear's INSERT (no Silver column: the table default), then the weapon UPDATE writes it.
        repository.insertGear(table, 31000 + i, 77, 3, "it-owner", 1, 5, 2, 4, "opt", 10, 6, 1);
        repository.insertGear(table, 31100 + i, 78, 3, "it-owner", 5, 31000, 7, 8, "", 9, 6, 0);
        repository.updateSilverWeapon(table, 79, 4, "it-owner", 1, 6, 3, 5, "opt2", 11, 2, 250, 7, 31000 + i);
        EXPECT_EQ("250", queryScalar("SELECT Silver" + where + std::to_string(31000 + i))) << name;

        std::vector<SilverWeaponObjectRow> owned = repository.loadSilverWeaponOfOwner(table, "it-owner");
        ASSERT_EQ(1u, owned.size()) << name;
        EXPECT_EQ(31000 + i, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(4u, owned[0].itemType);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(3, owned[0].x);
        EXPECT_EQ(5, owned[0].y);
        EXPECT_EQ("opt2", owned[0].optionField);
        EXPECT_EQ(11, owned[0].durability);
        EXPECT_EQ(2, owned[0].enchantLevel);
        EXPECT_EQ(250, owned[0].silver);
        EXPECT_EQ(7, owned[0].grade);
        EXPECT_EQ(1, owned[0].createType);

        std::vector<SilverWeaponZoneObjectRow> inZone = repository.loadSilverWeaponInZone(table, 5, 31000);
        ASSERT_EQ(1u, inZone.size()) << name;
        EXPECT_EQ((int)(31100 + i), inZone[0].itemID);
        EXPECT_EQ(0, inZone[0].silver); // the INSERT never names Silver
        EXPECT_EQ(9, inZone[0].durability);
        EXPECT_TRUE(repository.loadSilverWeaponInZone(table, 5, 31001).empty()) << name;

        // The MAX(ItemType) literal of each seeded Info table.
        const std::string info = name.substr(0, name.size() - 6) + "Info";
        EXPECT_EQ(queryScalar("SELECT MAX(ItemType) FROM " + info), std::to_string(repository.loadMaxGearType(table)))
            << info;

        repository.tinysaveGear(table, "Silver=3", 31000 + i);
        EXPECT_EQ("3", queryScalar("SELECT Silver" + where + std::to_string(31000 + i))) << name;
    }

    // The object-shape guard, both ways.
    EXPECT_THROW(repository.loadGearOfOwner(GEAR_SWORD, "it-owner"), Error);
    EXPECT_THROW(repository.updateGear(GEAR_MACE, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1, 31000), Error);
    EXPECT_THROW(repository.loadSilverWeaponInZone(GEAR_RING, 5, 31000), Error);

    // The Info shapes: MaxSilver after maxDamage, and Cross / Mace's MPBonus before it.
    std::vector<SilverWeaponInfoRow> swords = repository.loadSilverWeaponInfos(GEAR_SWORD);
    ASSERT_FALSE(swords.empty());
    EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM SwordInfo").c_str()), (int)swords.size());
    std::string id = std::to_string(swords[0].itemType);
    EXPECT_EQ(std::to_string(swords[0].maxSilver), queryScalar("SELECT MaxSilver FROM SwordInfo WHERE ItemType=" + id));
    EXPECT_EQ(std::to_string(swords[0].speed), queryScalar("SELECT Speed FROM SwordInfo WHERE ItemType=" + id));
    EXPECT_FALSE(repository.loadSilverWeaponInfos(GEAR_BLADE).empty());
    std::vector<SilverWeaponMPInfoRow> maces = repository.loadSilverWeaponMPInfos(GEAR_MACE);
    ASSERT_FALSE(maces.empty());
    EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM MaceInfo").c_str()), (int)maces.size());
    id = std::to_string(maces[0].itemType);
    EXPECT_EQ(std::to_string(maces[0].mpBonus), queryScalar("SELECT MPBonus FROM MaceInfo WHERE ItemType=" + id));
    EXPECT_EQ(std::to_string(maces[0].downgradeRatio),
              queryScalar("SELECT DowngradeRatio FROM MaceInfo WHERE ItemType=" + id));
    EXPECT_FALSE(repository.loadSilverWeaponMPInfos(GEAR_CROSS).empty());
    EXPECT_THROW(repository.loadSilverWeaponInfos(GEAR_CROSS), Error);
    EXPECT_THROW(repository.loadWeaponInfos(GEAR_SWORD), Error);
}

TEST_F(ItemObjectMySQL, GunRowsRoundTripWithBulletCountAndSilverInEachTablesOrder) {
    ItemObjectRepository& repository = defaultItemObjectRepository();

    for (size_t i = 0; i < sizeof(kGunTables) / sizeof(kGunTables[0]); i++) {
        const GearTable table = kGunTables[i].table;
        const std::string name = kGunTables[i].name;
        const std::string where = std::string(" FROM ") + name + " WHERE ItemID=";
        const std::string id = std::to_string(31000 + i);

        // The gun INSERT writes BulletCount (Silver stays at the table default);
        // the UPDATE writes both; saveBullet writes BulletCount alone.
        repository.insertGun(table, 31000 + i, 77, 3, "it-owner", 1, 5, 2, 4, "opt", 10, 12, 6, 1);
        repository.insertGun(table, 31100 + i, 78, 3, "it-owner", 5, 31000, 7, 8, "", 9, 30, 6, 0);
        EXPECT_EQ("12", queryScalar("SELECT BulletCount" + where + id)) << name;
        repository.updateGun(table, 79, 4, "it-owner", 1, 6, 3, 5, "opt2", 11, 2, 20, 250, 7, 31000 + i);
        EXPECT_EQ("20", queryScalar("SELECT BulletCount" + where + id)) << name;
        EXPECT_EQ("250", queryScalar("SELECT Silver" + where + id)) << name;
        repository.saveGunBullet(table, 19, 31000 + i);
        EXPECT_EQ("19", queryScalar("SELECT BulletCount" + where + id)) << name;

        // Each column lands in its own field whichever order the table's SELECT names
        // EnchantLevel, BulletCount and Silver (AR's differs from the other three).
        std::vector<GunObjectRow> owned = repository.loadGunOfOwner(table, "it-owner");
        ASSERT_EQ(1u, owned.size()) << name;
        EXPECT_EQ(31000 + i, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(4u, owned[0].itemType);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(3, owned[0].x);
        EXPECT_EQ(5, owned[0].y);
        EXPECT_EQ("opt2", owned[0].optionField);
        EXPECT_EQ(11, owned[0].durability);
        EXPECT_EQ(2, owned[0].enchantLevel) << name;
        EXPECT_EQ(19, owned[0].bulletCount) << name;
        EXPECT_EQ(250, owned[0].silver) << name;
        EXPECT_EQ(7, owned[0].grade);
        EXPECT_EQ(1, owned[0].createType);

        std::vector<GunZoneObjectRow> inZone = repository.loadGunInZone(table, 5, 31000);
        ASSERT_EQ(1u, inZone.size()) << name;
        EXPECT_EQ((int)(31100 + i), inZone[0].itemID);
        EXPECT_EQ(9, inZone[0].durability);
        EXPECT_EQ(0, inZone[0].enchantLevel) << name;
        EXPECT_EQ(30, inZone[0].bulletCount) << name;
        EXPECT_EQ(0, inZone[0].silver) << name; // the INSERT never names Silver

        // The Info shape: ToHitBonus and `Range` after maxDamage.
        const std::string info = name.substr(0, name.size() - 6) + "Info";
        std::vector<GunInfoRow> infos = repository.loadGunInfos(table);
        ASSERT_FALSE(infos.empty()) << info;
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM " + info).c_str()), (int)infos.size()) << info;
        const std::string type = std::to_string(infos[0].itemType);
        EXPECT_EQ(std::to_string(infos[0].toHitBonus),
                  queryScalar("SELECT ToHitBonus FROM " + info + " WHERE ItemType=" + type));
        EXPECT_EQ(std::to_string(infos[0].range),
                  queryScalar("SELECT `Range` FROM " + info + " WHERE ItemType=" + type));
        EXPECT_EQ(std::to_string(infos[0].downgradeRatio),
                  queryScalar("SELECT DowngradeRatio FROM " + info + " WHERE ItemType=" + type));
        EXPECT_EQ(queryScalar("SELECT MAX(ItemType) FROM " + info), std::to_string(repository.loadMaxGearType(table)))
            << info;

        // tinysave: SG, SMG and SR's literal carries a BulletCount; AR's is gear's.
        if (table == GEAR_AR) {
            repository.tinysaveGear(table, "Silver=3", 31000 + i);
            EXPECT_EQ("19", queryScalar("SELECT BulletCount" + where + id)) << name;
        } else {
            repository.tinysaveGun(table, "Silver=3", 5, 31000 + i);
            EXPECT_EQ("5", queryScalar("SELECT BulletCount" + where + id)) << name;
        }
        EXPECT_EQ("3", queryScalar("SELECT Silver" + where + id)) << name;
    }

    // Each tinysave method refuses the tables whose literal takes the other argument list;
    // insertGear's twelve varargs refuse the guns' thirteen-argument INSERT.
    EXPECT_THROW(repository.insertGear(GEAR_AR, 31900, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1), Error);
    EXPECT_THROW(repository.insertGear(GEAR_SR, 31900, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1), Error);
    EXPECT_THROW(repository.tinysaveGun(GEAR_AR, "Silver=3", 5, 31000), Error);
    EXPECT_THROW(repository.tinysaveGear(GEAR_SMG, "Silver=3", 31002), Error);
    EXPECT_THROW(repository.tinysaveGun(GEAR_SWORD, "Silver=3", 5, 31000), Error);

    // The object-shape guard, both ways.
    EXPECT_THROW(repository.loadGearOfOwner(GEAR_AR, "it-owner"), Error);
    EXPECT_THROW(repository.loadSilverWeaponOfOwner(GEAR_SMG, "it-owner"), Error);
    EXPECT_THROW(repository.insertGun(GEAR_RING, 31900, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1, 1), Error);
    EXPECT_THROW(repository.updateGun(GEAR_SWORD, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1, 1, 1, 31000), Error);
    EXPECT_THROW(repository.saveGunBullet(GEAR_RING, 1, 31000), Error);
    EXPECT_THROW(repository.loadGunInZone(GEAR_MACE, 5, 31000), Error);

    // The Info guard, both ways.
    EXPECT_THROW(repository.loadGunInfos(GEAR_SWORD), Error);
    EXPECT_THROW(repository.loadWeaponInfos(GEAR_SG), Error);
}

TEST_F(ItemObjectMySQL, NumItemRowsRoundTripThroughTheirNineColumns) {
    ItemObjectRepository& repository = defaultItemObjectRepository();

    for (size_t i = 0; i < sizeof(kNumTables) / sizeof(kNumTables[0]); i++) {
        const GearTable table = kNumTables[i].table;
        const std::string name = kNumTables[i].name;
        const std::string where = std::string(" FROM ") + name + " WHERE ItemID=";
        const std::string id = std::to_string(31000 + i);

        repository.insertNumItem(table, 31000 + i, 77, 3, "it-owner", 1, 5, 2, 4, 12, 1);
        repository.insertNumItem(table, 31100 + i, 78, 3, "it-owner", 5, 31000, 7, 8, 30, 0);
        EXPECT_EQ("12", queryScalar("SELECT Num" + where + id)) << name;
        EXPECT_EQ("1", queryScalar("SELECT ItemFlag" + where + id)) << name;
        repository.updateNumItem(table, 79, 4, "it-owner", 1, 6, 3, 5, 20, 31000 + i);
        EXPECT_EQ("20", queryScalar("SELECT Num" + where + id)) << name;
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + id)) << name;

        std::vector<NumObjectRow> owned = repository.loadNumItemOfOwner(table, "it-owner");
        ASSERT_EQ(1u, owned.size()) << name;
        EXPECT_EQ(31000 + i, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(4u, owned[0].itemType);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(3, owned[0].x);
        EXPECT_EQ(5, owned[0].y);
        EXPECT_EQ(20, owned[0].num) << name;
        EXPECT_EQ(1, owned[0].createType);

        std::vector<NumZoneObjectRow> inZone = repository.loadNumItemInZone(table, 5, 31000);
        ASSERT_EQ(1u, inZone.size()) << name;
        EXPECT_EQ((int)(31100 + i), inZone[0].itemID);
        EXPECT_EQ(30, inZone[0].num) << name;
        EXPECT_EQ(0, inZone[0].createType);
        EXPECT_TRUE(repository.loadNumItemInZone(table, 5, 31001).empty()) << name;

        repository.tinysaveGear(table, "Num=9", 31000 + i);
        EXPECT_EQ("9", queryScalar("SELECT Num" + where + id)) << name;

        // The MAX(ItemType) literal of each Info table (some of these tables are seeded, some not).
        const std::string info = name.substr(0, name.size() - 6) + "Info";
        EXPECT_EQ(atoi(queryScalar("SELECT ifnull(MAX(ItemType),0) FROM " + info).c_str()),
                  repository.loadMaxGearType(table))
            << info;

        // Every basic Info literal runs: the four GEAR_INFO_BASIC tables through the loop.
        if (table == GEAR_EVENT_ITEM || table == GEAR_EVENT_TREE || table == GEAR_LUCKY_BAG || table == GEAR_MOON_CARD)
            EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM " + info).c_str()),
                      (int)repository.loadBasicInfos(table).size())
                << info;
    }

    // The object-shape guard, both ways.
    EXPECT_THROW(repository.loadGearOfOwner(GEAR_EVENT_ITEM, "it-owner"), Error);
    EXPECT_THROW(repository.loadGunInZone(GEAR_MOON_CARD, 5, 31000), Error);
    EXPECT_THROW(repository.insertNumItem(GEAR_RING, 31900, 1, 1, "it-owner", 1, 1, 1, 1, 1, 1), Error);
    EXPECT_THROW(repository.updateNumItem(GEAR_AR, 1, 1, "it-owner", 1, 1, 1, 1, 1, 31000), Error);
    EXPECT_THROW(repository.loadNumItemOfOwner(GEAR_SWORD, "it-owner"), Error);
    EXPECT_THROW(repository.loadNumItemInZone(GEAR_SG, 5, 31000), Error);
    EXPECT_THROW(repository.insertGear(GEAR_EVENT_ITEM, 31900, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1), Error);

    // The six Info shapes: each pinned by COUNT(*) and, when the table is seeded, its
    // class-specific column; each refuses a table of another shape.
    {
        std::vector<BasicInfoRow> rows = repository.loadBasicInfos(GEAR_EVENT_ITEM);
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM EventItemInfo").c_str()), (int)rows.size());
        if (!rows.empty())
            EXPECT_EQ(std::to_string(rows[0].ratio), queryScalar("SELECT Ratio FROM EventItemInfo WHERE ItemType=" +
                                                                 std::to_string(rows[0].itemType)));
        EXPECT_THROW(repository.loadBasicInfos(GEAR_EVENT_ETC), Error);
    }
    {
        std::vector<FunctionInfoRow> rows = repository.loadFunctionInfos(GEAR_EVENT_ETC);
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM EventETCInfo").c_str()), (int)rows.size());
        if (!rows.empty())
            EXPECT_EQ(std::to_string(rows[0].function),
                      queryScalar("SELECT `Function` FROM EventETCInfo WHERE ItemType=" +
                                  std::to_string(rows[0].basic.itemType)));
        EXPECT_THROW(repository.loadFunctionInfos(GEAR_PET_ENCHANT_ITEM), Error);
    }
    {
        std::vector<ResurrectInfoRow> rows = repository.loadResurrectInfos(GEAR_RESURRECT_ITEM);
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM ResurrectItemInfo").c_str()), (int)rows.size());
        if (!rows.empty())
            EXPECT_EQ(std::to_string(rows[0].resurrectType),
                      queryScalar("SELECT ResurrectType FROM ResurrectItemInfo WHERE ItemType=" +
                                  std::to_string(rows[0].basic.itemType)));
        EXPECT_THROW(repository.loadResurrectInfos(GEAR_EVENT_ITEM), Error);
    }
    {
        std::vector<FunctionValueInfoRow> rows = repository.loadFunctionValueInfos(GEAR_DYE_POTION);
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM DyePotionInfo").c_str()), (int)rows.size());
        if (!rows.empty())
            EXPECT_EQ(std::to_string(rows[0].functionValue),
                      queryScalar("SELECT FunctionValue FROM DyePotionInfo WHERE ItemType=" +
                                  std::to_string(rows[0].basic.itemType)));
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM EventStarInfo").c_str()),
                  (int)repository.loadFunctionValueInfos(GEAR_EVENT_STAR).size());
        EXPECT_THROW(repository.loadFunctionValueInfos(GEAR_EFFECT_ITEM), Error);
    }
    {
        std::vector<EffectInfoRow> rows = repository.loadEffectInfos(GEAR_EFFECT_ITEM);
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM EffectItemInfo").c_str()), (int)rows.size());
        if (!rows.empty())
            EXPECT_EQ(std::to_string(rows[0].timeSec),
                      queryScalar("SELECT TimeSec FROM EffectItemInfo WHERE ItemType=" +
                                  std::to_string(rows[0].basic.itemType)));
        EXPECT_THROW(repository.loadEffectInfos(GEAR_DYE_POTION), Error);
    }
    {
        std::vector<FunctionGradeInfoRow> rows = repository.loadFunctionGradeInfos(GEAR_PET_ENCHANT_ITEM);
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM PetEnchantItemInfo").c_str()), (int)rows.size());
        if (!rows.empty())
            EXPECT_EQ(std::to_string(rows[0].functionGrade),
                      queryScalar("SELECT FunctionGrade FROM PetEnchantItemInfo WHERE ItemType=" +
                                  std::to_string(rows[0].basic.itemType)));
        EXPECT_THROW(repository.loadFunctionGradeInfos(GEAR_EVENT_ETC), Error);
    }
    EXPECT_THROW(repository.loadGunInfos(GEAR_EVENT_ITEM), Error);
    EXPECT_THROW(repository.loadGearInfos(GEAR_LUCKY_BAG), Error);
}

TEST_F(ItemObjectMySQL, NumOnlyItemRowsRoundTripThroughTheirEightColumns) {
    ItemObjectRepository& repository = defaultItemObjectRepository();

    for (size_t i = 0; i < sizeof(kNumOnlyTables) / sizeof(kNumOnlyTables[0]); i++) {
        const GearTable table = kNumOnlyTables[i].table;
        const std::string name = kNumOnlyTables[i].name;
        const std::string where = std::string(" FROM ") + name + " WHERE ItemID=";
        const std::string id = std::to_string(31000 + i);

        repository.insertNumOnlyItem(table, 31000 + i, 77, 3, "it-owner", 1, 5, 2, 4, 12);
        repository.insertNumOnlyItem(table, 31100 + i, 78, 3, "it-owner", 5, 31000, 7, 8, 30);
        EXPECT_EQ("12", queryScalar("SELECT Num" + where + id)) << name;
        EXPECT_EQ("4", queryScalar("SELECT Y" + where + id)) << name;
        repository.updateNumOnlyItem(table, 79, 4, "it-owner", 1, 6, 3, 5, 20, 31000 + i);
        EXPECT_EQ("20", queryScalar("SELECT Num" + where + id)) << name;
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + id)) << name;

        std::vector<NumOnlyObjectRow> owned = repository.loadNumOnlyItemOfOwner(table, "it-owner");
        ASSERT_EQ(1u, owned.size()) << name;
        EXPECT_EQ(31000 + i, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(4u, owned[0].itemType);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(3, owned[0].x);
        EXPECT_EQ(5, owned[0].y);
        EXPECT_EQ(20, owned[0].num) << name;

        std::vector<NumOnlyZoneObjectRow> inZone = repository.loadNumOnlyItemInZone(table, 5, 31000);
        ASSERT_EQ(1u, inZone.size()) << name;
        EXPECT_EQ((int)(31100 + i), inZone[0].itemID);
        EXPECT_EQ(7, inZone[0].x);
        EXPECT_EQ(30, inZone[0].num) << name;
        EXPECT_TRUE(repository.loadNumOnlyItemInZone(table, 5, 31001).empty()) << name;

        repository.tinysaveGear(table, "Num=9", 31000 + i);
        EXPECT_EQ("9", queryScalar("SELECT Num" + where + id)) << name;

        // The MAX(ItemType) literal of each Info table (all ten are seeded).
        const std::string info = name.substr(0, name.size() - 6) + "Info";
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM " + info).c_str()), repository.loadMaxGearType(table))
            << info;
    }

    // The object-shape guard, both ways: the gear INSERT and the Num + ItemFlag
    // methods refuse these tables; these methods refuse gear, silver, gun and Num tables.
    EXPECT_THROW(repository.loadGearOfOwner(GEAR_ETC, "it-owner"), Error);
    EXPECT_THROW(repository.loadNumItemOfOwner(GEAR_WATER, "it-owner"), Error);
    EXPECT_THROW(repository.insertNumItem(GEAR_SERUM, 31900, 1, 1, "it-owner", 1, 1, 1, 1, 1, 1), Error);
    EXPECT_THROW(repository.insertGear(GEAR_VAMPIRE_ETC, 31900, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1), Error);
    EXPECT_THROW(repository.insertNumOnlyItem(GEAR_EVENT_ITEM, 31900, 1, 1, "it-owner", 1, 1, 1, 1, 1), Error);
    EXPECT_THROW(repository.updateNumOnlyItem(GEAR_RING, 1, 1, "it-owner", 1, 1, 1, 1, 1, 31000), Error);
    EXPECT_THROW(repository.loadNumOnlyItemOfOwner(GEAR_SWORD, "it-owner"), Error);
    EXPECT_THROW(repository.loadNumOnlyItemInZone(GEAR_SG, 5, 31000), Error);

    // The two Info shapes: the basic one (ETC, Water) pinned by COUNT(*) and Ratio, the
    // basic-plus-string one (Serum, VampireETC) by COUNT(*) and its varchar column.
    {
        std::vector<BasicInfoRow> rows = repository.loadBasicInfos(GEAR_ETC);
        ASSERT_FALSE(rows.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM ETCInfo").c_str()), (int)rows.size());
        EXPECT_EQ(std::to_string(rows[0].ratio),
                  queryScalar("SELECT Ratio FROM ETCInfo WHERE ItemType=" + std::to_string(rows[0].itemType)));
        std::vector<BasicInfoRow> water = repository.loadBasicInfos(GEAR_WATER);
        ASSERT_FALSE(water.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM WaterInfo").c_str()), (int)water.size());
        EXPECT_EQ(std::to_string(water[0].ratio),
                  queryScalar("SELECT Ratio FROM WaterInfo WHERE ItemType=" + std::to_string(water[0].itemType)));
        EXPECT_THROW(repository.loadBasicInfos(GEAR_SERUM), Error);
    }
    {
        std::vector<StringInfoRow> rows = repository.loadStringInfos(GEAR_SERUM);
        ASSERT_FALSE(rows.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM SerumInfo").c_str()), (int)rows.size());
        EXPECT_EQ(rows[0].value, queryScalar("SELECT SerumEffect FROM SerumInfo WHERE ItemType=" +
                                             std::to_string(rows[0].basic.itemType)));
        std::vector<StringInfoRow> vampire = repository.loadStringInfos(GEAR_VAMPIRE_ETC);
        ASSERT_FALSE(vampire.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM VampireETCInfo").c_str()), (int)vampire.size());
        EXPECT_EQ(vampire[0].value, queryScalar("SELECT ReqAbility FROM VampireETCInfo WHERE ItemType=" +
                                                std::to_string(vampire[0].basic.itemType)));
        EXPECT_THROW(repository.loadStringInfos(GEAR_ETC), Error);
        EXPECT_THROW(repository.loadStringInfos(GEAR_EVENT_ETC), Error);
    }

    // The four classes with their own destroy(): the DELETE reports whether a row
    // went; a table without the literal is refused.
    {
        repository.insertNumOnlyItem(GEAR_PUPA, 31200, 1, 1, "it-owner", 1, 1, 1, 1, 1);
        EXPECT_TRUE(repository.destroyItemObject(GEAR_PUPA, "PupaObject", 31200));
        EXPECT_EQ("0", queryScalar("SELECT COUNT(*) FROM PupaObject WHERE ItemID=31200"));
        EXPECT_FALSE(repository.destroyItemObject(GEAR_PUPA, "PupaObject", 31200));
        EXPECT_FALSE(repository.destroyItemObject(GEAR_LARVA, "LarvaObject", 31200));
        EXPECT_FALSE(repository.destroyItemObject(GEAR_COMPOS_MEI, "ComposMeiObject", 31200));
        EXPECT_FALSE(repository.destroyItemObject(GEAR_POTION, "PotionObject", 31200));
        EXPECT_THROW(repository.destroyItemObject(GEAR_HOLY_WATER, "HolyWaterObject", 31200), Error);
        EXPECT_THROW(repository.destroyItemObject(GEAR_RING, "RingObject", 31200), Error);
    }

    // The Info shapes of the parameterized-create six, each pinned by COUNT(*) and
    // a class-specific column; each guard refusing another shape.
    {
        std::vector<DamageInfoRow> rows = repository.loadDamageInfos(GEAR_HOLY_WATER);
        ASSERT_FALSE(rows.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM HolyWaterInfo").c_str()), (int)rows.size());
        EXPECT_EQ(std::to_string(rows[0].maxDamage), queryScalar("SELECT maxDamage FROM HolyWaterInfo WHERE ItemType=" +
                                                                 std::to_string(rows[0].basic.itemType)));
        EXPECT_THROW(repository.loadDamageInfos(GEAR_MAGAZINE), Error);
    }
    {
        std::vector<MagazineInfoRow> rows = repository.loadMagazineInfos(GEAR_MAGAZINE);
        ASSERT_FALSE(rows.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM MagazineInfo").c_str()), (int)rows.size());
        const std::string type = std::to_string(rows[0].basic.itemType);
        EXPECT_EQ(std::to_string(rows[0].maxBullets),
                  queryScalar("SELECT MaxBullets FROM MagazineInfo WHERE ItemType=" + type));
        EXPECT_EQ(std::to_string(rows[0].gunType),
                  queryScalar("SELECT GunType-1 FROM MagazineInfo WHERE ItemType=" + type));
        EXPECT_THROW(repository.loadMagazineInfos(GEAR_POTION), Error);
    }
    {
        const GearTableName effectTables[] = {
            {GEAR_PUPA, "PupaInfo"}, {GEAR_LARVA, "LarvaInfo"}, {GEAR_COMPOS_MEI, "ComposMeiInfo"}};
        for (size_t i = 0; i < sizeof(effectTables) / sizeof(effectTables[0]); i++) {
            const std::string info = effectTables[i].name;
            std::vector<StringInfoRow> rows = repository.loadStringInfos(effectTables[i].table);
            ASSERT_FALSE(rows.empty()) << info;
            EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM " + info).c_str()), (int)rows.size()) << info;
            EXPECT_EQ(rows[0].value, queryScalar("SELECT Effect FROM " + info +
                                                 " WHERE ItemType=" + std::to_string(rows[0].basic.itemType)))
                << info;
        }
        std::vector<LevelStringInfoRow> potion = repository.loadLevelStringInfos(GEAR_POTION);
        ASSERT_FALSE(potion.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM PotionInfo").c_str()), (int)potion.size());
        const std::string type = std::to_string(potion[0].basic.itemType);
        EXPECT_EQ(std::to_string(potion[0].itemLevel),
                  queryScalar("SELECT ItemLevel FROM PotionInfo WHERE ItemType=" + type));
        EXPECT_EQ(potion[0].value, queryScalar("SELECT Effect FROM PotionInfo WHERE ItemType=" + type));
        EXPECT_THROW(repository.loadLevelStringInfos(GEAR_PUPA), Error);
        EXPECT_THROW(repository.loadStringInfos(GEAR_POTION), Error);
    }
}

TEST_F(ItemObjectMySQL, SkullAndBombTablesShareTheNumOnlyWritesButHaveTheirOwnZoneLoads) {
    ItemObjectRepository& repository = defaultItemObjectRepository();

    for (size_t i = 0; i < sizeof(kNumZoneVariantTables) / sizeof(kNumZoneVariantTables[0]); i++) {
        const GearTable table = kNumZoneVariantTables[i].table;
        const std::string name = kNumZoneVariantTables[i].name;
        const std::string where = std::string(" FROM ") + name + " WHERE ItemID=";
        const std::string id = std::to_string(31000 + i);

        repository.insertNumOnlyItem(table, 31000 + i, 77, 3, "it-owner", 1, 5, 2, 4, 12);
        repository.insertNumOnlyItem(table, 31100 + i, 78, 3, "it-owner", 5, 31000, 7, 8, 30);
        EXPECT_EQ("12", queryScalar("SELECT Num" + where + id)) << name;
        repository.updateNumOnlyItem(table, 79, 4, "it-owner", 1, 6, 3, 5, 20, 31000 + i);
        EXPECT_EQ("20", queryScalar("SELECT Num" + where + id)) << name;
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + id)) << name;

        std::vector<NumOnlyObjectRow> owned = repository.loadNumOnlyItemOfOwner(table, "it-owner");
        ASSERT_EQ(1u, owned.size()) << name;
        EXPECT_EQ(31000 + i, owned[0].itemID);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(20, owned[0].num) << name;

        repository.tinysaveGear(table, "Num=9", 31000 + i);
        EXPECT_EQ("9", queryScalar("SELECT Num" + where + id)) << name;

        // The zone load is the variant's own; the Num-only one refuses these tables.
        EXPECT_THROW(repository.loadNumOnlyItemInZone(table, 5, 31000), Error);
        if (table == GEAR_SKULL) {
            std::vector<SkullZoneObjectRow> inZone = repository.loadSkullInZone(table, 5, 31000);
            ASSERT_EQ(1u, inZone.size()) << name;
            EXPECT_EQ((int)(31100 + i), inZone[0].itemID);
            EXPECT_EQ(7, inZone[0].x);
            EXPECT_EQ(30u, inZone[0].num);
            EXPECT_TRUE(repository.loadSkullInZone(table, 5, 31001).empty());
            EXPECT_THROW(repository.loadBombInZone(table, 5, 31000), Error);
        } else {
            std::vector<BombZoneObjectRow> inZone = repository.loadBombInZone(table, 5, 31000);
            ASSERT_EQ(1u, inZone.size()) << name;
            EXPECT_EQ((int)(31100 + i), inZone[0].itemID);
            EXPECT_EQ(78, inZone[0].objectID);
            EXPECT_EQ(8, inZone[0].y);
            EXPECT_TRUE(repository.loadBombInZone(table, 5, 31001).empty()) << name;
            EXPECT_THROW(repository.loadSkullInZone(table, 5, 31000), Error) << name;
        }

        const std::string info = name.substr(0, name.size() - 6) + "Info";
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM " + info).c_str()), repository.loadMaxGearType(table))
            << info;
    }

    // The Num-only writes and owner load refuse the other shapes; the variant zone
    // loads refuse a plain Num-only table.
    EXPECT_THROW(repository.insertNumOnlyItem(GEAR_EVENT_ITEM, 31900, 1, 1, "it-owner", 1, 1, 1, 1, 1), Error);
    EXPECT_THROW(repository.loadSkullInZone(GEAR_ETC, 5, 31000), Error);
    EXPECT_THROW(repository.loadBombInZone(GEAR_WATER, 5, 31000), Error);
    EXPECT_THROW(repository.loadGearInZone(GEAR_BOMB, 5, 31000), Error);

    // Skull's Info shape (basic plus ItemLevel), Bomb's and Mine's damage shape,
    // BombMaterial's basic one; each guard refusing another shape.
    {
        std::vector<LevelInfoRow> rows = repository.loadLevelInfos(GEAR_SKULL);
        ASSERT_FALSE(rows.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM SkullInfo").c_str()), (int)rows.size());
        EXPECT_EQ(std::to_string(rows[0].itemLevel), queryScalar("SELECT ItemLevel FROM SkullInfo WHERE ItemType=" +
                                                                 std::to_string(rows[0].basic.itemType)));
        EXPECT_THROW(repository.loadLevelInfos(GEAR_POTION), Error);
        EXPECT_THROW(repository.loadLevelStringInfos(GEAR_SKULL), Error);
    }
    {
        const GearTableName damageTables[] = {{GEAR_BOMB, "BombInfo"}, {GEAR_MINE, "MineInfo"}};
        for (size_t i = 0; i < sizeof(damageTables) / sizeof(damageTables[0]); i++) {
            const std::string info = damageTables[i].name;
            std::vector<DamageInfoRow> rows = repository.loadDamageInfos(damageTables[i].table);
            ASSERT_FALSE(rows.empty()) << info;
            EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM " + info).c_str()), (int)rows.size()) << info;
            EXPECT_EQ(std::to_string(rows[0].maxDamage),
                      queryScalar("SELECT maxDamage FROM " + info +
                                  " WHERE ItemType=" + std::to_string(rows[0].basic.itemType)))
                << info;
        }
        std::vector<BasicInfoRow> material = repository.loadBasicInfos(GEAR_BOMB_MATERIAL);
        ASSERT_FALSE(material.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM BombMaterialInfo").c_str()), (int)material.size());
        EXPECT_EQ(std::to_string(material[0].ratio), queryScalar("SELECT Ratio FROM BombMaterialInfo WHERE ItemType=" +
                                                                 std::to_string(material[0].itemType)));
        EXPECT_THROW(repository.loadDamageInfos(GEAR_BOMB_MATERIAL), Error);
        EXPECT_THROW(repository.loadBasicInfos(GEAR_MINE), Error);
    }
}

TEST_F(ItemObjectMySQL, FlagAndPlainItemRowsRoundTripThroughTheirColumns) {
    ItemObjectRepository& repository = defaultItemObjectRepository();

    for (size_t i = 0; i < sizeof(kFlagTables) / sizeof(kFlagTables[0]); i++) {
        const GearTable table = kFlagTables[i].table;
        const std::string name = kFlagTables[i].name;
        const std::string where = std::string(" FROM ") + name + " WHERE ItemID=";
        const std::string id = std::to_string(31000 + i);

        repository.insertFlagItem(table, 31000 + i, 77, 3, "it-owner", 1, 5, 2, 4, 1);
        repository.insertFlagItem(table, 31100 + i, 78, 3, "it-owner", 5, 31000, 7, 8, 0);
        EXPECT_EQ("1", queryScalar("SELECT ItemFlag" + where + id)) << name;
        EXPECT_EQ("4", queryScalar("SELECT Y" + where + id)) << name;
        repository.updatePlainItem(table, 79, 4, "it-owner", 1, 6, 3, 5, 31000 + i);
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + id)) << name;
        EXPECT_EQ("1", queryScalar("SELECT ItemFlag" + where + id)) << name; // the UPDATE writes no ItemFlag

        std::vector<FlagObjectRow> owned = repository.loadFlagItemOfOwner(table, "it-owner");
        ASSERT_EQ(1u, owned.size()) << name;
        EXPECT_EQ(31000 + i, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(4u, owned[0].itemType);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(3, owned[0].x);
        EXPECT_EQ(5, owned[0].y);
        EXPECT_EQ(1, owned[0].createType);

        std::vector<FlagZoneObjectRow> inZone = repository.loadFlagItemInZone(table, 5, 31000);
        ASSERT_EQ(1u, inZone.size()) << name;
        EXPECT_EQ((int)(31100 + i), inZone[0].itemID);
        EXPECT_EQ(8, inZone[0].y);
        EXPECT_EQ(0, inZone[0].createType);
        EXPECT_TRUE(repository.loadFlagItemInZone(table, 5, 31001).empty()) << name;

        repository.tinysaveGear(table, "X=9", 31000 + i);
        EXPECT_EQ("9", queryScalar("SELECT X" + where + id)) << name;

        const std::string info = name.substr(0, name.size() - 6) + "Info";
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM " + info).c_str()), repository.loadMaxGearType(table))
            << info;
    }

    for (size_t i = 0; i < sizeof(kPlainTables) / sizeof(kPlainTables[0]); i++) {
        const GearTable table = kPlainTables[i].table;
        const std::string name = kPlainTables[i].name;
        const std::string where = std::string(" FROM ") + name + " WHERE ItemID=";
        const std::string id = std::to_string(31000 + i);

        repository.insertPlainItem(table, 31000 + i, 77, 3, "it-owner", 1, 5, 2, 4);
        repository.insertPlainItem(table, 31100 + i, 78, 3, "it-owner", 5, 31000, 7, 8);
        EXPECT_EQ("4", queryScalar("SELECT Y" + where + id)) << name;
        if (table != GEAR_LEARNING_ITEM) { // LearningItem's UPDATE literal says Storage=%s (kept as it was)
            repository.updatePlainItem(table, 79, 4, "it-owner", 1, 6, 3, 5, 31000 + i);
            EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + id)) << name;
        }

        std::vector<PlainObjectRow> owned = repository.loadPlainItemOfOwner(table, "it-owner");
        ASSERT_EQ(1u, owned.size()) << name;
        EXPECT_EQ(31000 + i, owned[0].itemID);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(table == GEAR_LEARNING_ITEM ? 2 : 3, owned[0].x) << name; // the UPDATE moved the others

        std::vector<PlainZoneObjectRow> inZone = repository.loadPlainItemInZone(table, 5, 31000);
        ASSERT_EQ(1u, inZone.size()) << name;
        EXPECT_EQ((int)(31100 + i), inZone[0].itemID);
        EXPECT_EQ(7, inZone[0].x);
        EXPECT_TRUE(repository.loadPlainItemInZone(table, 5, 31001).empty()) << name;

        repository.tinysaveGear(table, "X=9", 31000 + i);
        EXPECT_EQ("9", queryScalar("SELECT X" + where + id)) << name;

        const std::string info = name.substr(0, name.size() - 6) + "Info";
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM " + info).c_str()), repository.loadMaxGearType(table))
            << info;
    }

    // The object-shape guards, both ways.
    EXPECT_THROW(repository.insertFlagItem(GEAR_EVENT_GIFT_BOX, 31900, 1, 1, "it-owner", 1, 1, 1, 1, 1), Error);
    EXPECT_THROW(repository.insertPlainItem(GEAR_QUEST_ITEM, 31900, 1, 1, "it-owner", 1, 1, 1, 1), Error);
    EXPECT_THROW(repository.updatePlainItem(GEAR_ETC, 1, 1, "it-owner", 1, 1, 1, 1, 31000), Error);
    EXPECT_THROW(repository.loadFlagItemOfOwner(GEAR_LEARNING_ITEM, "it-owner"), Error);
    EXPECT_THROW(repository.loadPlainItemInZone(GEAR_TRAP_ITEM, 5, 31000), Error);
    EXPECT_THROW(repository.loadNumItemOfOwner(GEAR_SMSITEM, "it-owner"), Error);
    EXPECT_THROW(repository.loadNumOnlyItemInZone(GEAR_EVENT_GIFT_BOX, 5, 31000), Error);
    EXPECT_THROW(repository.insertGear(GEAR_SUB_INVENTORY, 31900, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1), Error);

    // The two Info shapes, each pinned by COUNT(*) and its class-specific column(s).
    {
        const GearTableName intTables[] = {{GEAR_QUEST_ITEM, "QuestItemInfo"},
                                           {GEAR_SMSITEM, "SMSItemInfo"},
                                           {GEAR_LEARNING_ITEM, "LearningItemInfo"}};
        const char* const intColumns[] = {"BonusRatio", "Charge", "SkillType"};
        for (size_t i = 0; i < sizeof(intTables) / sizeof(intTables[0]); i++) {
            const std::string info = intTables[i].name;
            std::vector<IntInfoRow> rows = repository.loadIntInfos(intTables[i].table);
            ASSERT_FALSE(rows.empty()) << info;
            EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM " + info).c_str()), (int)rows.size()) << info;
            EXPECT_EQ(std::to_string(rows[0].value),
                      queryScalar(std::string("SELECT ") + intColumns[i] + " FROM " + info +
                                  " WHERE ItemType=" + std::to_string(rows[0].basic.itemType)))
                << info;
        }
        EXPECT_THROW(repository.loadIntInfos(GEAR_TRAP_ITEM), Error);
    }
    {
        const GearTableName pairTables[] = {{GEAR_SUB_INVENTORY, "SubInventoryInfo"}, {GEAR_TRAP_ITEM, "TrapItemInfo"}};
        const char* const pairFirst[] = {"Width", "`Function`"};
        const char* const pairSecond[] = {"Height", "Parameter"};
        for (size_t i = 0; i < sizeof(pairTables) / sizeof(pairTables[0]); i++) {
            const std::string info = pairTables[i].name;
            std::vector<IntPairInfoRow> rows = repository.loadIntPairInfos(pairTables[i].table);
            ASSERT_FALSE(rows.empty()) << info;
            EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM " + info).c_str()), (int)rows.size()) << info;
            const std::string type = " FROM " + info + " WHERE ItemType=" + std::to_string(rows[0].basic.itemType);
            EXPECT_EQ(std::to_string(rows[0].first), queryScalar(std::string("SELECT ") + pairFirst[i] + type)) << info;
            EXPECT_EQ(std::to_string(rows[0].second), queryScalar(std::string("SELECT ") + pairSecond[i] + type))
                << info;
        }
        EXPECT_THROW(repository.loadIntPairInfos(GEAR_QUEST_ITEM), Error);
        std::vector<BasicInfoRow> box = repository.loadBasicInfos(GEAR_EVENT_GIFT_BOX);
        ASSERT_FALSE(box.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM EventGiftBoxInfo").c_str()), (int)box.size());
        EXPECT_THROW(repository.loadBasicInfos(GEAR_SMSITEM), Error);
    }
}

TEST_F(ItemObjectMySQL, NumIntKeyAndChargeItemRowsRoundTripThroughTheirColumns) {
    ItemObjectRepository& repository = defaultItemObjectRepository();

    // MixingItem and PetFood: the Num + ItemFlag writes; Num read back through getInt.
    for (size_t i = 0; i < sizeof(kNumIntTables) / sizeof(kNumIntTables[0]); i++) {
        const GearTable table = kNumIntTables[i].table;
        const std::string name = kNumIntTables[i].name;
        const std::string where = std::string(" FROM ") + name + " WHERE ItemID=";
        const std::string id = std::to_string(31000 + i);

        repository.insertNumItem(table, 31000 + i, 77, 3, "it-owner", 1, 5, 2, 4, 12, 1);
        repository.insertNumItem(table, 31100 + i, 78, 3, "it-owner", 5, 31000, 7, 8, 30, 0);
        EXPECT_EQ("12", queryScalar("SELECT Num" + where + id)) << name;
        EXPECT_EQ("1", queryScalar("SELECT ItemFlag" + where + id)) << name;
        repository.updateNumItem(table, 79, 4, "it-owner", 1, 6, 3, 5, 20, 31000 + i);
        EXPECT_EQ("20", queryScalar("SELECT Num" + where + id)) << name;
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + id)) << name;
        EXPECT_EQ("1", queryScalar("SELECT ItemFlag" + where + id)) << name; // the UPDATE writes no ItemFlag

        std::vector<NumIntObjectRow> owned = repository.loadNumIntItemOfOwner(table, "it-owner");
        ASSERT_EQ(1u, owned.size()) << name;
        EXPECT_EQ(31000 + i, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(4u, owned[0].itemType);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(3, owned[0].x);
        EXPECT_EQ(5, owned[0].y);
        EXPECT_EQ(20, owned[0].num);
        EXPECT_EQ(1, owned[0].createType);

        if (table == GEAR_MIXING_ITEM) {
            std::vector<NumIntZoneObjectRow> inZone = repository.loadNumIntItemInZone(table, 5, 31000);
            ASSERT_EQ(1u, inZone.size()) << name;
            EXPECT_EQ((int)(31100 + i), inZone[0].itemID);
            EXPECT_EQ(8, inZone[0].y);
            EXPECT_EQ(30, inZone[0].num);
            EXPECT_EQ(0, inZone[0].createType);
            EXPECT_TRUE(repository.loadNumIntItemInZone(table, 5, 31001).empty());
            EXPECT_THROW(repository.loadFlagItemInZone(table, 5, 31000), Error);
        } else { // PetFood's zone SELECT names no Num column: the ItemFlag-only zone shape serves it
            std::vector<FlagZoneObjectRow> inZone = repository.loadFlagItemInZone(table, 5, 31000);
            ASSERT_EQ(1u, inZone.size()) << name;
            EXPECT_EQ((int)(31100 + i), inZone[0].itemID);
            EXPECT_EQ(8, inZone[0].y);
            EXPECT_EQ(0, inZone[0].createType);
            EXPECT_TRUE(repository.loadFlagItemInZone(table, 5, 31001).empty());
            EXPECT_THROW(repository.loadNumIntItemInZone(table, 5, 31000), Error);
        }

        repository.tinysaveGear(table, "Num=9", 31000 + i);
        EXPECT_EQ("9", queryScalar("SELECT Num" + where + id)) << name;

        const std::string info = name.substr(0, name.size() - 6) + "Info";
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM " + info).c_str()), repository.loadMaxGearType(table))
            << info;
    }

    // Key: a Target column in the INSERT, the UPDATE and both loads.
    {
        const std::string where = " FROM KeyObject WHERE ItemID=";
        repository.insertKey(GEAR_KEY, 31000, 77, 3, "it-owner", 1, 5, 2, 4, 2000000001u);
        repository.insertKey(GEAR_KEY, 31100, 78, 3, "it-owner", 5, 31000, 7, 8, 2000000002u);
        EXPECT_EQ("2000000001", queryScalar("SELECT Target" + where + "31000"));
        repository.updateKey(GEAR_KEY, 79, 4, "it-owner", 1, 6, 3, 5, 2000000003u, 31000);
        EXPECT_EQ("2000000003", queryScalar("SELECT Target" + where + "31000"));
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + "31000"));

        std::vector<KeyObjectRow> owned = repository.loadKeyOfOwner(GEAR_KEY, "it-owner");
        ASSERT_EQ(1u, owned.size());
        EXPECT_EQ(31000u, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(4u, owned[0].itemType);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(3, owned[0].x);
        EXPECT_EQ(5, owned[0].y);
        EXPECT_EQ(2000000003u, owned[0].target);

        std::vector<KeyZoneObjectRow> inZone = repository.loadKeyInZone(GEAR_KEY, 5, 31000);
        ASSERT_EQ(1u, inZone.size());
        EXPECT_EQ(31100, inZone[0].itemID);
        EXPECT_EQ(7, inZone[0].x);
        EXPECT_EQ(2000000002u, inZone[0].target);
        EXPECT_TRUE(repository.loadKeyInZone(GEAR_KEY, 5, 31001).empty());

        repository.tinysaveGear(GEAR_KEY, "X=9", 31000);
        EXPECT_EQ("9", queryScalar("SELECT X" + where + "31000"));
        repository.saveKeyTarget(GEAR_KEY, 2000000004u, 31000);
        EXPECT_EQ("2000000004", queryScalar("SELECT Target" + where + "31000"));
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + "31000")); // the Target UPDATE touches nothing else
        EXPECT_THROW(repository.saveKeyTarget(GEAR_MIXING_ITEM, 1, 31000), Error);
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM KeyInfo").c_str()), repository.loadMaxGearType(GEAR_KEY));
    }

    // OustersSummonItem and SlayerPortalItem: a Charge column; both loads read the same getters.
    for (size_t i = 0; i < sizeof(kChargeTables) / sizeof(kChargeTables[0]); i++) {
        const GearTable table = kChargeTables[i].table;
        const std::string name = kChargeTables[i].name;
        const std::string where = std::string(" FROM ") + name + " WHERE ItemID=";
        const std::string id = std::to_string(31000 + i);

        repository.insertChargeItem(table, 31000 + i, 77, 3, "it-owner", 1, 5, 2, 4, 12);
        repository.insertChargeItem(table, 31100 + i, 78, 3, "it-owner", 5, 31000, 7, 8, 30);
        EXPECT_EQ("12", queryScalar("SELECT Charge" + where + id)) << name;
        repository.updateChargeItem(table, 79, 4, "it-owner", 1, 6, 3, 5, 20, 31000 + i);
        EXPECT_EQ("20", queryScalar("SELECT Charge" + where + id)) << name;
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + id)) << name;

        std::vector<ChargeObjectRow> owned = repository.loadChargeItemOfOwner(table, "it-owner");
        ASSERT_EQ(1u, owned.size()) << name;
        EXPECT_EQ(31000 + i, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(4u, owned[0].itemType);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(3, owned[0].x);
        EXPECT_EQ(5, owned[0].y);
        EXPECT_EQ(20, owned[0].charge);

        std::vector<ChargeObjectRow> inZone = repository.loadChargeItemInZone(table, 5, 31000);
        ASSERT_EQ(1u, inZone.size()) << name;
        EXPECT_EQ(31100 + i, inZone[0].itemID);
        EXPECT_EQ(31000u, inZone[0].storageID);
        EXPECT_EQ(7, inZone[0].x);
        EXPECT_EQ(8, inZone[0].y);
        EXPECT_EQ(30, inZone[0].charge);
        EXPECT_TRUE(repository.loadChargeItemInZone(table, 5, 31001).empty()) << name;

        repository.tinysaveGear(table, "Charge=9", 31000 + i);
        EXPECT_EQ("9", queryScalar("SELECT Charge" + where + id)) << name;

        const std::string info = name.substr(0, name.size() - 6) + "Info";
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM " + info).c_str()), repository.loadMaxGearType(table))
            << info;
    }

    // The object-shape guards, both ways.
    EXPECT_THROW(repository.loadNumItemOfOwner(GEAR_MIXING_ITEM, "it-owner"),
                 Error); // the getBYTE loads refuse the getInt tables
    EXPECT_THROW(repository.loadNumItemInZone(GEAR_PET_FOOD, 5, 31000), Error);
    EXPECT_THROW(repository.loadNumIntItemOfOwner(GEAR_EVENT_ITEM, "it-owner"), Error);
    EXPECT_THROW(repository.loadNumIntItemInZone(GEAR_EVENT_ITEM, 5, 31000), Error);
    EXPECT_THROW(repository.insertNumItem(GEAR_KEY, 31900, 1, 1, "it-owner", 1, 1, 1, 1, 1, 1), Error);
    EXPECT_THROW(repository.updateNumItem(GEAR_OUSTERS_SUMMON_ITEM, 1, 1, "it-owner", 1, 1, 1, 1, 1, 31000), Error);
    EXPECT_THROW(repository.insertKey(GEAR_MIXING_ITEM, 31900, 1, 1, "it-owner", 1, 1, 1, 1, 1), Error);
    EXPECT_THROW(repository.updateKey(GEAR_SLAYER_PORTAL_ITEM, 1, 1, "it-owner", 1, 1, 1, 1, 1, 31000), Error);
    EXPECT_THROW(repository.loadKeyOfOwner(GEAR_OUSTERS_SUMMON_ITEM, "it-owner"), Error);
    EXPECT_THROW(repository.loadKeyInZone(GEAR_PET_FOOD, 5, 31000), Error);
    EXPECT_THROW(repository.insertChargeItem(GEAR_KEY, 31900, 1, 1, "it-owner", 1, 1, 1, 1, 1), Error);
    EXPECT_THROW(repository.updateChargeItem(GEAR_PET_FOOD, 1, 1, "it-owner", 1, 1, 1, 1, 1, 31000), Error);
    EXPECT_THROW(repository.loadChargeItemOfOwner(GEAR_KEY, "it-owner"), Error);
    EXPECT_THROW(repository.loadChargeItemInZone(GEAR_MIXING_ITEM, 5, 31000), Error);
    EXPECT_THROW(repository.loadFlagItemInZone(GEAR_KEY, 5, 31000), Error);
    EXPECT_THROW(repository.loadPlainItemInZone(GEAR_KEY, 5, 31000), Error);
    EXPECT_THROW(repository.updatePlainItem(GEAR_SLAYER_PORTAL_ITEM, 1, 1, "it-owner", 1, 1, 1, 1, 31000), Error);
    EXPECT_THROW(repository.insertGear(GEAR_OUSTERS_SUMMON_ITEM, 31900, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1),
                 Error);

    // The Info shapes, each pinned by COUNT(*) and its class-specific columns.
    {
        std::vector<MixingItemInfoRow> rows = repository.loadMixingItemInfos(GEAR_MIXING_ITEM);
        ASSERT_FALSE(rows.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM MixingItemInfo").c_str()), (int)rows.size());
        const std::string type = " FROM MixingItemInfo WHERE ItemType=" + std::to_string(rows[0].head.itemType);
        EXPECT_EQ(rows[0].head.name, queryScalar("SELECT Name" + type));
        EXPECT_EQ(std::to_string(rows[0].target), queryScalar("SELECT Target-1" + type));
        EXPECT_EQ(std::to_string(rows[0].type), queryScalar("SELECT Type-1" + type));
        EXPECT_EQ(std::to_string(rows[0].oustersLevel), queryScalar("SELECT OustersLevel" + type));
        EXPECT_THROW(repository.loadMixingItemInfos(GEAR_PET_FOOD), Error);
        EXPECT_THROW(repository.loadBasicInfos(GEAR_MIXING_ITEM), Error);
    }
    {
        std::vector<IntTripleInfoRow> rows = repository.loadIntTripleInfos(GEAR_PET_FOOD);
        ASSERT_FALSE(rows.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM PetFoodInfo").c_str()), (int)rows.size());
        const std::string type = " FROM PetFoodInfo WHERE ItemType=" + std::to_string(rows[0].basic.itemType);
        EXPECT_EQ(std::to_string(rows[0].first), queryScalar("SELECT Target" + type));
        EXPECT_EQ(std::to_string(rows[0].second), queryScalar("SELECT PetHP" + type));
        EXPECT_EQ(std::to_string(rows[0].third), queryScalar("SELECT TameRatio" + type));
        EXPECT_THROW(repository.loadIntTripleInfos(GEAR_KEY), Error);
        EXPECT_THROW(repository.loadIntPairInfos(GEAR_PET_FOOD), Error);
    }
    {
        std::vector<IntPairInfoRow> rows = repository.loadIntPairInfos(GEAR_KEY);
        ASSERT_FALSE(rows.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM KeyInfo").c_str()), (int)rows.size());
        const std::string type = " FROM KeyInfo WHERE ItemType=" + std::to_string(rows[0].basic.itemType);
        EXPECT_EQ(std::to_string(rows[0].first), queryScalar("SELECT OptionType" + type));
        EXPECT_EQ(std::to_string(rows[0].second), queryScalar("SELECT TargetType" + type));
        EXPECT_THROW(repository.loadIntInfos(GEAR_KEY), Error);
    }
    {
        std::vector<SummonItemInfoRow> rows = repository.loadSummonItemInfos(GEAR_OUSTERS_SUMMON_ITEM);
        ASSERT_FALSE(rows.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM OustersSummonItemInfo").c_str()), (int)rows.size());
        const std::string type = " FROM OustersSummonItemInfo WHERE ItemType=" + std::to_string(rows[0].head.itemType);
        EXPECT_EQ(std::to_string(rows[0].maxCharge), queryScalar("SELECT MaxCharge" + type));
        EXPECT_EQ(std::to_string(rows[0].effectID), queryScalar("SELECT Effect" + type));
        EXPECT_THROW(repository.loadSummonItemInfos(GEAR_SLAYER_PORTAL_ITEM), Error);
        EXPECT_THROW(repository.loadLevelStringInfos(GEAR_OUSTERS_SUMMON_ITEM), Error);
    }
    {
        std::vector<LevelStringInfoRow> rows = repository.loadLevelStringInfos(GEAR_SLAYER_PORTAL_ITEM);
        ASSERT_FALSE(rows.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM SlayerPortalItemInfo").c_str()), (int)rows.size());
        const std::string type = " FROM SlayerPortalItemInfo WHERE ItemType=" + std::to_string(rows[0].basic.itemType);
        EXPECT_EQ(std::to_string(rows[0].itemLevel), queryScalar("SELECT MaxCharge" + type)); // the shape's int
        EXPECT_EQ(rows[0].value, queryScalar("SELECT ReqAbility" + type));
        EXPECT_THROW(repository.loadIntTripleInfos(GEAR_SLAYER_PORTAL_ITEM), Error);
    }
}

TEST_F(ItemObjectMySQL, MoneyCoupleRingAndVampirePortalRowsRoundTripThroughTheirColumns) {
    ItemObjectRepository& repository = defaultItemObjectRepository();

    // Money: Amount and Num in the INSERT and UPDATE, Amount in the tinysave too; the
    // owner load reads both, the zone load Amount alone.
    {
        const std::string where = " FROM MoneyObject WHERE ItemID=";
        repository.insertMoney(GEAR_MONEY, 31000, 77, 3, "it-owner", 1, 5, 2, 4, 123456u, 1);
        repository.insertMoney(GEAR_MONEY, 31100, 78, 3, "it-owner", 5, 31000, 7, 8, 654321u, 2);
        EXPECT_EQ("123456", queryScalar("SELECT Amount" + where + "31000"));
        EXPECT_EQ("1", queryScalar("SELECT Num" + where + "31000"));
        repository.updateMoney(GEAR_MONEY, 79, 4, "it-owner", 1, 6, 3, 5, 222u, 3, 31000);
        EXPECT_EQ("222", queryScalar("SELECT Amount" + where + "31000"));
        EXPECT_EQ("3", queryScalar("SELECT Num" + where + "31000"));
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + "31000"));

        std::vector<MoneyObjectRow> owned = repository.loadMoneyOfOwner(GEAR_MONEY, "it-owner");
        ASSERT_EQ(1u, owned.size());
        EXPECT_EQ(31000u, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(4u, owned[0].itemType);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(3, owned[0].x);
        EXPECT_EQ(5, owned[0].y);
        EXPECT_EQ(222u, owned[0].amount);
        EXPECT_EQ(3, owned[0].num);

        std::vector<MoneyZoneObjectRow> inZone = repository.loadMoneyInZone(GEAR_MONEY, 5, 31000);
        ASSERT_EQ(1u, inZone.size());
        EXPECT_EQ(31100, inZone[0].itemID);
        EXPECT_EQ(8, inZone[0].y);
        EXPECT_EQ(654321u, inZone[0].amount);
        EXPECT_TRUE(repository.loadMoneyInZone(GEAR_MONEY, 5, 31001).empty());

        repository.tinysaveMoney(GEAR_MONEY, "X=9", 333u, 31000);
        EXPECT_EQ("9", queryScalar("SELECT X" + where + "31000"));
        EXPECT_EQ("333", queryScalar("SELECT Amount" + where + "31000")); // the tinysave literal writes Amount too
        EXPECT_THROW(repository.tinysaveGear(GEAR_MONEY, "X=1", 31000), Error);
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM MoneyInfo").c_str()),
                  repository.loadMaxGearType(GEAR_MONEY));
    }

    // The two couple rings: OptionType, Name and PartnerItemID in the INSERT, Name and
    // PartnerItemID in the UPDATE, the ten-column owner load, the plain zone load, and
    // the partner count.
    for (size_t i = 0; i < sizeof(kCoupleRingTables) / sizeof(kCoupleRingTables[0]); i++) {
        const GearTable table = kCoupleRingTables[i].table;
        const std::string name = kCoupleRingTables[i].name;
        const std::string where = std::string(" FROM ") + name + " WHERE ItemID=";
        const std::string id = std::to_string(31000 + i);

        repository.insertCoupleRing(table, 31000 + i, 77, 3, "it-owner", 1, 5, 2, 4, "1,2", "partner", 31100 + i);
        repository.insertCoupleRing(table, 31100 + i, 78, 3, "it-owner", 5, 31000, 7, 8, "", "owner", 31000 + i);
        EXPECT_EQ("1,2", queryScalar("SELECT OptionType" + where + id)) << name;
        EXPECT_EQ("partner", queryScalar("SELECT Name" + where + id)) << name;
        EXPECT_EQ(std::to_string(31100 + i), queryScalar("SELECT PartnerItemID" + where + id)) << name;
        repository.updateCoupleRing(table, 79, 4, "it-owner", 1, 6, 3, 5, "partner2", 31900 + i, 31000 + i);
        EXPECT_EQ("partner2", queryScalar("SELECT Name" + where + id)) << name;
        EXPECT_EQ(std::to_string(31900 + i), queryScalar("SELECT PartnerItemID" + where + id)) << name;
        EXPECT_EQ("1,2", queryScalar("SELECT OptionType" + where + id)) << name; // the UPDATE writes no OptionType
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + id)) << name;

        std::vector<CoupleRingObjectRow> owned = repository.loadCoupleRingOfOwner(table, "it-owner");
        ASSERT_EQ(1u, owned.size()) << name;
        EXPECT_EQ(31000 + i, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(4u, owned[0].itemType);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(3, owned[0].x);
        EXPECT_EQ(5, owned[0].y);
        EXPECT_EQ("1,2", owned[0].optionField);
        EXPECT_EQ("partner2", owned[0].name);
        EXPECT_EQ(31900 + i, owned[0].partnerItemID);

        std::vector<PlainZoneObjectRow> inZone = repository.loadPlainItemInZone(table, 5, 31000);
        ASSERT_EQ(1u, inZone.size()) << name;
        EXPECT_EQ((int)(31100 + i), inZone[0].itemID);
        EXPECT_EQ(7, inZone[0].x);
        EXPECT_TRUE(repository.loadPlainItemInZone(table, 5, 31001).empty()) << name;

        // count(*) of the partner's row in an owner's storage: the storage-1 ring counts,
        // the zone ring (Storage 5) does not, and an unknown id reports a row of zero.
        int count = -1;
        EXPECT_TRUE(repository.loadCoupleRingPartnerCount(table, 31000 + i, count)) << name;
        EXPECT_EQ(1, count) << name;
        count = -1;
        EXPECT_TRUE(repository.loadCoupleRingPartnerCount(table, 31100 + i, count)) << name;
        EXPECT_EQ(0, count) << name;
        count = -1;
        EXPECT_TRUE(repository.loadCoupleRingPartnerCount(table, 31999, count)) << name;
        EXPECT_EQ(0, count) << name;

        repository.tinysaveGear(table, "X=9", 31000 + i);
        EXPECT_EQ("9", queryScalar("SELECT X" + where + id)) << name;

        const std::string info = name.substr(0, name.size() - 6) + "Info";
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM " + info).c_str()), repository.loadMaxGearType(table))
            << info;
    }

    // VampirePortalItem: Charge and the three targets in the INSERT, the UPDATE and the
    // owner load; the zone load over-reads its eight-column SELECT and throws on a row.
    {
        const std::string where = " FROM VampirePortalItemObject WHERE ItemID=";
        repository.insertVampirePortal(GEAR_VAMPIRE_PORTAL_ITEM, 31000, 77, 3, "it-owner", 1, 5, 2, 4, 12, 40, 100,
                                       200);
        repository.insertVampirePortal(GEAR_VAMPIRE_PORTAL_ITEM, 31100, 78, 3, "it-owner", 5, 31000, 7, 8, 30, 41, 101,
                                       201);
        EXPECT_EQ("12", queryScalar("SELECT Charge" + where + "31000"));
        EXPECT_EQ("200", queryScalar("SELECT TargetY" + where + "31000"));
        repository.updateVampirePortal(GEAR_VAMPIRE_PORTAL_ITEM, 79, 4, "it-owner", 1, 6, 3, 5, 20, 42, 102, 202,
                                       31000);
        EXPECT_EQ("20", queryScalar("SELECT Charge" + where + "31000"));
        EXPECT_EQ("42", queryScalar("SELECT TargetZID" + where + "31000"));
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + "31000"));

        std::vector<VampirePortalObjectRow> owned =
            repository.loadVampirePortalOfOwner(GEAR_VAMPIRE_PORTAL_ITEM, "it-owner");
        ASSERT_EQ(1u, owned.size());
        EXPECT_EQ(31000u, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(4u, owned[0].itemType);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(3, owned[0].x);
        EXPECT_EQ(5, owned[0].y);
        EXPECT_EQ(20, owned[0].charge);
        EXPECT_EQ(42, owned[0].targetZoneID);
        EXPECT_EQ(102, owned[0].targetX);
        EXPECT_EQ(202, owned[0].targetY);

        EXPECT_THROW(repository.loadVampirePortalInZone(GEAR_VAMPIRE_PORTAL_ITEM, 5, 31000), OutOfBoundException);
        EXPECT_TRUE(repository.loadVampirePortalInZone(GEAR_VAMPIRE_PORTAL_ITEM, 5, 31001).empty());

        repository.tinysaveGear(GEAR_VAMPIRE_PORTAL_ITEM, "X=9", 31000);
        EXPECT_EQ("9", queryScalar("SELECT X" + where + "31000"));
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM VampirePortalItemInfo").c_str()),
                  repository.loadMaxGearType(GEAR_VAMPIRE_PORTAL_ITEM));
    }

    // The object-shape guards, both ways.
    EXPECT_THROW(repository.tinysaveMoney(GEAR_COUPLE_RING, "X=1", 1u, 31000), Error);
    EXPECT_THROW(repository.tinysaveMoney(GEAR_SG, "X=1", 1u, 31000), Error);
    EXPECT_THROW(repository.tinysaveGun(GEAR_MONEY, "X=1", 1, 31000), Error);
    EXPECT_THROW(repository.insertMoney(GEAR_COUPLE_RING, 31900, 1, 1, "it-owner", 1, 1, 1, 1, 1u, 1), Error);
    EXPECT_THROW(repository.updateMoney(GEAR_VAMPIRE_PORTAL_ITEM, 1, 1, "it-owner", 1, 1, 1, 1, 1u, 1, 31000), Error);
    EXPECT_THROW(repository.loadMoneyOfOwner(GEAR_KEY, "it-owner"), Error);
    EXPECT_THROW(repository.loadMoneyInZone(GEAR_VAMPIRE_COUPLE_RING, 5, 31000), Error);
    EXPECT_THROW(repository.insertCoupleRing(GEAR_MONEY, 31900, 1, 1, "it-owner", 1, 1, 1, 1, "", "", 1), Error);
    EXPECT_THROW(repository.updateCoupleRing(GEAR_VAMPIRE_PORTAL_ITEM, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 31000),
                 Error);
    EXPECT_THROW(repository.loadCoupleRingOfOwner(GEAR_MONEY, "it-owner"), Error);
    {
        int count = -1;
        EXPECT_THROW(repository.loadCoupleRingPartnerCount(GEAR_MONEY, 1, count), Error);
        EXPECT_THROW(repository.loadCoupleRingPartnerCount(GEAR_EVENT_GIFT_BOX, 1, count), Error);
    }
    EXPECT_THROW(repository.loadPlainItemInZone(GEAR_MONEY, 5, 31000), Error);
    EXPECT_THROW(repository.loadPlainItemOfOwner(GEAR_COUPLE_RING, "it-owner"), Error);
    EXPECT_THROW(repository.insertVampirePortal(GEAR_MONEY, 31900, 1, 1, "it-owner", 1, 1, 1, 1, 1, 1, 1, 1), Error);
    EXPECT_THROW(repository.updateVampirePortal(GEAR_COUPLE_RING, 1, 1, "it-owner", 1, 1, 1, 1, 1, 1, 1, 1, 31000),
                 Error);
    EXPECT_THROW(repository.loadVampirePortalOfOwner(GEAR_SLAYER_PORTAL_ITEM, "it-owner"), Error);
    EXPECT_THROW(repository.loadVampirePortalInZone(GEAR_OUSTERS_SUMMON_ITEM, 5, 31000), Error);
    EXPECT_THROW(repository.loadChargeItemOfOwner(GEAR_VAMPIRE_PORTAL_ITEM, "it-owner"), Error);
    EXPECT_THROW(repository.loadNumOnlyItemOfOwner(GEAR_MONEY, "it-owner"), Error);
    EXPECT_THROW(repository.insertGear(GEAR_MONEY, 31900, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1), Error);

    // The Info shapes: basic for Money and the couple rings, the Potion shape for the portal.
    {
        const GearTableName basicTables[] = {{GEAR_MONEY, "MoneyInfo"},
                                             {GEAR_COUPLE_RING, "CoupleRingInfo"},
                                             {GEAR_VAMPIRE_COUPLE_RING, "VampireCoupleRingInfo"}};
        for (size_t i = 0; i < sizeof(basicTables) / sizeof(basicTables[0]); i++) {
            const std::string info = basicTables[i].name;
            std::vector<BasicInfoRow> rows = repository.loadBasicInfos(basicTables[i].table);
            ASSERT_FALSE(rows.empty()) << info;
            EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM " + info).c_str()), (int)rows.size()) << info;
            EXPECT_EQ(std::to_string(rows[0].ratio),
                      queryScalar("SELECT Ratio FROM " + info + " WHERE ItemType=" + std::to_string(rows[0].itemType)))
                << info;
        }
        EXPECT_THROW(repository.loadLevelStringInfos(GEAR_MONEY), Error);
        std::vector<LevelStringInfoRow> rows = repository.loadLevelStringInfos(GEAR_VAMPIRE_PORTAL_ITEM);
        ASSERT_FALSE(rows.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM VampirePortalItemInfo").c_str()), (int)rows.size());
        const std::string type = " FROM VampirePortalItemInfo WHERE ItemType=" + std::to_string(rows[0].basic.itemType);
        EXPECT_EQ(std::to_string(rows[0].itemLevel), queryScalar("SELECT MaxCharge" + type)); // the shape's int
        EXPECT_EQ(rows[0].value, queryScalar("SELECT ReqAbility" + type));
        EXPECT_THROW(repository.loadBasicInfos(GEAR_VAMPIRE_PORTAL_ITEM), Error);
    }
}

TEST_F(ItemObjectMySQL, AmuletCoreZapAndPocketRowsRoundTripThroughTheirColumns) {
    ItemObjectRepository& repository = defaultItemObjectRepository();

    // VampireAmulet: gear's two loads over an INSERT and an UPDATE that name no Durability.
    {
        const std::string where = " FROM VampireAmuletObject WHERE ItemID=";
        repository.insertOptionGradeItem(GEAR_VAMPIRE_AMULET, 31000, 77, 3, "it-owner", 1, 5, 2, 4, "1,2", 3, 1);
        repository.insertOptionGradeItem(GEAR_VAMPIRE_AMULET, 31100, 78, 3, "it-owner", 5, 31000, 7, 8, "", 2, 0);
        EXPECT_EQ("1,2", queryScalar("SELECT OptionType" + where + "31000"));
        EXPECT_EQ("3", queryScalar("SELECT Grade" + where + "31000"));
        EXPECT_EQ("0", queryScalar("SELECT Durability" + where + "31000")); // the INSERT names none: the column default
        repository.updateAmulet(GEAR_VAMPIRE_AMULET, 79, 4, "it-owner", 1, 6, 3, 5, "2,3", 4, 7, 31000);
        EXPECT_EQ("4", queryScalar("SELECT Grade" + where + "31000"));
        EXPECT_EQ("7", queryScalar("SELECT EnchantLevel" + where + "31000"));
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + "31000"));
        EXPECT_EQ("0", queryScalar("SELECT Durability" + where + "31000")); // the UPDATE names none either

        std::vector<GearObjectRow> owned = repository.loadGearOfOwner(GEAR_VAMPIRE_AMULET, "it-owner");
        ASSERT_EQ(1u, owned.size());
        EXPECT_EQ(31000u, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ("2,3", owned[0].optionField);
        EXPECT_EQ(0, owned[0].durability);
        EXPECT_EQ(4, owned[0].grade);
        EXPECT_EQ(7, owned[0].enchantLevel);
        EXPECT_EQ(1, owned[0].createType);

        std::vector<GearZoneObjectRow> inZone = repository.loadGearInZone(GEAR_VAMPIRE_AMULET, 5, 31000);
        ASSERT_EQ(1u, inZone.size());
        EXPECT_EQ(31100, inZone[0].itemID);
        EXPECT_EQ("", inZone[0].optionField);
        EXPECT_EQ(0, inZone[0].enchantLevel);
        EXPECT_EQ(0, inZone[0].createType);
        EXPECT_TRUE(repository.loadGearInZone(GEAR_VAMPIRE_AMULET, 5, 31001).empty());

        repository.tinysaveGear(GEAR_VAMPIRE_AMULET, "X=9", 31000);
        EXPECT_EQ("9", queryScalar("SELECT X" + where + "31000"));
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM VampireAmuletInfo").c_str()),
                  repository.loadMaxGearType(GEAR_VAMPIRE_AMULET));
        std::vector<GearInfoRow> infos = repository.loadGearInfos(GEAR_VAMPIRE_AMULET);
        ASSERT_FALSE(infos.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM VampireAmuletInfo").c_str()), (int)infos.size());
    }

    // CoreZap: the same INSERT, an UPDATE with Grade alone, its own two loads, basic plus OptionClass.
    {
        const std::string where = " FROM CoreZapObject WHERE ItemID=";
        repository.insertOptionGradeItem(GEAR_CORE_ZAP, 31000, 77, 3, "it-owner", 1, 5, 2, 4, "1,2", 3, 1);
        repository.insertOptionGradeItem(GEAR_CORE_ZAP, 31100, 78, 3, "it-owner", 5, 31000, 7, 8, "", 2, 0);
        EXPECT_EQ("3", queryScalar("SELECT Grade" + where + "31000"));
        EXPECT_EQ("1", queryScalar("SELECT ItemFlag" + where + "31000"));
        repository.updateCoreZap(GEAR_CORE_ZAP, 79, 4, "it-owner", 1, 6, 3, 5, "2,3", 4, 31000);
        EXPECT_EQ("4", queryScalar("SELECT Grade" + where + "31000"));
        EXPECT_EQ("2,3", queryScalar("SELECT OptionType" + where + "31000"));
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + "31000"));

        std::vector<CoreZapObjectRow> owned = repository.loadCoreZapOfOwner(GEAR_CORE_ZAP, "it-owner");
        ASSERT_EQ(1u, owned.size());
        EXPECT_EQ(31000u, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(4u, owned[0].itemType);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(3, owned[0].x);
        EXPECT_EQ(5, owned[0].y);
        EXPECT_EQ("2,3", owned[0].optionField);
        EXPECT_EQ(4, owned[0].grade);
        EXPECT_EQ(1, owned[0].createType);

        std::vector<CoreZapZoneObjectRow> inZone = repository.loadCoreZapInZone(GEAR_CORE_ZAP, 5, 31000);
        ASSERT_EQ(1u, inZone.size());
        EXPECT_EQ(31100, inZone[0].itemID);
        EXPECT_EQ(7, inZone[0].x);
        EXPECT_EQ("", inZone[0].optionField);
        EXPECT_EQ(0, inZone[0].createType);
        EXPECT_TRUE(repository.loadCoreZapInZone(GEAR_CORE_ZAP, 5, 31001).empty());

        repository.tinysaveGear(GEAR_CORE_ZAP, "X=9", 31000);
        EXPECT_EQ("9", queryScalar("SELECT X" + where + "31000"));
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM CoreZapInfo").c_str()),
                  repository.loadMaxGearType(GEAR_CORE_ZAP));
        std::vector<IntInfoRow> infos = repository.loadIntInfos(GEAR_CORE_ZAP);
        ASSERT_FALSE(infos.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM CoreZapInfo").c_str()), (int)infos.size());
        EXPECT_EQ(std::to_string(infos[0].value), queryScalar("SELECT OptionClass FROM CoreZapInfo WHERE ItemType=" +
                                                              std::to_string(infos[0].basic.itemType)));
        EXPECT_THROW(repository.loadBasicInfos(GEAR_CORE_ZAP), Error);
    }

    // Belt and OustersArmsband: gear's seven statements, destroy() by ItemID, the pocket Info shape.
    for (size_t i = 0; i < sizeof(kPocketTables) / sizeof(kPocketTables[0]); i++) {
        const GearTable table = kPocketTables[i].table;
        const std::string name = kPocketTables[i].name;
        const std::string where = std::string(" FROM ") + name + " WHERE ItemID=";
        const std::string id = std::to_string(31000 + i);

        repository.insertGear(table, 31000 + i, 77, 3, "it-owner", 1, 5, 2, 4, "1,2", 10, 3, 1);
        repository.insertGear(table, 31100 + i, 78, 3, "it-owner", 5, 31000, 7, 8, "", 20, 2, 0);
        EXPECT_EQ("10", queryScalar("SELECT Durability" + where + id)) << name;
        repository.updateGear(table, 79, 4, "it-owner", 1, 6, 3, 5, "2,3", 11, 4, 7, 31000 + i);
        EXPECT_EQ("11", queryScalar("SELECT Durability" + where + id)) << name;
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + id)) << name;

        std::vector<GearObjectRow> owned = repository.loadGearOfOwner(table, "it-owner");
        ASSERT_EQ(1u, owned.size()) << name;
        EXPECT_EQ(31000 + i, owned[0].itemID);
        EXPECT_EQ("2,3", owned[0].optionField);
        EXPECT_EQ(11, owned[0].durability);
        EXPECT_EQ(4, owned[0].grade);
        EXPECT_EQ(7, owned[0].enchantLevel);
        EXPECT_EQ(1, owned[0].createType);

        std::vector<GearZoneObjectRow> inZone = repository.loadGearInZone(table, 5, 31000);
        ASSERT_EQ(1u, inZone.size()) << name;
        EXPECT_EQ((int)(31100 + i), inZone[0].itemID);
        EXPECT_EQ(20, inZone[0].durability);
        EXPECT_TRUE(repository.loadGearInZone(table, 5, 31001).empty()) << name;

        repository.tinysaveGear(table, "X=9", 31000 + i);
        EXPECT_EQ("9", queryScalar("SELECT X" + where + id)) << name;

        EXPECT_TRUE(repository.destroyGearObject(table, 31100 + i)) << name;
        EXPECT_TRUE(repository.loadGearInZone(table, 5, 31000).empty()) << name;
        EXPECT_FALSE(repository.destroyGearObject(table, 31100 + i)) << name; // no row went the second time
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + id)) << name; // the other row stays

        const std::string info = name.substr(0, name.size() - 6) + "Info";
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM " + info).c_str()), repository.loadMaxGearType(table))
            << info;
        std::vector<PocketInfoRow> rows = repository.loadPocketInfos(table);
        ASSERT_FALSE(rows.empty()) << info;
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM " + info).c_str()), (int)rows.size()) << info;
        const std::string type = " FROM " + info + " WHERE ItemType=" + std::to_string(rows[0].itemType);
        EXPECT_EQ(std::to_string(rows[0].pocketCount), queryScalar("SELECT PocketCount" + type)) << info;
        EXPECT_EQ(std::to_string(rows[0].itemLevel), queryScalar("SELECT ItemLevel" + type)) << info;
        EXPECT_EQ(std::to_string(rows[0].downgradeRatio), queryScalar("SELECT DowngradeRatio" + type)) << info;
        EXPECT_THROW(repository.loadGearInfos(table), Error) << info;
    }

    // The object-shape and Info guards, both ways.
    EXPECT_THROW(repository.insertOptionGradeItem(GEAR_BELT, 31900, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1), Error);
    EXPECT_THROW(repository.insertGear(GEAR_VAMPIRE_AMULET, 31900, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1), Error);
    EXPECT_THROW(repository.insertGear(GEAR_CORE_ZAP, 31900, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1), Error);
    EXPECT_THROW(repository.updateGear(GEAR_VAMPIRE_AMULET, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1, 31000), Error);
    EXPECT_THROW(repository.updateAmulet(GEAR_CORE_ZAP, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 31000), Error);
    EXPECT_THROW(repository.updateAmulet(GEAR_BELT, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 31000), Error);
    EXPECT_THROW(repository.updateCoreZap(GEAR_VAMPIRE_AMULET, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 31000), Error);
    EXPECT_THROW(repository.loadGearOfOwner(GEAR_CORE_ZAP, "it-owner"), Error);
    EXPECT_THROW(repository.loadGearInZone(GEAR_CORE_ZAP, 5, 31000), Error);
    EXPECT_THROW(repository.loadCoreZapOfOwner(GEAR_VAMPIRE_AMULET, "it-owner"), Error);
    EXPECT_THROW(repository.loadCoreZapInZone(GEAR_BELT, 5, 31000), Error);
    EXPECT_THROW(repository.destroyGearObject(GEAR_RING, 31000), Error);
    EXPECT_THROW(repository.destroyGearObject(GEAR_VAMPIRE_AMULET, 31000), Error);
    EXPECT_THROW(repository.destroyItemObject(GEAR_BELT, "BeltObject", 31000), Error);
    EXPECT_THROW(repository.loadPocketInfos(GEAR_RING), Error);
    EXPECT_THROW(repository.loadPocketInfos(GEAR_VAMPIRE_AMULET), Error);
    EXPECT_THROW(repository.loadIntInfos(GEAR_VAMPIRE_AMULET), Error);
    EXPECT_THROW(repository.loadSilverWeaponOfOwner(GEAR_VAMPIRE_AMULET, "it-owner"), Error);
    EXPECT_THROW(repository.loadNumItemOfOwner(GEAR_CORE_ZAP, "it-owner"), Error);
}

TEST_F(ItemObjectMySQL, OneLoaderGearAndOptionGradeRowsRoundTripAndTheZoneLoadRefusesThem) {
    ItemObjectRepository& repository = defaultItemObjectRepository();

    // Mitten, ShoulderArmor and Persona: gear's six statements, no zone literal.
    for (size_t i = 0; i < sizeof(kOneLoaderGearTables) / sizeof(kOneLoaderGearTables[0]); i++) {
        const GearTable table = kOneLoaderGearTables[i].table;
        const std::string name = kOneLoaderGearTables[i].name;
        const std::string where = std::string(" FROM ") + name + " WHERE ItemID=";
        const std::string id = std::to_string(31000 + i);

        repository.insertGear(table, 31000 + i, 77, 3, "it-owner", 1, 5, 2, 4, "1,2", 10, 3, 1);
        // Storage 5 is outside the owner load's IN(0, 1, 2, 3, 4, 9) list, and no zone load can read it.
        repository.insertGear(table, 31100 + i, 78, 3, "it-owner", 5, 31000, 7, 8, "", 20, 2, 0);
        EXPECT_EQ("10", queryScalar("SELECT Durability" + where + id)) << name;
        EXPECT_EQ("1", queryScalar("SELECT ItemFlag" + where + id)) << name;
        repository.updateGear(table, 79, 4, "it-owner", 1, 6, 3, 5, "2,3", 11, 4, 7, 31000 + i);
        EXPECT_EQ("11", queryScalar("SELECT Durability" + where + id)) << name;
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + id)) << name;

        std::vector<GearObjectRow> owned = repository.loadGearOfOwner(table, "it-owner");
        ASSERT_EQ(1u, owned.size()) << name;
        EXPECT_EQ(31000 + i, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ("2,3", owned[0].optionField);
        EXPECT_EQ(11, owned[0].durability);
        EXPECT_EQ(4, owned[0].grade);
        EXPECT_EQ(7, owned[0].enchantLevel);
        EXPECT_EQ(1, owned[0].createType);

        // The spec row carries no zone literal: the zone load refuses the table.
        EXPECT_THROW(repository.loadGearInZone(table, 5, 31000), Error) << name;

        repository.tinysaveGear(table, "X=9", 31000 + i);
        EXPECT_EQ("9", queryScalar("SELECT X" + where + id)) << name;

        const std::string info = name.substr(0, name.size() - 6) + "Info";
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM " + info).c_str()), repository.loadMaxGearType(table))
            << info;
        if (table == GEAR_PERSONA) { // VampireCoat's Info shape: no UpgradeRatio, no DowngradeRatio
            std::vector<GearInfoNoRatioRow> rows = repository.loadGearInfosNoRatio(table);
            ASSERT_FALSE(rows.empty()) << info;
            EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM " + info).c_str()), (int)rows.size()) << info;
            const std::string type = " FROM " + info + " WHERE ItemType=" + std::to_string(rows[0].itemType);
            EXPECT_EQ(std::to_string(rows[0].durability), queryScalar("SELECT Durability" + type)) << info;
            EXPECT_EQ(std::to_string(rows[0].nextItemType), queryScalar("SELECT NextItemType" + type)) << info;
            EXPECT_THROW(repository.loadGearInfos(table), Error) << info;
        } else {
            std::vector<GearInfoRow> rows = repository.loadGearInfos(table);
            ASSERT_FALSE(rows.empty()) << info;
            EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM " + info).c_str()), (int)rows.size()) << info;
            const std::string type = " FROM " + info + " WHERE ItemType=" + std::to_string(rows[0].itemType);
            EXPECT_EQ(std::to_string(rows[0].durability), queryScalar("SELECT Durability" + type)) << info;
            EXPECT_EQ(std::to_string(rows[0].downgradeRatio), queryScalar("SELECT DowngradeRatio" + type)) << info;
            EXPECT_THROW(repository.loadGearInfosNoRatio(table), Error) << info;
        }
        EXPECT_THROW(repository.loadGearInfosNoDurability(table), Error) << info;
    }

    // Dermis, Fascia and CarryingReceiver: the amulet INSERT and UPDATE, an owner
    // load of eleven columns and an Info SELECT of seventeen — both without Durability.
    for (size_t i = 0; i < sizeof(kOptionGradeTables) / sizeof(kOptionGradeTables[0]); i++) {
        const GearTable table = kOptionGradeTables[i].table;
        const std::string name = kOptionGradeTables[i].name;
        const std::string where = std::string(" FROM ") + name + " WHERE ItemID=";
        const std::string id = std::to_string(31000 + i);

        repository.insertOptionGradeItem(table, 31000 + i, 77, 3, "it-owner", 1, 5, 2, 4, "1,2", 3, 1);
        repository.insertOptionGradeItem(table, 31100 + i, 78, 3, "it-owner", 5, 31000, 7, 8, "", 2, 0);
        EXPECT_EQ("1,2", queryScalar("SELECT OptionType" + where + id)) << name;
        EXPECT_EQ("3", queryScalar("SELECT Grade" + where + id)) << name;
        EXPECT_EQ("1", queryScalar("SELECT ItemFlag" + where + id)) << name;
        EXPECT_EQ("0", queryScalar("SELECT Durability" + where + id)) << name; // the INSERT names none
        repository.updateAmulet(table, 79, 4, "it-owner", 1, 6, 3, 5, "2,3", 4, 7, 31000 + i);
        EXPECT_EQ("4", queryScalar("SELECT Grade" + where + id)) << name;
        EXPECT_EQ("7", queryScalar("SELECT EnchantLevel" + where + id)) << name;
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + id)) << name;
        EXPECT_EQ("0", queryScalar("SELECT Durability" + where + id)) << name; // nor does the UPDATE

        std::vector<OptionGradeObjectRow> owned = repository.loadOptionGradeOfOwner(table, "it-owner");
        ASSERT_EQ(1u, owned.size()) << name; // the Storage 5 row is outside the IN list
        EXPECT_EQ(31000 + i, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(4u, owned[0].itemType);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(3, owned[0].x);
        EXPECT_EQ(5, owned[0].y);
        EXPECT_EQ("2,3", owned[0].optionField);
        EXPECT_EQ(4, owned[0].grade);
        EXPECT_EQ(7, owned[0].enchantLevel);
        EXPECT_EQ(1, owned[0].createType);

        repository.tinysaveGear(table, "X=9", 31000 + i);
        EXPECT_EQ("9", queryScalar("SELECT X" + where + id)) << name;

        const std::string info = name.substr(0, name.size() - 6) + "Info";
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM " + info).c_str()), repository.loadMaxGearType(table))
            << info;
        std::vector<GearInfoNoDurabilityRow> rows = repository.loadGearInfosNoDurability(table);
        ASSERT_FALSE(rows.empty()) << info;
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM " + info).c_str()), (int)rows.size()) << info;
        const std::string type = " FROM " + info + " WHERE ItemType=" + std::to_string(rows[0].itemType);
        EXPECT_EQ(rows[0].name, queryScalar("SELECT Name" + type)) << info;
        EXPECT_EQ(std::to_string(rows[0].ratio), queryScalar("SELECT Ratio" + type)) << info;
        EXPECT_EQ(std::to_string(rows[0].defense), queryScalar("SELECT Defense" + type)) << info;
        EXPECT_EQ(std::to_string(rows[0].upgradeRatio), queryScalar("SELECT UpgradeRatio" + type)) << info;
        EXPECT_EQ(std::to_string(rows[0].downgradeRatio), queryScalar("SELECT DowngradeRatio" + type)) << info;
        EXPECT_THROW(repository.loadGearInfos(table), Error) << info;
    }

    // The object-shape and Info guards, both ways.
    EXPECT_THROW(repository.insertGear(GEAR_DERMIS, 31900, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1), Error);
    EXPECT_THROW(repository.insertOptionGradeItem(GEAR_MITTEN, 31900, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1), Error);
    EXPECT_THROW(repository.updateGear(GEAR_FASCIA, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1, 31000), Error);
    EXPECT_THROW(repository.updateAmulet(GEAR_SHOULDER_ARMOR, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 31000), Error);
    EXPECT_THROW(repository.updateCoreZap(GEAR_DERMIS, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 31000), Error);
    EXPECT_THROW(repository.loadGearOfOwner(GEAR_CARRYING_RECEIVER, "it-owner"), Error);
    EXPECT_THROW(repository.loadOptionGradeOfOwner(GEAR_PERSONA, "it-owner"), Error);
    EXPECT_THROW(repository.loadOptionGradeOfOwner(GEAR_VAMPIRE_AMULET, "it-owner"), Error);
    EXPECT_THROW(repository.loadGearInfosNoDurability(GEAR_RING), Error);
    EXPECT_THROW(repository.loadGearInfosNoDurability(GEAR_VAMPIRE_COAT), Error);
    EXPECT_THROW(repository.destroyGearObject(GEAR_MITTEN, 31000), Error);
    EXPECT_THROW(repository.loadNumItemOfOwner(GEAR_DERMIS, "it-owner"), Error);
    EXPECT_THROW(repository.loadSilverWeaponOfOwner(GEAR_MITTEN, "it-owner"), Error);
    // The tables that do carry a zone literal still load: the guard is the literal, not the shape.
    EXPECT_NO_THROW(repository.loadGearInZone(GEAR_RING, 5, 31000));
    EXPECT_NO_THROW(repository.loadGearInZone(GEAR_VAMPIRE_AMULET, 5, 31000));
}

TEST_F(ItemObjectMySQL, WarItemRowsRoundTripAndTheirCreatureLoaderDeletesTheOwnersRows) {
    ItemObjectRepository& repository = defaultItemObjectRepository();

    for (size_t i = 0; i < sizeof(kWarTables) / sizeof(kWarTables[0]); i++) {
        const GearTable table = kWarTables[i].table;
        const std::string name = kWarTables[i].name;
        const std::string where = std::string(" FROM ") + name + " WHERE ItemID=";
        const std::string id = std::to_string(31000 + i);
        const std::string zoneId = std::to_string(31100 + i);

        // The owner's row, and a zone row belonging to somebody else. The repository
        // returns the statement it ran - the text BloodBible, CastleSymbol and Sweeper log.
        const std::string sql = repository.insertWarItem(table, 31000 + i, 77, 3, "it-owner", 1, 5, 2, 4, 10);
        EXPECT_EQ("INSERT INTO " + name + " (ItemID,  ObjectID, ItemType, OwnerID, Storage, StorageID , X, Y, " +
                      (table == GEAR_CASTLE_SYMBOL ? "Durability )" : "Durability)") + " VALUES(" + id +
                      ", 77, 3, 'it-owner', 1, 5, 2, 4, 10)",
                  sql)
            << name;
        repository.insertWarItem(table, 31100 + i, 78, 3, "it-other", 5, 31000, 7, 8, 20);
        EXPECT_EQ("10", queryScalar("SELECT Durability" + where + id)) << name;
        EXPECT_EQ("77", queryScalar("SELECT ObjectID" + where + id)) << name;
        EXPECT_EQ("0", queryScalar("SELECT EnchantLevel" + where + id)) << name; // the INSERT names none

        repository.updateWarItem(table, 79, 4, "it-owner", 1, 6, 3, 5, 11, 7, 31000 + i);
        EXPECT_EQ("11", queryScalar("SELECT Durability" + where + id)) << name;
        EXPECT_EQ("7", queryScalar("SELECT EnchantLevel" + where + id)) << name;
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + id)) << name;

        repository.tinysaveGear(table, "X=9", 31000 + i);
        EXPECT_EQ("9", queryScalar("SELECT X" + where + id)) << name;

        std::vector<WarItemZoneObjectRow> inZone = repository.loadWarItemInZone(table, 5, 31000);
        ASSERT_EQ(1u, inZone.size()) << name;
        EXPECT_EQ((int)(31100 + i), inZone[0].itemID);
        EXPECT_EQ(78, inZone[0].objectID);
        EXPECT_EQ(3, inZone[0].itemType);
        EXPECT_EQ(5, inZone[0].storage);
        EXPECT_EQ(31000, inZone[0].storageID);
        EXPECT_EQ(7, inZone[0].x);
        EXPECT_EQ(8, inZone[0].y);
        EXPECT_EQ(20, inZone[0].durability);
        EXPECT_EQ(0, inZone[0].enchantLevel);
        EXPECT_TRUE(repository.loadWarItemInZone(table, 5, 31001).empty()) << name;

        // What the creature loader does in place of a SELECT: the owner's rows go, nobody else's.
        repository.deleteWarItemsOfOwner(table, "it-owner");
        EXPECT_EQ("0", queryScalar("SELECT COUNT(*)" + where + id)) << name;
        EXPECT_EQ("1", queryScalar("SELECT COUNT(*)" + where + zoneId)) << name;

        const std::string info = name.substr(0, name.size() - 6) + "Info";
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM " + info).c_str()), repository.loadMaxGearType(table))
            << info;
        if (table == GEAR_RELIC) {
            std::vector<RelicInfoRow> rows = repository.loadRelicInfos(table);
            ASSERT_FALSE(rows.empty()) << info;
            EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM " + info).c_str()), (int)rows.size()) << info;
            const std::string type = " FROM " + info + " WHERE ItemType=" + std::to_string(rows[0].war.itemType);
            EXPECT_EQ(std::to_string(rows[0].war.durability), queryScalar("SELECT Durability" + type)) << info;
            EXPECT_EQ(std::to_string(rows[0].war.itemLevel), queryScalar("SELECT ItemLevel" + type)) << info;
            EXPECT_EQ(rows[0].relicType, queryScalar("SELECT RelicType" + type)) << info;
            EXPECT_EQ(std::to_string(rows[0].zoneID), queryScalar("SELECT ZoneID" + type)) << info;
            EXPECT_EQ(std::to_string(rows[0].x), queryScalar("SELECT XCoord" + type)) << info;
            EXPECT_EQ(std::to_string(rows[0].y), queryScalar("SELECT YCoord" + type)) << info;
            EXPECT_EQ(std::to_string(rows[0].monsterType), queryScalar("SELECT MonsterType" + type)) << info;
            EXPECT_THROW(repository.loadWarInfos(table), Error) << info;
        } else {
            std::vector<WarInfoRow> rows = repository.loadWarInfos(table);
            ASSERT_FALSE(rows.empty()) << info;
            EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM " + info).c_str()), (int)rows.size()) << info;
            const std::string type = " FROM " + info + " WHERE ItemType=" + std::to_string(rows[0].itemType);
            EXPECT_EQ(rows[0].name, queryScalar("SELECT Name" + type)) << info;
            EXPECT_EQ(std::to_string(rows[0].durability), queryScalar("SELECT Durability" + type)) << info;
            EXPECT_EQ(std::to_string(rows[0].protection), queryScalar("SELECT Protection" + type)) << info;
            EXPECT_EQ(std::to_string(rows[0].itemLevel), queryScalar("SELECT ItemLevel" + type)) << info;
            EXPECT_THROW(repository.loadRelicInfos(table), Error) << info;
        }
        EXPECT_THROW(repository.loadGearInfos(table), Error) << info;
    }

    // The object-shape and Info guards, both ways.
    EXPECT_THROW(repository.insertGear(GEAR_BLOOD_BIBLE, 31900, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1), Error);
    EXPECT_THROW(repository.insertWarItem(GEAR_RING, 31900, 1, 1, "it-owner", 1, 1, 1, 1, 1), Error);
    EXPECT_THROW(repository.updateWarItem(GEAR_RING, 1, 1, "it-owner", 1, 1, 1, 1, 1, 1, 31000), Error);
    EXPECT_THROW(repository.updateGear(GEAR_SWEEPER, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 1, 1, 31000), Error);
    EXPECT_THROW(repository.loadWarItemInZone(GEAR_RING, 5, 31000), Error);
    EXPECT_THROW(repository.deleteWarItemsOfOwner(GEAR_RING, "it-owner"), Error);
    EXPECT_THROW(repository.loadGearOfOwner(GEAR_RELIC, "it-owner"), Error);
    EXPECT_THROW(repository.loadWarInfos(GEAR_RING), Error);
    EXPECT_THROW(repository.loadRelicInfos(GEAR_RING), Error);
    EXPECT_THROW(repository.destroyGearObject(GEAR_BLOOD_BIBLE, 31000), Error);
}

TEST_F(ItemObjectMySQL, MotorcycleCodeSheetAndWarItemRowsRoundTripThroughTheirColumns) {
    ItemObjectRepository& repository = defaultItemObjectRepository();

    // Motorcycle: the gear INSERT without Grade and ItemFlag, its own two loads.
    {
        const std::string where = " FROM MotorcycleObject WHERE ItemID=";
        repository.insertMotorcycle(GEAR_MOTORCYCLE, 31000, 77, 3, "it-owner", 1, 5, 2, 4, "1,2", 10);
        repository.insertMotorcycle(GEAR_MOTORCYCLE, 31100, 78, 3, "it-owner", 5, 31000, 7, 8, "", 20);
        EXPECT_EQ("1,2", queryScalar("SELECT OptionType" + where + "31000"));
        EXPECT_EQ("10", queryScalar("SELECT Durability" + where + "31000"));
        repository.updateMotorcycle(GEAR_MOTORCYCLE, 79, 4, "it-owner", 1, 6, 3, 5, "2,3", 11, 31000);
        EXPECT_EQ("2,3", queryScalar("SELECT OptionType" + where + "31000"));
        EXPECT_EQ("11", queryScalar("SELECT Durability" + where + "31000"));
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + "31000"));

        std::vector<MotorcycleObjectRow> owned = repository.loadMotorcycleOfOwner(GEAR_MOTORCYCLE, "it-owner");
        ASSERT_EQ(1u, owned.size());
        EXPECT_EQ(31000u, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(4u, owned[0].itemType);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(3, owned[0].x);
        EXPECT_EQ(5, owned[0].y);
        EXPECT_EQ("2,3", owned[0].optionField);
        EXPECT_EQ(11, owned[0].durability);

        std::vector<MotorcycleZoneObjectRow> inZone = repository.loadMotorcycleInZone(GEAR_MOTORCYCLE, 5, 31000);
        ASSERT_EQ(1u, inZone.size());
        EXPECT_EQ(31100, inZone[0].itemID);
        EXPECT_EQ(78, inZone[0].objectID);
        EXPECT_EQ(7, inZone[0].x);
        EXPECT_EQ(20, inZone[0].durability);
        EXPECT_TRUE(repository.loadMotorcycleInZone(GEAR_MOTORCYCLE, 5, 31001).empty());

        repository.tinysaveGear(GEAR_MOTORCYCLE, "X=9", 31000);
        EXPECT_EQ("9", queryScalar("SELECT X" + where + "31000"));
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM MotorcycleInfo").c_str()),
                  repository.loadMaxGearType(GEAR_MOTORCYCLE));
        std::vector<DurabilityInfoRow> infos = repository.loadDurabilityInfos(GEAR_MOTORCYCLE);
        ASSERT_FALSE(infos.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM MotorcycleInfo").c_str()), (int)infos.size());
        const std::string type = " FROM MotorcycleInfo WHERE ItemType=" + std::to_string(infos[0].itemType);
        EXPECT_EQ(infos[0].name, queryScalar("SELECT Name" + type));
        EXPECT_EQ(std::to_string(infos[0].ratio), queryScalar("SELECT Ratio" + type));
        EXPECT_EQ(std::to_string(infos[0].durability), queryScalar("SELECT Durability" + type));
        EXPECT_THROW(repository.loadGearInfos(GEAR_MOTORCYCLE), Error);
    }

    // CodeSheet: the plain INSERT plus OptionType, an owner load of eight columns.
    {
        const std::string where = " FROM CodeSheetObject WHERE ItemID=";
        repository.insertCodeSheet(GEAR_CODE_SHEET, 31000, 77, 3, "it-owner", 1, 5, 2, 4, "1,2");
        EXPECT_EQ("1,2", queryScalar("SELECT OptionType" + where + "31000"));
        EXPECT_EQ("5", queryScalar("SELECT StorageID" + where + "31000"));
        repository.updateCodeSheet(GEAR_CODE_SHEET, 79, 4, "it-owner", 1, 6, 3, 5, "2,3", 31000);
        EXPECT_EQ("2,3", queryScalar("SELECT OptionType" + where + "31000"));
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + "31000"));

        std::vector<CodeSheetObjectRow> owned = repository.loadCodeSheetOfOwner(GEAR_CODE_SHEET, "it-owner");
        ASSERT_EQ(1u, owned.size());
        EXPECT_EQ(31000u, owned[0].itemID);
        EXPECT_EQ(79u, owned[0].objectID);
        EXPECT_EQ(4u, owned[0].itemType);
        EXPECT_EQ(1, owned[0].storage);
        EXPECT_EQ(6u, owned[0].storageID);
        EXPECT_EQ(3, owned[0].x);
        EXPECT_EQ(5, owned[0].y);
        EXPECT_EQ("2,3", owned[0].optionField);

        // Its zone SELECT names Durability, EnchantLevel and ItemFlag, which
        // CodeSheetObject does not have: the statement fails against this schema
        // (END_DB rethrows).
        EXPECT_ANY_THROW(repository.loadGearInZone(GEAR_CODE_SHEET, 5, 31000));

        repository.tinysaveGear(GEAR_CODE_SHEET, "X=9", 31000);
        EXPECT_EQ("9", queryScalar("SELECT X" + where + "31000"));
        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM CodeSheetInfo").c_str()),
                  repository.loadMaxGearType(GEAR_CODE_SHEET));
        std::vector<HeadInfoRow> infos = repository.loadHeadInfos(GEAR_CODE_SHEET);
        ASSERT_FALSE(infos.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM CodeSheetInfo").c_str()), (int)infos.size());
        const std::string type = " FROM CodeSheetInfo WHERE ItemType=" + std::to_string(infos[0].itemType);
        EXPECT_EQ(infos[0].name, queryScalar("SELECT Name" + type));
        EXPECT_EQ(std::to_string(infos[0].price), queryScalar("SELECT Price" + type));
        EXPECT_THROW(repository.loadBasicInfos(GEAR_CODE_SHEET), Error);
    }

    // WarItem: the plain statements, but no loader holds SQL - so neither load serves it.
    {
        const std::string where = " FROM WarItemObject WHERE ItemID=";
        // The repository returns the statement it ran; WarItem's create logs it to WarLog.txt.
        const std::string sql = repository.insertPlainItemLogged(GEAR_WAR_ITEM, 31000, 77, 3, "it-owner", 1, 5, 2, 4);
        EXPECT_EQ("INSERT INTO WarItemObject (ItemID,  ObjectID, ItemType, OwnerID, Storage, StorageID , X, Y) "
                  "VALUES(31000, 77, 3, 'it-owner', 1, 5, 2, 4)",
                  sql);
        EXPECT_EQ("77", queryScalar("SELECT ObjectID" + where + "31000"));
        EXPECT_EQ("0", queryScalar("SELECT ItemFlag" + where + "31000")); // the INSERT names none
        repository.updatePlainItem(GEAR_WAR_ITEM, 79, 4, "it-owner", 1, 6, 3, 5, 31000);
        EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + "31000"));
        EXPECT_EQ("6", queryScalar("SELECT StorageID" + where + "31000"));
        repository.tinysaveGear(GEAR_WAR_ITEM, "X=9", 31000);
        EXPECT_EQ("9", queryScalar("SELECT X" + where + "31000"));

        EXPECT_THROW(repository.loadPlainItemOfOwner(GEAR_WAR_ITEM, "it-owner"), Error);
        EXPECT_THROW(repository.loadPlainItemInZone(GEAR_WAR_ITEM, 5, 31000), Error);

        EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM WarItemInfo").c_str()),
                  repository.loadMaxGearType(GEAR_WAR_ITEM));
        std::vector<BasicInfoRow> infos = repository.loadBasicInfos(GEAR_WAR_ITEM);
        ASSERT_FALSE(infos.empty());
        EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM WarItemInfo").c_str()), (int)infos.size());
        EXPECT_THROW(repository.loadHeadInfos(GEAR_WAR_ITEM), Error);
    }

    // The object-shape and Info guards, both ways.
    EXPECT_THROW(repository.insertPlainItem(GEAR_MOTORCYCLE, 31900, 1, 1, "it-owner", 1, 1, 1, 1), Error);
    EXPECT_THROW(repository.insertPlainItemLogged(GEAR_MOTORCYCLE, 31900, 1, 1, "it-owner", 1, 1, 1, 1), Error);
    EXPECT_THROW(repository.insertMotorcycle(GEAR_RING, 31900, 1, 1, "it-owner", 1, 1, 1, 1, "", 1), Error);
    EXPECT_THROW(repository.insertCodeSheet(GEAR_WAR_ITEM, 31900, 1, 1, "it-owner", 1, 1, 1, 1, ""), Error);
    EXPECT_THROW(repository.updateMotorcycle(GEAR_CODE_SHEET, 1, 1, "it-owner", 1, 1, 1, 1, "", 1, 31000), Error);
    EXPECT_THROW(repository.updateCodeSheet(GEAR_MOTORCYCLE, 1, 1, "it-owner", 1, 1, 1, 1, "", 31000), Error);
    EXPECT_THROW(repository.loadMotorcycleOfOwner(GEAR_CODE_SHEET, "it-owner"), Error);
    EXPECT_THROW(repository.loadCodeSheetOfOwner(GEAR_MOTORCYCLE, "it-owner"), Error);
    EXPECT_THROW(repository.loadGearOfOwner(GEAR_CODE_SHEET, "it-owner"), Error);
    EXPECT_THROW(repository.loadGearInZone(GEAR_MOTORCYCLE, 5, 31000), Error);
    EXPECT_THROW(repository.loadDurabilityInfos(GEAR_RING), Error);
    EXPECT_THROW(repository.loadHeadInfos(GEAR_RING), Error);
    EXPECT_THROW(repository.loadHeadInfos(GEAR_MIXING_ITEM), Error);
    EXPECT_THROW(repository.destroyGearObject(GEAR_MOTORCYCLE, 31000), Error);
    // The plain loads still serve the tables that do carry their literals.
    EXPECT_NO_THROW(repository.loadPlainItemOfOwner(GEAR_EVENT_GIFT_BOX, "it-owner"));
    EXPECT_NO_THROW(repository.loadPlainItemInZone(GEAR_EVENT_GIFT_BOX, 5, 31000));
}

TEST_F(ItemObjectMySQL, PetItemRowsRoundTripWithAndWithoutTheirPetColumns) {
    ItemObjectRepository& repository = defaultItemObjectRepository();
    const std::string where = " FROM PetItemObject WHERE ItemID=";

    // The owner's row goes in without a PetInfo; the zone row with one.
    repository.insertPetItem(GEAR_PET_ITEM, 31000, 77, 3, "it-owner", 1, 5, 2, 4, 1);
    repository.insertPetItemWithInfo(GEAR_PET_ITEM, 31100, 78, 3, "it-owner", 5, 31000, 7, 8, 0, 12, 13, 1400u, 15, 16,
                                     17, 18, 19, 1, 0, 1, "2026-09-03 10:11:12");
    EXPECT_EQ("77", queryScalar("SELECT ObjectID" + where + "31000"));
    EXPECT_EQ("1", queryScalar("SELECT ItemFlag" + where + "31000"));
    EXPECT_EQ("0", queryScalar("SELECT PetLevel" + where + "31000")); // the short INSERT names no pet column
    EXPECT_EQ("12", queryScalar("SELECT PetCreatureType" + where + "31100"));
    EXPECT_EQ("13", queryScalar("SELECT PetLevel" + where + "31100"));
    EXPECT_EQ("1400", queryScalar("SELECT PetExp" + where + "31100"));
    EXPECT_EQ("19", queryScalar("SELECT FoodType" + where + "31100"));
    EXPECT_EQ("2026-09-03 10:11:12", queryScalar("SELECT LastFeedTime" + where + "31100"));

    repository.updatePetItem(GEAR_PET_ITEM, 79, 4, "it-owner", 1, 6, 3, 5, 31000);
    EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + "31000"));
    EXPECT_EQ("6", queryScalar("SELECT StorageID" + where + "31000"));
    EXPECT_EQ("0", queryScalar("SELECT PetLevel" + where + "31000")); // nor does the short UPDATE

    repository.updatePetItemWithInfo(GEAR_PET_ITEM, 80, 4, "it-owner", 1, 6, 3, 5, 22, 23, 24, 25, 2600u, 27, 28, 1, 1,
                                     0, "2026-09-04 01:02:03", "pet-name", 31000);
    EXPECT_EQ("80", queryScalar("SELECT ObjectID" + where + "31000"));
    EXPECT_EQ("22", queryScalar("SELECT PetCreatureType" + where + "31000"));
    EXPECT_EQ("2600", queryScalar("SELECT PetExp" + where + "31000"));
    EXPECT_EQ("27", queryScalar("SELECT PetHP" + where + "31000"));
    EXPECT_EQ("pet-name", queryScalar("SELECT Nickname" + where + "31000"));
    EXPECT_EQ("2026-09-04 01:02:03", queryScalar("SELECT LastFeedTime" + where + "31000"));

    // savePetInfo writes the pet columns alone.
    repository.savePetItemInfo(GEAR_PET_ITEM, 32, 33, 34, 35, 3600u, 37, 38, 0, 1, 1, "2026-09-05 04:05:06",
                               "fed-again", 31000);
    EXPECT_EQ("80", queryScalar("SELECT ObjectID" + where + "31000")); // untouched
    EXPECT_EQ("32", queryScalar("SELECT PetCreatureType" + where + "31000"));
    EXPECT_EQ("3600", queryScalar("SELECT PetExp" + where + "31000"));
    EXPECT_EQ("fed-again", queryScalar("SELECT Nickname" + where + "31000"));

    std::vector<PetItemObjectRow> owned = repository.loadPetItemOfOwner(GEAR_PET_ITEM, "it-owner");
    ASSERT_EQ(1u, owned.size()); // the Storage 5 row is outside the owner list
    EXPECT_EQ(31000u, owned[0].itemID);
    EXPECT_EQ(80u, owned[0].objectID);
    EXPECT_EQ(4u, owned[0].itemType);
    EXPECT_EQ(1, owned[0].storage);
    EXPECT_EQ(6u, owned[0].storageID);
    EXPECT_EQ(3, owned[0].x);
    EXPECT_EQ(5, owned[0].y);
    EXPECT_EQ(1, owned[0].createType);
    EXPECT_EQ(32, owned[0].petCreatureType);
    EXPECT_EQ(33, owned[0].petLevel);
    EXPECT_EQ(3600, owned[0].petExp);
    EXPECT_EQ(37, owned[0].petHP);
    EXPECT_EQ(34, owned[0].petAttr);
    EXPECT_EQ(35, owned[0].petAttrLevel);
    EXPECT_EQ(0, owned[0].petOption); // only the long INSERT writes PetOption, and this row
                                      // came in through the short one
    EXPECT_EQ(38, owned[0].foodType);
    EXPECT_EQ(0, owned[0].canGamble);
    EXPECT_EQ(1, owned[0].canCutHead);
    EXPECT_EQ(1, owned[0].canAttack);
    EXPECT_EQ("2026-09-05 04:05:06", owned[0].lastFeedTime);
    EXPECT_EQ("fed-again", owned[0].nickname);

    // Its zone SELECT is the ItemFlag-only eight.
    std::vector<FlagZoneObjectRow> inZone = repository.loadFlagItemInZone(GEAR_PET_ITEM, 5, 31000);
    ASSERT_EQ(1u, inZone.size());
    EXPECT_EQ(31100, inZone[0].itemID);
    EXPECT_EQ(78, inZone[0].objectID);
    EXPECT_EQ(7, inZone[0].x);
    EXPECT_EQ(0, inZone[0].createType);
    EXPECT_TRUE(repository.loadFlagItemInZone(GEAR_PET_ITEM, 5, 31001).empty());

    repository.tinysaveGear(GEAR_PET_ITEM, "X=9", 31000);
    EXPECT_EQ("9", queryScalar("SELECT X" + where + "31000"));
    EXPECT_EQ(atoi(queryScalar("SELECT MAX(ItemType) FROM PetItemInfo").c_str()),
              repository.loadMaxGearType(GEAR_PET_ITEM));
    std::vector<BasicInfoRow> infos = repository.loadBasicInfos(GEAR_PET_ITEM);
    ASSERT_FALSE(infos.empty());
    EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM PetItemInfo").c_str()), (int)infos.size());

    // The object-shape guards, both ways.
    EXPECT_THROW(repository.insertPetItem(GEAR_RING, 31900, 1, 1, "it-owner", 1, 1, 1, 1, 1), Error);
    EXPECT_THROW(repository.insertPetItemWithInfo(GEAR_RING, 31900, 1, 1, "it-owner", 1, 1, 1, 1, 1, 1, 1, 1u, 1, 1, 1,
                                                  1, 1, 1, 1, 1, ""),
                 Error);
    EXPECT_THROW(repository.updatePetItem(GEAR_RING, 1, 1, "it-owner", 1, 1, 1, 1, 31000), Error);
    EXPECT_THROW(repository.updatePetItemWithInfo(GEAR_RING, 1, 1, "it-owner", 1, 1, 1, 1, 1, 1, 1, 1, 1u, 1, 1, 1, 1,
                                                  1, "", "", 31000),
                 Error);
    EXPECT_THROW(repository.savePetItemInfo(GEAR_RING, 1, 1, 1, 1, 1u, 1, 1, 1, 1, 1, "", "", 31000), Error);
    EXPECT_THROW(repository.loadPetItemOfOwner(GEAR_QUEST_ITEM, "it-owner"), Error);
    EXPECT_THROW(repository.insertPlainItem(GEAR_PET_ITEM, 31900, 1, 1, "it-owner", 1, 1, 1, 1), Error);
    EXPECT_THROW(repository.loadFlagItemOfOwner(GEAR_PET_ITEM, "it-owner"), Error);
    EXPECT_THROW(repository.loadGearInZone(GEAR_PET_ITEM, 5, 31000), Error);
    EXPECT_THROW(repository.destroyGearObject(GEAR_PET_ITEM, 31000), Error);
    // The ItemFlag-only zone load still serves the tables it always did.
    EXPECT_NO_THROW(repository.loadFlagItemInZone(GEAR_QUEST_ITEM, 5, 31000));
}

// The motorcycle-redeem statements: the probe, the four-column read in both
// spellings, and the fresh-row insert in both. A row this test does not own
// (31100) stays untouched by every step, so the reads and inserts are shown
// scoped to the id they were given, and the REDEEM_SPELLING_QUEST_ACTION
// insert's IGNORE is shown to matter: the handler spelling refuses a
// duplicate key, the quest-action spelling swallows it.
TEST_F(ItemObjectMySQL, MotorcycleRedeemProbesReadsAndInsertsInBothSpellings) {
    ItemObjectRepository& repository = defaultItemObjectRepository();
    const std::string where = " FROM MotorcycleObject WHERE ItemID=";

    // Seed a row this test does not own; nothing below may touch it.
    execSQL("INSERT INTO MotorcycleObject (ItemID, ObjectID, ItemType, OwnerID, Storage, StorageID, X, Y, "
            "OptionType, Durability) VALUES (31100, 5, 2, 'it-owner', 5, 9, 1, 1, '7', 44)");

    EXPECT_FALSE(repository.motorcycleExists(31000));
    EXPECT_TRUE(repository.motorcycleExists(31100));

    MotorcycleRedeemRow row;
    row.itemID = -1;
    EXPECT_FALSE(repository.loadMotorcycleForRedeem(REDEEM_SPELLING_HANDLER, 31000, row));
    EXPECT_FALSE(repository.loadMotorcycleForRedeem(REDEEM_SPELLING_QUEST_ACTION, 31000, row));
    EXPECT_EQ(-1, row.itemID); // a miss leaves the row alone

    // The handler spelling's insert: an empty OwnerID and OptionType in the
    // literal, every number through "%d".
    repository.insertRedeemedMotorcycle(REDEEM_SPELLING_HANDLER, 31000, 77, 3, 5, 31, 12, 13, 300);
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*)" + where + "31000"));
    EXPECT_EQ("", queryScalar("SELECT OwnerID" + where + "31000"));
    EXPECT_EQ("", queryScalar("SELECT OptionType" + where + "31000"));
    EXPECT_EQ("77", queryScalar("SELECT ObjectID" + where + "31000"));
    EXPECT_EQ("3", queryScalar("SELECT ItemType" + where + "31000"));
    EXPECT_EQ("5", queryScalar("SELECT Storage" + where + "31000"));
    EXPECT_EQ("31", queryScalar("SELECT StorageID" + where + "31000"));
    EXPECT_EQ("12", queryScalar("SELECT X" + where + "31000"));
    EXPECT_EQ("13", queryScalar("SELECT Y" + where + "31000"));
    EXPECT_EQ("300", queryScalar("SELECT Durability" + where + "31000"));
    EXPECT_TRUE(repository.motorcycleExists(31000));

    // Both read spellings reach the same row through their own columns.
    for (int spelling = 0; spelling < REDEEM_SPELLING_MAX; spelling++) {
        MotorcycleRedeemRow read;
        ASSERT_TRUE(repository.loadMotorcycleForRedeem((MotorcycleRedeemSpelling)spelling, 31000, read))
            << "spelling " << spelling;
        EXPECT_EQ(31000, read.itemID);
        EXPECT_EQ(3, read.itemType);
        EXPECT_EQ("", read.optionField);
        EXPECT_EQ(300, read.durability);

        MotorcycleRedeemRow seeded;
        ASSERT_TRUE(repository.loadMotorcycleForRedeem((MotorcycleRedeemSpelling)spelling, 31100, seeded))
            << "spelling " << spelling;
        EXPECT_EQ(31100, seeded.itemID);
        EXPECT_EQ(2, seeded.itemType);
        EXPECT_EQ("7", seeded.optionField);
        EXPECT_EQ(44, seeded.durability);
    }

    // The quest-action spelling is INSERT IGNORE: a second row for the same id
    // is swallowed and the standing row is untouched. The handler spelling
    // has no IGNORE, so the same duplicate is a SQL failure thrown as
    // END_DB's DatabaseError.
    repository.insertRedeemedMotorcycle(REDEEM_SPELLING_QUEST_ACTION, 31000, 78, 4, 5, 32, 14, 15, 301);
    EXPECT_EQ("77", queryScalar("SELECT ObjectID" + where + "31000"));
    EXPECT_EQ("300", queryScalar("SELECT Durability" + where + "31000"));
    EXPECT_THROW(repository.insertRedeemedMotorcycle(REDEEM_SPELLING_HANDLER, 31000, 78, 4, 5, 32, 14, 15, 301),
                 DatabaseError);

    // The quest-action spelling inserts a fresh id like the handler one does.
    repository.insertRedeemedMotorcycle(REDEEM_SPELLING_QUEST_ACTION, 31001, 79, 4, 5, 32, 14, 15, 301);
    EXPECT_EQ("79", queryScalar("SELECT ObjectID" + where + "31001"));
    EXPECT_EQ("", queryScalar("SELECT OwnerID" + where + "31001"));
    EXPECT_EQ("301", queryScalar("SELECT Durability" + where + "31001"));

    // The row this test does not own is as seeded.
    EXPECT_EQ("5", queryScalar("SELECT ObjectID" + where + "31100"));
    EXPECT_EQ("44", queryScalar("SELECT Durability" + where + "31100"));
    EXPECT_EQ("3", queryScalar("SELECT COUNT(*) FROM MotorcycleObject WHERE ItemID >= 31000"));
}

// --- SpecialEvent against real MySQL -----------------------------------------
// The one repository that goes through DatabaseManager::getConnection(int) — the
// world-default connection main() hands it. Rows are keyed by account id.

class SpecialEventMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        clean();
    }
    virtual void TearDown() {
        clean();
    }
    static void clean() {
        execSQL("DELETE FROM SpecialEvent WHERE Name LIKE 'it-%'");
    }
};

TEST_F(SpecialEventMySQL, CountIsReadPerAccountAndResetOnlyForThatAccount) {
    execSQL("INSERT INTO SpecialEvent (Name, Count) VALUES ('it-acct', 3)");
    execSQL("INSERT INTO SpecialEvent (Name, Count) VALUES ('it-other', 4)");

    int count = -1;
    EXPECT_FALSE(defaultSpecialEventRepository().loadCount("it-nobody", count));
    EXPECT_EQ(-1, count); // a miss leaves the out-parameter alone
    ASSERT_TRUE(defaultSpecialEventRepository().loadCount("it-acct", count));
    EXPECT_EQ(3, count);

    defaultSpecialEventRepository().resetCount("it-acct");
    EXPECT_EQ("0", queryScalar("SELECT Count FROM SpecialEvent WHERE Name = 'it-acct'"));
    EXPECT_EQ("4", queryScalar("SELECT Count FROM SpecialEvent WHERE Name = 'it-other'"));
    ASSERT_TRUE(defaultSpecialEventRepository().loadCount("it-acct", count));
    EXPECT_EQ(0, count); // a zeroed row still reads as present: the action
                         // would hand out nothing, but not say "not joined"
}

// --- the quest catalogues against real MySQL -------------------------------
// The mission/ managers load these at NPC creation and at login. The tables are
// seeded; each test writes its own rows (QuestIDs from 31000 up, NPC 'it-npc')
// and cleans them in SetUp/TearDown.

class QuestInfoMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        clean();
    }
    virtual void TearDown() {
        clean();
    }
    static void clean() {
        execSQL("DELETE FROM MonsterKillQuestInfo WHERE QuestID >= 31000");
        execSQL("DELETE FROM GatherItemQuestInfo WHERE QuestID >= 31000");
        execSQL("DELETE FROM MeetNPCQuestInfo WHERE QuestID >= 31000");
        execSQL("DELETE FROM MiniGameQuestInfo WHERE QuestID >= 31000");
        execSQL("DELETE FROM ItemRewardInfo WHERE NPC='it-npc'");
        execSQL("DELETE FROM SlayerWeaponRewardInfo WHERE NPC='it-npc'");
        execSQL("DELETE FROM EventQuestAdvance WHERE OwnerID='it-owner'");
        execSQL("DELETE FROM EventQuestLootingInfo WHERE QuestLevel >= 31000");
    }
};

TEST_F(QuestInfoMySQL, TheFourCataloguesComeBackPerNPCWithTheirOwnColumns) {
    QuestInfoRepository& repository = defaultQuestInfoRepository();

    execSQL(
        "INSERT INTO MonsterKillQuestInfo (QuestID, NPC, TargetSType, IsChief, Goal, TimeLimitSec, Race, MinGrade,"
        " MaxGrade, RewardClass, EventQuest, QuestLevel) VALUES (31000, 'it-npc', 12, 1, 13, 600, 2, 3, 4, 5, 1, 6)");
    execSQL("INSERT INTO GatherItemQuestInfo (QuestID, NPC, TargetIClass, TargetIType, Goal, TimeLimitSec, Race,"
            " MinGrade, MaxGrade, RewardClass, EventQuest, QuestLevel) VALUES (31001, 'it-npc', 7, 8, 9, 700, 1, 2, 3,"
            " 4, 0, 5)");
    execSQL("INSERT INTO MeetNPCQuestInfo (QuestID, NPC, TargetNPCID, SecondNPCID, TimeLimitSec, Race, MinGrade,"
            " MaxGrade, RewardClass, EventQuest, QuestLevel) VALUES (31002, 'it-npc', 21, 22, 800, 0, 1, 2, 3, 1, 4)");
    execSQL("INSERT INTO MiniGameQuestInfo (QuestID, NPC, GameType, TimeLimitSec, Race, MinGrade, MaxGrade,"
            " RewardClass, EventQuest, QuestLevel) VALUES (31003, 'it-npc', 31, 900, 2, 5, 6, 7, 1, 8)");

    // SimpleQuestInfoManager's nine columns: the head plus TargetSType, IsChief, Goal.
    std::vector<MonsterKillQuestRow> simple = repository.loadMonsterKillQuestsOfNPC("it-npc");
    ASSERT_EQ(1u, simple.size());
    EXPECT_EQ(31000, simple[0].head.questID);
    EXPECT_EQ(2, simple[0].head.race);
    EXPECT_EQ(4, simple[0].head.maxGrade);
    EXPECT_EQ(3, simple[0].head.minGrade);
    EXPECT_EQ(600, simple[0].head.timeLimitSec);
    EXPECT_EQ(5, simple[0].head.rewardClass);
    EXPECT_EQ(12, simple[0].targetSType);
    EXPECT_EQ(1, simple[0].isChief);
    EXPECT_EQ(13, simple[0].goal);
    EXPECT_TRUE(repository.loadMonsterKillQuestsOfNPC("it-other-npc").empty());

    // EventQuestInfoManager reads the same table plus EventQuest and QuestLevel.
    std::vector<EventMonsterKillQuestRow> kills = repository.loadEventMonsterKillQuestsOfNPC("it-npc");
    ASSERT_EQ(1u, kills.size());
    EXPECT_EQ(31000, kills[0].quest.head.questID);
    EXPECT_EQ(12, kills[0].quest.targetSType);
    EXPECT_EQ(13, kills[0].quest.goal);
    EXPECT_EQ(1, kills[0].eventQuest);
    EXPECT_EQ(6, kills[0].questLevel);

    std::vector<EventGatherItemQuestRow> gathers = repository.loadEventGatherItemQuestsOfNPC("it-npc");
    ASSERT_EQ(1u, gathers.size());
    EXPECT_EQ(31001, gathers[0].quest.head.questID);
    EXPECT_EQ(700, gathers[0].quest.head.timeLimitSec);
    EXPECT_EQ(7, gathers[0].quest.targetIClass);
    EXPECT_EQ(8, gathers[0].quest.targetIType);
    EXPECT_EQ(9, gathers[0].quest.goal);
    EXPECT_EQ(0, gathers[0].eventQuest);
    EXPECT_EQ(5, gathers[0].questLevel);

    std::vector<EventMeetNPCQuestRow> meets = repository.loadEventMeetNPCQuestsOfNPC("it-npc");
    ASSERT_EQ(1u, meets.size());
    EXPECT_EQ(31002, meets[0].quest.head.questID);
    EXPECT_EQ(21, meets[0].quest.targetNPCID);
    EXPECT_EQ(22, meets[0].quest.secondNPCID);
    EXPECT_EQ(1, meets[0].eventQuest);
    EXPECT_EQ(4, meets[0].questLevel);

    std::vector<EventMiniGameQuestRow> games = repository.loadEventMiniGameQuestsOfNPC("it-npc");
    ASSERT_EQ(1u, games.size());
    EXPECT_EQ(31003, games[0].quest.head.questID);
    EXPECT_EQ(7, games[0].quest.head.rewardClass);
    EXPECT_EQ(31, games[0].quest.gameType);
    EXPECT_EQ(1, games[0].eventQuest);
    EXPECT_EQ(8, games[0].questLevel);
}

TEST_F(QuestInfoMySQL, TheTwoRewardTablesShareTheirSixColumns) {
    QuestInfoRepository& repository = defaultQuestInfoRepository();

    execSQL("INSERT INTO ItemRewardInfo (RewardClass, NPC, IClass, IType, OptionType, TimeLimitSec) VALUES "
            "(41, 'it-npc', 3, 4, '1,2', 500)");
    execSQL("INSERT INTO SlayerWeaponRewardInfo (RewardClass, NPC, IClass, IType, OptionType, TimeLimitSec) VALUES "
            "(42, 'it-npc', 5, 6, '', 600)");

    std::vector<ItemRewardRow> items = repository.loadItemRewardsOfNPC("it-npc");
    ASSERT_EQ(1u, items.size());
    EXPECT_EQ(41, items[0].rewardClass);
    EXPECT_EQ(atoi(queryScalar("SELECT RewardID FROM ItemRewardInfo WHERE NPC='it-npc'").c_str()), items[0].rewardID);
    EXPECT_EQ(3, items[0].itemClass);
    EXPECT_EQ(4, items[0].itemType);
    EXPECT_EQ("1,2", items[0].optionType);
    EXPECT_EQ(500, items[0].timeLimitSec);

    std::vector<ItemRewardRow> weapons = repository.loadSlayerWeaponRewardsOfNPC("it-npc");
    ASSERT_EQ(1u, weapons.size());
    EXPECT_EQ(42, weapons[0].rewardClass);
    EXPECT_EQ(5, weapons[0].itemClass);
    EXPECT_EQ(6, weapons[0].itemType);
    EXPECT_EQ("", weapons[0].optionType);
    EXPECT_EQ(600, weapons[0].timeLimitSec);

    EXPECT_TRUE(repository.loadItemRewardsOfNPC("it-other-npc").empty());
    EXPECT_TRUE(repository.loadSlayerWeaponRewardsOfNPC("it-other-npc").empty());
}

TEST_F(QuestInfoMySQL, EventQuestAdvanceUpdatesWhenARowExistsAndInsertsWhenItDoesNot) {
    QuestInfoRepository& repository = defaultQuestInfoRepository();

    // No row yet: the UPDATE reports nothing written, which is what sends the
    // caller to the INSERT IGNORE.
    EXPECT_FALSE(repository.updateEventQuestAdvance(2, "it-owner", 3));
    repository.insertEventQuestAdvance(3, "it-owner", 2);
    EXPECT_EQ("2", queryScalar("SELECT Status FROM EventQuestAdvance WHERE OwnerID='it-owner' AND QuestLevel=3"));

    // Now it does: the UPDATE writes and reports it.
    EXPECT_TRUE(repository.updateEventQuestAdvance(4, "it-owner", 3));
    EXPECT_EQ("4", queryScalar("SELECT Status FROM EventQuestAdvance WHERE OwnerID='it-owner' AND QuestLevel=3"));
    // INSERT IGNORE over the (OwnerID, QuestLevel) primary key changes nothing.
    repository.insertEventQuestAdvance(3, "it-owner", 9);
    EXPECT_EQ("4", queryScalar("SELECT Status FROM EventQuestAdvance WHERE OwnerID='it-owner' AND QuestLevel=3"));

    repository.insertEventQuestAdvance(5, "it-owner", 1);
    std::vector<EventQuestAdvanceRow> rows = repository.loadEventQuestAdvances("it-owner");
    ASSERT_EQ(2u, rows.size());
    EXPECT_TRUE(repository.loadEventQuestAdvances("it-other-owner").empty());
}

TEST_F(QuestInfoMySQL, LootingInfosCarryTheirTypeMinusOne) {
    QuestInfoRepository& repository = defaultQuestInfoRepository();

    execSQL("INSERT INTO EventQuestLootingInfo (QuestLevel, LootingType, LootingZone, LootingMType, LootingIClass,"
            " LootingITypeMin, LootingITypeMax, Race, MinGrade, MaxGrade) VALUES (31000, 'MONSTER', 11, 12, 13, 14,"
            " 15, 1, 2, 3)");

    std::vector<EventQuestLootingRow> rows = repository.loadEventQuestLootingInfos();
    bool found = false;
    for (size_t i = 0; i < rows.size(); i++) {
        if (rows[i].questLevel != 31000)
            continue;
        found = true;
        // The enum's second label, read as an index and decremented by the SELECT.
        EXPECT_EQ(1, rows[i].lootingType);
        EXPECT_EQ(11, rows[i].lootingZone);
        EXPECT_EQ(12, rows[i].lootingMType);
        EXPECT_EQ(13, rows[i].lootingIClass);
        EXPECT_EQ(14, rows[i].lootingITypeMin);
        EXPECT_EQ(15, rows[i].lootingITypeMax);
        EXPECT_EQ(1, rows[i].race);
        EXPECT_EQ(2, rows[i].minGrade);
        EXPECT_EQ(3, rows[i].maxGrade);
    }
    EXPECT_TRUE(found);
}

TEST(ConfigLoadersMySQL, OptionInfoIsReadInSelectOrderAndTheOtherOptionTablesAreSeeded) {
    GameInfoRepository& repository = defaultGameInfoRepository();

    std::vector<OptionInfoRow> options = repository.loadOptionInfos();
    ASSERT_FALSE(options.empty());
    // The schema orders UpgradeOptionType before PreviousOptionType; the
    // SELECT (and so the row) the other way round. Pin the mapping.
    std::string id = std::to_string(options[0].optionType);
    EXPECT_EQ(options[0].name, queryScalar("SELECT Name FROM OptionInfo WHERE OptionType=" + id));
    EXPECT_EQ(options[0].nickname, queryScalar("SELECT Nickname FROM OptionInfo WHERE OptionType=" + id));
    EXPECT_EQ(std::to_string(options[0].optionClass),
              queryScalar("SELECT Class FROM OptionInfo WHERE OptionType=" + id));
    EXPECT_EQ(std::to_string(options[0].previousOptionType),
              queryScalar("SELECT PreviousOptionType FROM OptionInfo WHERE OptionType=" + id));
    EXPECT_EQ(std::to_string(options[0].upgradeOptionType),
              queryScalar("SELECT UpgradeOptionType FROM OptionInfo WHERE OptionType=" + id));
    EXPECT_EQ(std::to_string(options[0].grade), queryScalar("SELECT Grade FROM OptionInfo WHERE OptionType=" + id));

    EXPECT_FALSE(repository.loadOptionClassInfos().empty());
    EXPECT_FALSE(repository.loadRareEnchantInfos().empty());
    EXPECT_FALSE(repository.loadPetEnchantOptionRatios().empty());
}

// --- the content-info cluster against real MySQL ---------------------------
// Monsters, skill balance, NPCs, scripts, directive sets and variables. The
// tables are seeded; the tests insert their own rows (ids from 31000 up)
// and clean them in SetUp/TearDown.

class ContentInfoMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        clean();
    }
    virtual void TearDown() {
        clean();
    }
    static void clean() {
        execSQL("DELETE FROM MonsterInfo WHERE MType >= 31000");
        execSQL("DELETE FROM SkillBalance WHERE Type >= 31000");
        execSQL("DELETE FROM NPC WHERE ZoneID >= 31000");
        execSQL("DELETE FROM Script WHERE ScriptID >= 31000");
        execSQL("DELETE FROM Triggers WHERE TriggerID >= 31000");
        execSQL("DELETE FROM DirectiveSet WHERE ID >= 31000");
        execSQL("DELETE FROM AttrInfo WHERE attrID >= 31000");
    }
    static int scalar(const std::string& sql) {
        return atoi(queryScalar(sql).c_str());
    }
};

TEST_F(ContentInfoMySQL, SeededReadsAreNonEmptyAndEveryMaxProbeMatchesItsTable) {
    ContentInfoRepository& repository = defaultContentInfoRepository();
    int maximum = -1;

    ASSERT_TRUE(repository.loadMaxMonsterType(maximum));
    EXPECT_EQ(scalar("SELECT MAX(MType) FROM MonsterInfo"), maximum);
    std::vector<MonsterInfoRow> monsters = repository.loadMonsterInfos();
    EXPECT_FALSE(monsters.empty());
    EXPECT_EQ(monsters.size(), repository.loadMonsterSummonInfos().size());
    EXPECT_EQ(monsters.size(), repository.loadMonsterInfosForReload().size());

    ASSERT_TRUE(repository.loadMaxSkillBalanceType(maximum));
    EXPECT_EQ(scalar("SELECT MAX(Type) FROM SkillBalance"), maximum);
    EXPECT_FALSE(repository.loadSkillBalances().empty());

    EXPECT_FALSE(repository.loadScripts().empty());

    ASSERT_TRUE(repository.loadMaxDirectiveSetID(maximum));
    EXPECT_EQ(scalar("SELECT MAX(ID) FROM DirectiveSet"), maximum);
    EXPECT_FALSE(repository.loadDirectiveSets().empty());

    ASSERT_TRUE(repository.loadMaxAttrID(maximum));
    EXPECT_EQ(scalar("SELECT MAX(attrID) FROM AttrInfo"), maximum);
    EXPECT_FALSE(repository.loadVariables().empty());

    ASSERT_TRUE(defaultZoneInfoRepository().loadMaxZoneGroupID(maximum));
    EXPECT_EQ(scalar("SELECT MAX(ZoneGroupID) FROM ZoneGroupInfo"), maximum);
}

TEST_F(ContentInfoMySQL, MonsterRowsCarryEveryColumnAndReloadReadsOneTypeOrAll) {
    execSQL("INSERT INTO MonsterInfo (MType, SType, HName, EName, Level, STR, DEX, INTE, BSize, Exp, MColor, SColor, "
            "Align, AOrder, Moral, Delay, ADelay, Sight, MeleeRange, MissileRange, RegenPortal, RegenInvisible, "
            "RegenBat, UnburrowChance, MMode, AIType, Enhance, Master, ClanType, MonsterSummonInfo, DefaultEffects, "
            "Chief, NormalRegen, HasTreasure, MonsterClass, SkullType) VALUES (31000, 2, 'it-h', 'it-e', 5, 6, 7, 8, "
            "9, 10, 11, 12, 1, 2, 15, 16, 17, 18, 19, 20, 21, 22, 23, 27, 'FLY', 25, 'e26', 1, 29, 's30', 'd31', 1, "
            "0, 1, 35, 36)");

    ContentInfoRepository& repository = defaultContentInfoRepository();

    std::vector<MonsterInfoRow> all = repository.loadMonsterInfos();
    const MonsterInfoRow* mine = NULL;
    for (size_t r = 0; r < all.size(); r++)
        if (all[r].monsterType == 31000)
            mine = &all[r];
    ASSERT_TRUE(mine != NULL);
    EXPECT_EQ(2, mine->spriteType);
    EXPECT_EQ("it-h", mine->hName);
    EXPECT_EQ("it-e", mine->eName);
    EXPECT_EQ(5, mine->level);
    EXPECT_EQ(6, mine->str);
    EXPECT_EQ(7, mine->dex);
    EXPECT_EQ(8, mine->inte);
    EXPECT_EQ(9, mine->bodySize);
    EXPECT_EQ(10, mine->exp);
    EXPECT_EQ(11, mine->mainColor);
    EXPECT_EQ(12, mine->subColor);
    EXPECT_EQ(1, mine->alignment);
    EXPECT_EQ(2, mine->attackOrder);
    EXPECT_EQ(15, mine->moral);
    EXPECT_EQ(16, mine->delay);
    EXPECT_EQ(17, mine->attackDelay);
    EXPECT_EQ(18, mine->sight);
    EXPECT_EQ(19, mine->meleeRange);
    EXPECT_EQ(20, mine->missileRange);
    EXPECT_EQ(21, mine->regenPortal);
    EXPECT_EQ(22, mine->regenInvisible);
    EXPECT_EQ(23, mine->regenBat);
    EXPECT_EQ("FLY", mine->moveMode);
    EXPECT_EQ(25, mine->aiType);
    EXPECT_EQ("e26", mine->enhance);
    EXPECT_EQ(27, mine->unburrowChance);
    EXPECT_EQ(1, mine->master);
    EXPECT_EQ(29, mine->clanType);
    EXPECT_EQ("d31", mine->defaultEffects);
    EXPECT_EQ(1, mine->chief);
    EXPECT_EQ(0, mine->normalRegen);
    EXPECT_EQ(1, mine->hasTreasure);
    EXPECT_EQ(35, mine->monsterClass);
    EXPECT_EQ(36, mine->skullType);

    std::vector<MonsterSummonRow> summons = repository.loadMonsterSummonInfos();
    bool seenSummon = false;
    for (size_t r = 0; r < summons.size(); r++) {
        if (summons[r].monsterType == 31000) {
            seenSummon = true;
            EXPECT_EQ("s30", summons[r].summonInfo);
        }
    }
    EXPECT_TRUE(seenSummon);

    std::vector<MonsterReloadRow> one = repository.loadMonsterInfoForReload(31000);
    ASSERT_EQ(1u, one.size());
    EXPECT_EQ(31000, one[0].monsterType);
    EXPECT_EQ("it-e", one[0].eName);
    EXPECT_EQ(20, one[0].missileRange);
    EXPECT_EQ("FLY", one[0].moveMode);
    EXPECT_EQ("s30", one[0].monsterSummonInfo);
    EXPECT_EQ("d31", one[0].defaultEffects);
    EXPECT_EQ(0, one[0].normalRegen);
    EXPECT_TRUE(repository.loadMonsterInfoForReload(31001).empty());
    EXPECT_EQ(all.size(), repository.loadMonsterInfosForReload().size());
}

TEST_F(ContentInfoMySQL, SkillBalanceRowsCarryEveryColumnWithByteDomains) {
    execSQL("INSERT INTO SkillBalance (Type, Name, HName, Level, MinDam, MaxDam, MinDelay, MaxDelay, MinDur, MaxDur, "
            "Mana, MinRange, MaxRange, Target, SubExp, Point, Domain, MagicDomain, Melee, Magic, Physic, SkillPoint, "
            "LevelUpPoint, RequireSkill, `Condition`, ElementalDomain, CanDelete) VALUES (31000, 'it-skill', '', 3, "
            "4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 3, 4, 18, 19, 20, 21, 22, 'r23', 'c24', 25, 1)");

    std::vector<SkillBalanceRow> rows = defaultContentInfoRepository().loadSkillBalances();
    const SkillBalanceRow* mine = NULL;
    for (size_t r = 0; r < rows.size(); r++)
        if (rows[r].type == 31000)
            mine = &rows[r];
    ASSERT_TRUE(mine != NULL);
    EXPECT_EQ("it-skill", mine->name);
    EXPECT_EQ(3, mine->level);
    EXPECT_EQ(4, mine->minDamage);
    EXPECT_EQ(5, mine->maxDamage);
    EXPECT_EQ(6, mine->minDelay);
    EXPECT_EQ(7, mine->maxDelay);
    EXPECT_EQ(8, mine->minDuration);
    EXPECT_EQ(9, mine->maxDuration);
    EXPECT_EQ(10, mine->mana);
    EXPECT_EQ(11, mine->minRange);
    EXPECT_EQ(12, mine->maxRange);
    EXPECT_EQ(13, mine->target);
    EXPECT_EQ(14, mine->subExp);
    EXPECT_EQ(15, mine->point);
    EXPECT_EQ(3, (int)mine->domain);
    EXPECT_EQ(4, (int)mine->magicDomain);
    EXPECT_EQ(18, mine->melee);
    EXPECT_EQ(19, mine->magic);
    EXPECT_EQ(20, mine->physic);
    EXPECT_EQ(21, mine->skillPoint);
    EXPECT_EQ(22, mine->levelUpPoint);
    EXPECT_EQ("r23", mine->requireSkill);
    EXPECT_EQ("c24", mine->condition);
    EXPECT_EQ(25, mine->elementalDomain);
    EXPECT_EQ(1, mine->canDelete);
}

TEST_F(ContentInfoMySQL, NPCsAreScopedToTheZoneAndOptionallyTheRace) {
    execSQL("INSERT INTO NPC (Name, NPCID, SpriteType, Race, MainColor, SubColor, ZoneID, ClanType, ShowInMinimap, "
            "Description, TaxingCastleZoneID) VALUES ('it-npc-a', 1, 2, 1, 4, 5, 31000, 7, 1, '', 9)");
    execSQL("INSERT INTO NPC (Name, NPCID, SpriteType, Race, MainColor, SubColor, ZoneID, ClanType, ShowInMinimap, "
            "Description, TaxingCastleZoneID) VALUES ('it-npc-b', 11, 12, 2, 14, 15, 31000, 17, 0, '', 19)");
    execSQL("INSERT INTO NPC (Name, NPCID, SpriteType, Race, MainColor, SubColor, ZoneID, ClanType, ShowInMinimap, "
            "Description, TaxingCastleZoneID) VALUES ('it-npc-c', 21, 22, 2, 24, 25, 31001, 27, 1, '', 29)");

    ContentInfoRepository& repository = defaultContentInfoRepository();

    std::vector<NPCRow> zone = repository.loadNPCs(31000);
    ASSERT_EQ(2u, zone.size());

    std::vector<NPCRow> race = repository.loadNPCsOfRace(31000, 2);
    ASSERT_EQ(1u, race.size());
    EXPECT_EQ("it-npc-b", race[0].name);
    EXPECT_EQ(11, race[0].npcID);
    EXPECT_EQ(12, race[0].spriteType);
    EXPECT_EQ(2, race[0].race);
    EXPECT_EQ(14, race[0].mainColor);
    EXPECT_EQ(15, race[0].subColor);
    EXPECT_EQ(17, race[0].clanType);
    EXPECT_EQ(0, race[0].showInMinimap);
    EXPECT_EQ(19, race[0].taxingCastleZoneID);

    EXPECT_TRUE(repository.loadNPCsOfRace(31000, 3).empty());
    EXPECT_EQ(1u, repository.loadNPCs(31001).size());
}

// quest/TriggerManager::load(name): an NPC's Triggers rows, four columns,
// texts untrimmed (the caller trims), other NPCs' rows left out.
TEST_F(ContentInfoMySQL, NPCTriggersAreScopedToTheNPC) {
    execSQL("INSERT INTO Triggers (TriggerID, TriggerType, NPC, Conditions, Actions) "
            "VALUES (31000, 'NPC', 'it-npc', ' c0 ', 'a0')");
    execSQL("INSERT INTO Triggers (TriggerID, TriggerType, NPC, Conditions, Actions) "
            "VALUES (31001, 'QUEST', 'it-npc', 'c1', 'a1')");
    execSQL("INSERT INTO Triggers (TriggerID, TriggerType, NPC, Conditions, Actions) "
            "VALUES (31002, 'NPC', 'it-other', 'c2', 'a2')");

    std::vector<NPCTriggerRow> rows = defaultContentInfoRepository().loadNPCTriggers("it-npc");
    ASSERT_EQ(2u, rows.size());
    int seen = 0;
    for (size_t r = 0; r < rows.size(); r++) {
        if (rows[r].triggerID == 31000) {
            EXPECT_EQ("NPC", rows[r].triggerType);
            EXPECT_EQ(" c0 ", rows[r].conditions);
            EXPECT_EQ("a0", rows[r].actions);
            seen++;
        } else if (rows[r].triggerID == 31001) {
            EXPECT_EQ("QUEST", rows[r].triggerType);
            EXPECT_EQ("c1", rows[r].conditions);
            seen++;
        }
    }
    EXPECT_EQ(2, seen);
    EXPECT_TRUE(defaultContentInfoRepository().loadNPCTriggers("it-nobody").empty());
}

TEST_F(ContentInfoMySQL, ScriptsComeBackOrderedByScriptID) {
    execSQL("INSERT INTO Script (ScriptID, OwnerID, Subject, Content) VALUES (31001, 'it-owner', 's1**s2', 'c1')");
    execSQL("INSERT INTO Script (ScriptID, OwnerID, Subject, Content) VALUES (31000, 'it-owner', 's0', 'c0**c1')");

    std::vector<ScriptRow> rows = defaultContentInfoRepository().loadScripts();
    int at31000 = -1, at31001 = -1;
    for (size_t r = 0; r < rows.size(); r++) {
        if (rows[r].scriptID == 31000) {
            at31000 = (int)r;
            EXPECT_EQ("it-owner", rows[r].ownerID);
            EXPECT_EQ("s0", rows[r].subject);
            EXPECT_EQ("c0**c1", rows[r].content);
        }
        if (rows[r].scriptID == 31001)
            at31001 = (int)r;
    }
    ASSERT_NE(-1, at31000);
    ASSERT_NE(-1, at31001);
    EXPECT_LT(at31000, at31001); // ORDER BY ScriptID, despite the reversed insert order
    for (size_t r = 1; r < rows.size(); r++)
        EXPECT_LT(rows[r - 1].scriptID, rows[r].scriptID);
}

TEST_F(ContentInfoMySQL, DirectiveSetsAndVariablesAreReadAndTheVariableSaved) {
    execSQL("INSERT INTO DirectiveSet (ID, Name, Content, DeadContent) VALUES (31000, 'it-set', 'live', 'dead')");
    execSQL("INSERT INTO AttrInfo (attrID, attr1, attr2, comm) VALUES (31000, 5, 6, 'it')");

    ContentInfoRepository& repository = defaultContentInfoRepository();

    int maximum = -1;
    ASSERT_TRUE(repository.loadMaxDirectiveSetID(maximum));
    EXPECT_EQ(31000, maximum);
    std::vector<DirectiveSetRow> sets = repository.loadDirectiveSets();
    bool seenSet = false;
    for (size_t r = 0; r < sets.size(); r++) {
        if (sets[r].id == 31000) {
            seenSet = true;
            EXPECT_EQ("it-set", sets[r].name);
            EXPECT_EQ("live", sets[r].content);
            EXPECT_EQ("dead", sets[r].deadContent);
        }
    }
    EXPECT_TRUE(seenSet);

    ASSERT_TRUE(repository.loadMaxAttrID(maximum));
    EXPECT_EQ(31000, maximum);
    std::vector<VariableRow> variables = repository.loadVariables();
    bool seenVariable = false;
    for (size_t r = 0; r < variables.size(); r++) {
        if (variables[r].attrID == 31000) {
            seenVariable = true;
            EXPECT_EQ(5, variables[r].attr1);
            EXPECT_EQ(6, variables[r].attr2);
        }
    }
    EXPECT_TRUE(seenVariable);

    repository.saveVariable(9, 31000);
    EXPECT_EQ("9", queryScalar("SELECT attr1 FROM AttrInfo WHERE attrID=31000"));
    EXPECT_EQ("6", queryScalar("SELECT attr2 FROM AttrInfo WHERE attrID=31000"));
}

// --- the play-record cluster against real MySQL -----------------------------
// Saved quest states, the head-count log and the minigame score board. All
// three tables are unseeded or log-only; the tests use 'it-' owners.

class PlayRecordMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        clean();
    }
    virtual void TearDown() {
        clean();
    }
    static void clean() {
        execSQL("DELETE FROM GQuestSave WHERE OwnerID LIKE 'it-%'");
        execSQL("DELETE FROM HeadCount WHERE Name LIKE 'it-%'");
        execSQL("DELETE FROM MiniGameScores WHERE Name LIKE 'it-%'");
        execSQL("DELETE FROM TradeLog WHERE Name1 LIKE 'it-%'");
        execSQL("DELETE FROM EventLotto WHERE PlayerID LIKE 'it-%'");
        execSQL("DELETE FROM UnderworldEvent WHERE PlayerID LIKE 'it-%'");
    }
};

TEST_F(PlayRecordMySQL, SavedQuestsAreReplacedLoadedPerOwnerAndDeleted) {
    PlayRecordRepository& repository = defaultPlayRecordRepository();
    repository.replaceSavedQuest(31000, "it-quester", 3);
    repository.replaceSavedQuest(31001, "it-quester", 4);
    repository.replaceSavedQuest(31000, "it-other", 1);

    std::vector<SavedQuestRow> rows = repository.loadSavedQuests("it-quester");
    ASSERT_EQ(2u, rows.size());
    bool seen31000 = false;
    for (size_t r = 0; r < rows.size(); r++) {
        if (rows[r].questID == 31000) {
            seen31000 = true;
            EXPECT_EQ(3, rows[r].status);
        }
        EXPECT_TRUE(rows[r].secondsSinceSave >= 0 && rows[r].secondsSinceSave <= 5);
    }
    EXPECT_TRUE(seen31000);

    // REPLACE on the (QuestID, OwnerID) key rewrites in place.
    repository.replaceSavedQuest(31000, "it-quester", 5);
    EXPECT_EQ("2", queryScalar("SELECT COUNT(*) FROM GQuestSave WHERE OwnerID='it-quester'"));
    EXPECT_EQ("5", queryScalar("SELECT Status FROM GQuestSave WHERE OwnerID='it-quester' AND QuestID=31000"));

    repository.deleteSavedQuest("it-quester", 31000);
    rows = repository.loadSavedQuests("it-quester");
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(31001, rows[0].questID);
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM GQuestSave WHERE OwnerID='it-other'"));
}

TEST_F(PlayRecordMySQL, HeadCountRowsCarryTheLevelsCountAndAServerSideTime) {
    defaultPlayRecordRepository().insertHeadCount("it-head", 30, 35, 7);

    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM HeadCount WHERE Name='it-head'"));
    EXPECT_EQ("30", queryScalar("SELECT FirstLevel FROM HeadCount WHERE Name='it-head'"));
    EXPECT_EQ("35", queryScalar("SELECT LastLevel FROM HeadCount WHERE Name='it-head'"));
    EXPECT_EQ("7", queryScalar("SELECT HeadCount FROM HeadCount WHERE Name='it-head'"));
    EXPECT_NE("2003-01-01 00:00:00", queryScalar("SELECT Time FROM HeadCount WHERE Name='it-head'"));
}

TEST_F(PlayRecordMySQL, MiniGameScoreReadReportsTheRowOrNone) {
    PlayRecordRepository& repository = defaultPlayRecordRepository();
    std::string name;
    int score = -1;

    EXPECT_FALSE(repository.loadMiniGameScore(120, 5, name, score));

    execSQL("INSERT INTO MiniGameScores (Name, Type, Level, Score) VALUES ('it-scorer', 120, 5, 999)");
    ASSERT_TRUE(repository.loadMiniGameScore(120, 5, name, score));
    EXPECT_EQ("it-scorer", name);
    EXPECT_EQ(999, score);
    EXPECT_FALSE(repository.loadMiniGameScore(120, 6, name, score)); // other level
}

// CGSubmitScoreHandler: the UPDATE replaces a standing (type, level) row
// whose Score is GREATER than the submitted one (whether that means the
// board keeps low scores or is simply inverted is not knowable from the
// server: the live read has no ORDER BY, and the two commented-out reads
// in the tree order opposite ways); a higher submission changes nothing,
// another level's row is never touched, and with no row there is nothing
// to update (the statement never inserts). LIMIT 1 is not pinned: the
// test never seeds two beatable rows for one (type, level).
TEST_F(PlayRecordMySQL, MiniGameScoreReplacesOnlyAWorseRowOfTheSameTypeAndLevel) {
    PlayRecordRepository& repository = defaultPlayRecordRepository();
    execSQL("INSERT INTO MiniGameScores (Name, Type, Level, Score) VALUES ('it-scorer', 120, 5, 999)");
    execSQL("INSERT INTO MiniGameScores (Name, Type, Level, Score) VALUES ('it-scorer', 120, 6, 999)");

    repository.recordMiniGameScore("it-new", 500, 120, 5);
    EXPECT_EQ("it-new", queryScalar("SELECT Name FROM MiniGameScores WHERE Type=120 AND Level=5"));
    EXPECT_EQ("500", queryScalar("SELECT Score FROM MiniGameScores WHERE Type=120 AND Level=5"));
    EXPECT_EQ("it-scorer", queryScalar("SELECT Name FROM MiniGameScores WHERE Type=120 AND Level=6"));

    repository.recordMiniGameScore("it-worse", 700, 120, 5); // 500 > 700 is false
    EXPECT_EQ("it-new", queryScalar("SELECT Name FROM MiniGameScores WHERE Type=120 AND Level=5"));

    repository.recordMiniGameScore("it-new", 1, 120, 7); // no row for this level
    EXPECT_EQ("0", queryScalar("SELECT COUNT(*) FROM MiniGameScores WHERE Type=120 AND Level=7"));
}

// CreatureUtil's lotto counter: the first add of a (player, type) takes the
// REPLACE (the UPDATE changed no row) and reads back num; the next takes
// the UPDATE and reads back the sum; another type is its own row.
TEST_F(PlayRecordMySQL, LottoCountIsReplacedThenAddedToPerPlayerAndType) {
    PlayRecordRepository& repository = defaultPlayRecordRepository();
    int count = -1;

    ASSERT_TRUE(repository.addLotto("it-acct", 3, 2, count));
    EXPECT_EQ(2, count);
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM EventLotto WHERE PlayerID = 'it-acct'"));
    ASSERT_TRUE(repository.addLotto("it-acct", 3, 5, count));
    EXPECT_EQ(7, count);
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM EventLotto WHERE PlayerID = 'it-acct'"));
    ASSERT_TRUE(repository.addLotto("it-acct", 4, 1, count));
    EXPECT_EQ(1, count);
    EXPECT_EQ("7", queryScalar("SELECT count FROM EventLotto WHERE PlayerID = 'it-acct' AND Type = 3"));
}

// CreatureUtil's gold medal: the table is not in initdb/, so the INSERT
// fails on the shipped schema — the bug the header records, pinned here
// as the DatabaseError END_DB throws.
TEST_F(PlayRecordMySQL, GoldMedalInsertFailsOnTheShippedSchema) {
    EXPECT_EQ("0", queryScalar("SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = DATABASE() "
                               "AND table_name = 'GoldMedalCount'"));
    EXPECT_THROW(defaultPlayRecordRepository().insertGoldMedal("it-acct"), DatabaseError);
}

// CGBuyStoreItemHandler: the store-purchase TradeLog row, the two names in
// their columns AND inside the Content text, the price at the end of it.
TEST_F(PlayRecordMySQL, StoreTradeIsLoggedWithBothNamesInTheContent) {
    defaultPlayRecordRepository().logStoreTrade("2026-09-06 10:00:00", "it-store", "10.0.0.1", "it-acct1", "it-buyer",
                                                "10.0.0.2", "it-acct2", "ITEM(31000)", 123);
    const std::string where = " FROM TradeLog WHERE Name1 = 'it-store'";
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*)" + where));
    EXPECT_EQ("2026-09-06 10:00:00", queryScalar("SELECT Timeline" + where));
    EXPECT_EQ("it-buyer", queryScalar("SELECT Name2" + where));
    EXPECT_EQ("10.0.0.1", queryScalar("SELECT IP1" + where));
    EXPECT_EQ("10.0.0.2", queryScalar("SELECT IP2" + where));
    EXPECT_EQ("Store:[it-store(it-acct1)]\nITEM(31000)\n----\nBuy:[it-buyer(it-acct2)]\nGOLD:123\n",
              queryScalar("SELECT Content" + where));
}

// TradeManager::processTrade: the player-to-player TradeLog row. Its
// content is the caller's whole transcript of the trade, both sides'
// name, account, gold and one line per item.
TEST_F(PlayRecordMySQL, PlayerTradeIsLoggedWithBothSidesInTheContent) {
    const std::string content = "[it-one(it-acct1)] \nGOLD:10\n----\n[it-two(it-acct2)] \nGOLD:20\n";
    defaultPlayRecordRepository().logPlayerTrade("2026-09-10 11:22:33", "it-one", "10.0.0.1", "it-two", "10.0.0.2",
                                                 content);
    const std::string where = " FROM TradeLog WHERE Name1 = 'it-one'";
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*)" + where));
    EXPECT_EQ("2026-09-10 11:22:33", queryScalar("SELECT Timeline" + where));
    EXPECT_EQ("it-two", queryScalar("SELECT Name2" + where));
    EXPECT_EQ("10.0.0.1", queryScalar("SELECT IP1" + where));
    EXPECT_EQ("10.0.0.2", queryScalar("SELECT IP2" + where));
    EXPECT_EQ(content, queryScalar("SELECT Content" + where));
}

// Why this row is assembled and sent through executeQueryString instead
// of formatted: a trade of two loaded inventories writes a transcript far
// past executeQuery's 2048-byte format buffer, and it must land whole.
TEST_F(PlayRecordMySQL, PlayerTradeContentPastTheFormatBufferLandsWhole) {
    const std::string content(6000, 'x');
    defaultPlayRecordRepository().logPlayerTrade("2026-09-10 11:22:33", "it-one", "10.0.0.1", "it-two", "10.0.0.2",
                                                 content);
    EXPECT_EQ("6000", queryScalar("SELECT LENGTH(Content) FROM TradeLog WHERE Name1 = 'it-one'"));
}

// --- the session cluster against real MySQL ----------------------------------
// Session end, the boot sweep, the PC-room lotto and the NetMarble user
// count. Player/PCRoom rows go through the dist connection (same schema),
// UserStatus through the USERINFO connection the tier now registers. All
// rows use 'it-' ids or ids from 31000 up and are cleaned in SetUp/TearDown.

class SessionMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        clean();
    }
    virtual void TearDown() {
        clean();
    }
    static void clean() {
        execSQL("DELETE FROM GuildMember WHERE Name LIKE 'it-%'");
        execSQL("DELETE FROM Player WHERE PlayerID LIKE 'it-%'");
        execSQL("DELETE FROM PCRoomUserInfo WHERE PlayerID LIKE 'it-%'");
        execSQL("DELETE FROM PCRoomLottoObject WHERE PlayerID LIKE 'it-%'");
        execSQL("DELETE FROM UserIPInfo WHERE Name LIKE 'it-%'");
        execSQL("DELETE FROM SpeedHackPlayer WHERE PlayerID LIKE 'it-%'");
        execSQL("DELETE FROM CrashReportLog WHERE PlayerID LIKE 'it-%'");
        execSQL("DELETE FROM BugReportLog WHERE PlayerID LIKE 'it-%'");
        execSQL("DELETE FROM CrashLog WHERE PlayerID LIKE 'it-%'");
        execSQL("DELETE FROM USERINFO.UserStatus WHERE ServerID >= 31000");
    }
};

TEST_F(SessionMySQL, TheWhisperLocationReadReachesTheSameRowsAsThePlayerDbName) {
    SessionRepository& repository = defaultSessionRepository();

    execSQL("INSERT INTO Player (PlayerID, CurrentServerGroupID, LogOn) "
            "VALUES ('it-where', 9, 'GAME')");

    int serverGroupID = -1;
    std::string logOn = "untouched";
    ASSERT_TRUE(repository.loadPlayerLocation("it-where", serverGroupID, logOn));
    EXPECT_EQ(9, serverGroupID);
    EXPECT_EQ("GAME", logOn);

    // This is the point of the test. loadPlayerLocation asks
    // getDistConnection for "USERINFO" where every other Player
    // statement asks for "PLAYER_DB"; DatabaseManager ignores the name
    // and both reach the same socket. A write through the PLAYER_DB-named
    // path landing in the row the USERINFO-named path reads is what
    // establishes that.
    execSQL("UPDATE Player SET LogOn='LOGOFF' WHERE PlayerID='it-where'");
    ASSERT_TRUE(repository.markPlayerLoggedOn("it-where")) << "the LOGOFF row was not claimed";
    ASSERT_TRUE(repository.loadPlayerLocation("it-where", serverGroupID, logOn));
    EXPECT_EQ("GAME", logOn);

    logOn = "untouched";
    EXPECT_FALSE(repository.loadPlayerLocation("it-nobody", serverGroupID, logOn));
    EXPECT_EQ("untouched", logOn);
}

TEST_F(SessionMySQL, TheSessionHandshakeReadsTheAccountThenClaimsItExactlyOnce) {
    SessionRepository& repository = defaultSessionRepository();

    execSQL("INSERT INTO Player (PlayerID, CurrentServerGroupID, LogOn, SpecialEventCount, PayType, PayPlayDate, "
            "PayPlayHours, PayPlayFlag, BillingUserKey, FamilyPayPlayDate) VALUES "
            "('it-acct', 7, 'LOGOFF', 42, 3, '2026-01-02 03:04:05', 11, 22, 33, '2026-02-03 04:05:06')");

    PlayerSessionRow row;
    ASSERT_TRUE(repository.loadPlayerSession("it-acct", row));
    EXPECT_EQ("it-acct", row.playerID);
    EXPECT_EQ(7, row.serverGroupID);
    EXPECT_EQ("LOGOFF", row.logOn);
    EXPECT_EQ(42u, row.specialEventCount);
    EXPECT_EQ(3, row.payType);
    EXPECT_EQ("2026-01-02 03:04:05", row.payPlayDate);
    EXPECT_EQ(11, row.payPlayHours);
    EXPECT_EQ(22, row.payPlayFlag);
    EXPECT_EQ(33, row.billingUserKey);
    EXPECT_EQ("2026-02-03 04:05:06", row.familyPayPlayDate);

    // The columns are read positionally, and every value above is
    // distinct, so a projection that drifted out of step with the reads
    // shows up here. birthday is never selected, so it stays empty.

    // Claiming the session is the double-login guard: the UPDATE only
    // matches a row still in LOGOFF, so the SECOND caller gets false and
    // the handler throws rather than letting two clients in.
    EXPECT_TRUE(repository.markPlayerLoggedOn("it-acct"));
    EXPECT_EQ("GAME", queryScalar("SELECT LogOn FROM Player WHERE PlayerID='it-acct'"));
    EXPECT_FALSE(repository.markPlayerLoggedOn("it-acct"));
    EXPECT_EQ("GAME", queryScalar("SELECT LogOn FROM Player WHERE PlayerID='it-acct'"));

    // And the mirror puts it back, which is what makes a second login
    // possible at all.
    repository.markPlayerLoggedOff("it-acct");
    EXPECT_EQ("LOGOFF", queryScalar("SELECT LogOn FROM Player WHERE PlayerID='it-acct'"));
    EXPECT_TRUE(repository.markPlayerLoggedOn("it-acct"));

    // An account with no row is false, not a throw — the caller turns
    // that into its own ProtocolException.
    EXPECT_FALSE(repository.loadPlayerSession("it-nobody", row));
    EXPECT_FALSE(repository.markPlayerLoggedOn("it-nobody"));
}

TEST_F(SessionMySQL, GuildMemberLogOnAndLogOffAreMirrorsScopedToOneMember) {
    SessionRepository& repository = defaultSessionRepository();

    execSQL("INSERT INTO GuildMember (GuildID, Name, Rank, Intro, LogOn) VALUES (5, 'it-on-a', 1, '', 0)");
    execSQL("INSERT INTO GuildMember (GuildID, Name, Rank, Intro, LogOn) VALUES (5, 'it-on-b', 1, '', 0)");

    repository.markGuildMemberLoggedOn("it-on-a");
    EXPECT_EQ("1", queryScalar("SELECT LogOn FROM GuildMember WHERE Name='it-on-a'"));
    EXPECT_EQ("0", queryScalar("SELECT LogOn FROM GuildMember WHERE Name='it-on-b'"));

    repository.markGuildMemberLoggedOff("it-on-a");
    EXPECT_EQ("0", queryScalar("SELECT LogOn FROM GuildMember WHERE Name='it-on-a'"));

    // A name with no row is a silent no-op, as the connect path assumes.
    // Assert it rather than just calling it: without these three lines
    // the call proved only that it does not throw.
    repository.markGuildMemberLoggedOn("it-none");
    EXPECT_EQ("", queryScalar("SELECT LogOn FROM GuildMember WHERE Name='it-none'"));
    EXPECT_EQ("0", queryScalar("SELECT LogOn FROM GuildMember WHERE Name='it-on-a'"));
    EXPECT_EQ("0", queryScalar("SELECT LogOn FROM GuildMember WHERE Name='it-on-b'"));
}

TEST_F(SessionMySQL, GuildMemberLogOffFlagsOnlyThatMember) {
    execSQL("INSERT INTO GuildMember (GuildID, Name, Rank, Intro, LogOn) VALUES (5, 'it-gm-a', 1, '', 1)");
    execSQL("INSERT INTO GuildMember (GuildID, Name, Rank, Intro, LogOn) VALUES (5, 'it-gm-b', 1, '', 1)");

    defaultSessionRepository().markGuildMemberLoggedOff("it-gm-a");

    EXPECT_EQ("0", queryScalar("SELECT LogOn FROM GuildMember WHERE Name='it-gm-a'"));
    EXPECT_EQ("1", queryScalar("SELECT LogOn FROM GuildMember WHERE Name='it-gm-b'"));
}

TEST_F(SessionMySQL, SessionEndFlipsOnlyGameRowsAndTheBootSweepClearsThisServersAccounts) {
    execSQL("INSERT INTO Player (PlayerID, LogOn, CurrentWorldID, CurrentServerGroupID) VALUES ('it-p1', 'GAME', 250, "
            "251)");
    execSQL("INSERT INTO Player (PlayerID, LogOn, CurrentWorldID, CurrentServerGroupID) VALUES ('it-p2', 'LOGON', 250, "
            "251)");
    execSQL("INSERT INTO Player (PlayerID, LogOn, CurrentWorldID, CurrentServerGroupID) VALUES ('it-p3', 'GAME', 250, "
            "252)");
    execSQL("INSERT INTO Player (PlayerID, LogOn, CurrentWorldID, CurrentServerGroupID) VALUES ('it-p4', 'GAME', 250, "
            "251)");
    execSQL("INSERT INTO PCRoomUserInfo (ID, PlayerID) VALUES (31000, 'it-p4')");
    execSQL("INSERT INTO PCRoomUserInfo (ID, PlayerID) VALUES (31000, 'it-p3')");
    execSQL("INSERT INTO UserIPInfo (Name, IP, ServerID) VALUES ('it-ip1', 1, 31000)");
    execSQL("INSERT INTO UserIPInfo (Name, IP, ServerID) VALUES ('it-ip2', 2, 31001)");
    execSQL("INSERT INTO UserIPInfo (Name, IP, ServerID) VALUES ('it-ip3', 3, 31001)");

    SessionRepository& repository = defaultSessionRepository();

    // Session end: only a row still in GAME flips, and it gets a logout time.
    repository.markPlayerLoggedOff("it-p1");
    repository.markPlayerLoggedOff("it-p2");
    EXPECT_EQ("LOGOFF", queryScalar("SELECT LogOn FROM Player WHERE PlayerID='it-p1'"));
    EXPECT_NE("0000-00-00 00:00:00", queryScalar("SELECT LastLogoutDate FROM Player WHERE PlayerID='it-p1'"));
    EXPECT_EQ("LOGON", queryScalar("SELECT LogOn FROM Player WHERE PlayerID='it-p2'"));
    EXPECT_EQ("0000-00-00 00:00:00", queryScalar("SELECT LastLogoutDate FROM Player WHERE PlayerID='it-p2'"));

    // The boot sweep sees only this world/server group's GAME rows.
    std::vector<std::string> inGame = repository.loadPlayersInGame(250, 251);
    ASSERT_EQ(1u, inGame.size());
    EXPECT_EQ("it-p4", inGame[0]);

    repository.deletePCRoomUser("it-p4");
    EXPECT_EQ("0", queryScalar("SELECT COUNT(*) FROM PCRoomUserInfo WHERE PlayerID='it-p4'"));
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM PCRoomUserInfo WHERE PlayerID='it-p3'"));

    repository.logOffPlayersOfServer(250, 251);
    EXPECT_EQ("LOGOFF", queryScalar("SELECT LogOn FROM Player WHERE PlayerID='it-p4'"));
    EXPECT_EQ("GAME", queryScalar("SELECT LogOn FROM Player WHERE PlayerID='it-p3'"));  // other server group
    EXPECT_EQ("LOGON", queryScalar("SELECT LogOn FROM Player WHERE PlayerID='it-p2'")); // not in GAME

    repository.deleteUserIP("it-ip1");
    EXPECT_EQ("0", queryScalar("SELECT COUNT(*) FROM UserIPInfo WHERE Name='it-ip1'"));
    repository.deleteUserIPsOfServer(31001);
    EXPECT_EQ("0", queryScalar("SELECT COUNT(*) FROM UserIPInfo WHERE Name LIKE 'it-%'"));
}

TEST_F(SessionMySQL, SpecialEventCountIsReadSavedAndFalseForAnUnknownAccount) {
    execSQL("INSERT INTO Player (PlayerID, SpecialEventCount) VALUES ('it-p1', 7)");

    SessionRepository& repository = defaultSessionRepository();
    DWORD count = 0;
    ASSERT_TRUE(repository.loadSpecialEventCount("it-p1", count));
    EXPECT_EQ(7u, count);

    repository.saveSpecialEventCount(9, "it-p1");
    EXPECT_EQ("9", queryScalar("SELECT SpecialEventCount FROM Player WHERE PlayerID='it-p1'"));

    EXPECT_FALSE(repository.loadSpecialEventCount("it-none", count));
}

TEST_F(SessionMySQL, PCRoomLottoRowIsInsertedWithItsPositionalColumnsThenCounted) {
    SessionRepository& repository = defaultSessionRepository();
    int amount = -1;

    EXPECT_FALSE(repository.loadPCRoomLottoAmount("it-p1", "it-name", 250, 251, amount));
    EXPECT_EQ(-1, amount); // untouched when there is no row

    repository.insertPCRoomLotto(31000, "it-p1", 250, 251, "it-name", 1);
    ASSERT_TRUE(repository.loadPCRoomLottoAmount("it-p1", "it-name", 250, 251, amount));
    EXPECT_EQ(1, amount);
    EXPECT_EQ("31000", queryScalar("SELECT PCRoomID FROM PCRoomLottoObject WHERE PlayerID='it-p1'"));
    EXPECT_EQ("1", queryScalar("SELECT Race FROM PCRoomLottoObject WHERE PlayerID='it-p1'"));
    EXPECT_EQ("250", queryScalar("SELECT DimensionID FROM PCRoomLottoObject WHERE PlayerID='it-p1'"));
    EXPECT_EQ("251", queryScalar("SELECT WorldID FROM PCRoomLottoObject WHERE PlayerID='it-p1'"));

    repository.updatePCRoomLottoAmount(2, "it-p1", "it-name", 250, 251);
    ASSERT_TRUE(repository.loadPCRoomLottoAmount("it-p1", "it-name", 250, 251, amount));
    EXPECT_EQ(2, amount);

    EXPECT_FALSE(repository.loadPCRoomLottoAmount("it-p1", "it-name", 252, 251, amount)); // other dimension
}

TEST_F(SessionMySQL, UserStatusIsUpdatedOrInsertedOnTheUserInfoDatabase) {
    SessionRepository& repository = defaultSessionRepository();

    EXPECT_FALSE(repository.updateUserStatus(5, 120, 31000)); // no row yet
    repository.insertUserStatus(120, 31000, 5);
    EXPECT_EQ("5", queryScalar("SELECT CurrentUser FROM USERINFO.UserStatus WHERE WorldID=120 AND ServerID=31000"));

    EXPECT_TRUE(repository.updateUserStatus(6, 120, 31000));
    EXPECT_EQ("6", queryScalar("SELECT CurrentUser FROM USERINFO.UserStatus WHERE WorldID=120 AND ServerID=31000"));
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM USERINFO.UserStatus WHERE ServerID=31000"));

    // CurrentUser is a signed tinyint: a count above 127 is clamped by the
    // tier's non-strict sql_mode, not stored. Pinned as observed.
    EXPECT_TRUE(repository.updateUserStatus(300, 120, 31000));
    EXPECT_EQ("127", queryScalar("SELECT CurrentUser FROM USERINFO.UserStatus WHERE WorldID=120 AND ServerID=31000"));
}

// CGPortCheckHandler / CGRequestIPHandler: the first record of a character
// is the INSERT IGNORE; a second record for the same name changes no row
// there and so takes the UPDATE; the read hands back both columns through
// getDWORD and is false for a name without a row.
TEST_F(SessionMySQL, UserIPIsInsertedThenUpdatedAndReadBack) {
    SessionRepository& repository = defaultSessionRepository();
    DWORD ip = 0, port = 0;

    EXPECT_FALSE(repository.loadUserIP("it-nobody", ip, port));

    repository.recordUserIP("it-char", 0x0100007fu, 9858, 31000);
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM UserIPInfo WHERE Name = 'it-char'"));
    EXPECT_EQ("31000", queryScalar("SELECT ServerID FROM UserIPInfo WHERE Name = 'it-char'"));
    ASSERT_TRUE(repository.loadUserIP("it-char", ip, port));
    EXPECT_EQ(0x0100007fu, ip);
    EXPECT_EQ(9858u, port);

    // The UPDATE path: IP and Port move, ServerID is not in the UPDATE.
    repository.recordUserIP("it-char", 0x0200007fu, 9859, 31001);
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM UserIPInfo WHERE Name = 'it-char'"));
    EXPECT_EQ("31000", queryScalar("SELECT ServerID FROM UserIPInfo WHERE Name = 'it-char'"));
    ASSERT_TRUE(repository.loadUserIP("it-char", ip, port));
    EXPECT_EQ(0x0200007fu, ip);
    EXPECT_EQ(9859u, port);
}

// CGVerifyTimeHandler: the first sighting of an account takes the INSERT
// IGNORE (the UPDATE changed no row) with Count 1; the next takes the
// UPDATE, moving IP/NAME/WorldID/ServerGroupID and counting up; another
// account's row is untouched.
TEST_F(SessionMySQL, SpeedHackIsInsertedOnFirstSightThenCountedUp) {
    SessionRepository& repository = defaultSessionRepository();
    execSQL("INSERT INTO SpeedHackPlayer (PlayerID, IP, Name, WorldID, ServerGroupID, Date, Count) "
            "VALUES ('it-other', '1.1.1.1', 'Slayer:o', 1, 1, now(), 5)");

    repository.recordSpeedHack("it-acct", "10.0.0.1", "Slayer:it-char", 2, 3);
    EXPECT_EQ("1", queryScalar("SELECT Count FROM SpeedHackPlayer WHERE PlayerID = 'it-acct'"));
    EXPECT_EQ("10.0.0.1", queryScalar("SELECT IP FROM SpeedHackPlayer WHERE PlayerID = 'it-acct'"));
    EXPECT_EQ("2", queryScalar("SELECT WorldID FROM SpeedHackPlayer WHERE PlayerID = 'it-acct'"));

    repository.recordSpeedHack("it-acct", "10.0.0.2", "Vampire:it-char2", 4, 5);
    EXPECT_EQ("2", queryScalar("SELECT Count FROM SpeedHackPlayer WHERE PlayerID = 'it-acct'"));
    EXPECT_EQ("10.0.0.2", queryScalar("SELECT IP FROM SpeedHackPlayer WHERE PlayerID = 'it-acct'"));
    EXPECT_EQ("Vampire:it-char2", queryScalar("SELECT Name FROM SpeedHackPlayer WHERE PlayerID = 'it-acct'"));
    EXPECT_EQ("4", queryScalar("SELECT WorldID FROM SpeedHackPlayer WHERE PlayerID = 'it-acct'"));
    EXPECT_EQ("5", queryScalar("SELECT ServerGroupID FROM SpeedHackPlayer WHERE PlayerID = 'it-acct'"));
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM SpeedHackPlayer WHERE PlayerID = 'it-acct'"));
    EXPECT_EQ("5", queryScalar("SELECT Count FROM SpeedHackPlayer WHERE PlayerID = 'it-other'"));
}

// CGCrashReportHandler: one row per report, ReportTime server-side, the
// WORD version and the texts landing in their columns.
TEST_F(SessionMySQL, CrashReportRowIsInsertedWithItsColumns) {
    defaultSessionRepository().insertCrashReport("it-acct", "it-char", "2026-09-06 10:00:00", 1234, "0x0040", "boom",
                                                 "WinXP", "a\\nb");
    EXPECT_EQ("1", queryScalar("SELECT COUNT(*) FROM CrashReportLog WHERE PlayerID = 'it-acct'"));
    EXPECT_EQ("it-char", queryScalar("SELECT Name FROM CrashReportLog WHERE PlayerID = 'it-acct'"));
    EXPECT_EQ("1234", queryScalar("SELECT Version FROM CrashReportLog WHERE PlayerID = 'it-acct'"));
    EXPECT_EQ("0x0040", queryScalar("SELECT Address FROM CrashReportLog WHERE PlayerID = 'it-acct'"));
    EXPECT_EQ("boom", queryScalar("SELECT Message FROM CrashReportLog WHERE PlayerID = 'it-acct'"));
    EXPECT_EQ("WinXP", queryScalar("SELECT OS FROM CrashReportLog WHERE PlayerID = 'it-acct'"));
    EXPECT_EQ("2026-09-06 10:00:00",
              queryScalar("SELECT ExecutableTime FROM CrashReportLog WHERE PlayerID = 'it-acct'"));
    EXPECT_EQ("1", queryScalar("SELECT ReportTime > '2026-01-01' FROM CrashReportLog WHERE PlayerID = 'it-acct'"));
}

// CommonBillingPacket::setExpire_Date: the LastLogoutDate text of an
// account through the dist connection; false for an account without a row.
TEST_F(SessionMySQL, LastLogoutDateIsReadAsTextForAnAccount) {
    std::string date = "untouched";
    EXPECT_FALSE(defaultSessionRepository().loadLastLogoutDate("it-nobody", date));
    EXPECT_EQ("untouched", date);

    execSQL("INSERT INTO Player (PlayerID, LastLogoutDate) VALUES ('it-acct', '2026-09-05 23:59:58')");
    ASSERT_TRUE(defaultSessionRepository().loadLastLogoutDate("it-acct", date));
    EXPECT_EQ("2026-09-05 23:59:58", date);
}

// CGSayHandler's GM bookkeeping: the UserIPInfo ServerID read (false for
// an unknown name), the online count over LogOn = 'GAME' or 'LOGON' only,
// the ban flipping Access to DENY for that account alone, and the two
// GM-typed report rows.
TEST_F(SessionMySQL, GMCommandsReadServerIDCountOnlineDenyAndLogReports) {
    SessionRepository& repository = defaultSessionRepository();

    int serverID = -1;
    EXPECT_FALSE(repository.loadUserServerID("it-nobody", serverID));
    EXPECT_EQ(-1, serverID);
    execSQL("INSERT INTO UserIPInfo (Name, IP, Port, ServerID) VALUES ('it-char', 1, 2, 31000)");
    ASSERT_TRUE(repository.loadUserServerID("it-char", serverID));
    EXPECT_EQ(31000, serverID);

    const int before = repository.countPlayersOnline();
    execSQL("INSERT INTO Player (PlayerID, LogOn) VALUES ('it-a1', 'GAME')");
    execSQL("INSERT INTO Player (PlayerID, LogOn) VALUES ('it-a2', 'LOGON')");
    execSQL("INSERT INTO Player (PlayerID, LogOn) VALUES ('it-a3', 'LOGOFF')");
    EXPECT_EQ(before + 2, repository.countPlayersOnline());

    repository.denyAccount("it-a1");
    EXPECT_EQ("DENY", queryScalar("SELECT Access FROM Player WHERE PlayerID = 'it-a1'"));
    EXPECT_EQ("ALLOW", queryScalar("SELECT Access FROM Player WHERE PlayerID = 'it-a2'"));

    repository.insertBugReport("it-a1", "it-char", "it broke");
    EXPECT_EQ("it broke", queryScalar("SELECT ReportLog FROM BugReportLog WHERE PlayerID = 'it-a1'"));
    EXPECT_EQ("it-char", queryScalar("SELECT Name FROM BugReportLog WHERE PlayerID = 'it-a1'"));
    EXPECT_EQ("1", queryScalar("SELECT ReportTime > '2026-01-01' FROM BugReportLog WHERE PlayerID = 'it-a1'"));

    repository.insertCrashLog("it-a1", "it-char", "2026-09-06 10:00:00", "12", "0x0040", "boom");
    const std::string crash = " FROM CrashLog WHERE PlayerID = 'it-a1'";
    EXPECT_EQ("2026-09-06 10:00:00", queryScalar("SELECT ExecutableTime" + crash));
    EXPECT_EQ("12", queryScalar("SELECT Version" + crash));
    EXPECT_EQ("0x0040", queryScalar("SELECT Address" + crash));
    EXPECT_EQ("boom", queryScalar("SELECT Message" + crash));
    EXPECT_EQ("1", queryScalar("SELECT ReportTime > '2026-01-01'" + crash));
}

// --- the ExpTable template's generic balance read ---------------------------
// SomethingGrowingUp.h's ExpTable::load names its table and columns; the
// repository formats "SELECT %s, %s, %s FROM %s %s" from them. The seeded RankEXPInfo,
// AdvancementClassEXPInfo and the attribute balance tables stand in.

TEST(ExpTableMySQL, ExpTablesLoadByNamedColumnsWithAndWithoutACondition) {
    BalanceInfoRepository& repository = defaultBalanceInfoRepository();

    std::vector<ExpTableRow> ranks =
        repository.loadExpTable("Level", "GoalExp", "AccumExp", "RankEXPInfo", "where RankType=0");
    ASSERT_FALSE(ranks.empty());
    EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM RankEXPInfo where RankType=0").c_str()), (int)ranks.size());
    for (size_t r = 0; r < ranks.size(); r++) {
        EXPECT_TRUE(ranks[r].level >= 1 && ranks[r].level <= 50);
        EXPECT_EQ(atoi(queryScalar("SELECT GoalExp FROM RankEXPInfo where RankType=0 AND Level=" +
                                   std::to_string(ranks[r].level))
                           .c_str()),
                  ranks[r].goalExp);
    }

    // No condition: the whole table. The original's trailing space lives only
    // in the statement's bytes; the rows cannot show it.
    std::vector<ExpTableRow> all = repository.loadExpTable("Level", "GoalExp", "AccumExp", "RankEXPInfo", "");
    EXPECT_EQ(atoi(queryScalar("SELECT COUNT(*) FROM RankEXPInfo").c_str()), (int)all.size());

    EXPECT_FALSE(repository.loadExpTable("Level", "GoalExp", "AccumExp", "AdvancementClassEXPInfo", "").empty());
    // STRBalanceInfo.AccumExp exceeds INT_MAX in the top rows; the row's int
    // (getInt = atoi) truncates.
    EXPECT_FALSE(repository.loadExpTable("Level", "GoalExp", "AccumExp", "STRBalanceInfo", "").empty());
}

// --- the guild cluster against real MySQL --------------------------------------
// Guilds, members, unions and union offers, plus the guild-scoped war reads.
// GuildInfo and GuildMember are unseeded; the union and war tables are
// seeded, so the tests use ids from 31000 up and clean them in SetUp/TearDown.

class GuildMySQL : public ::testing::Test {
protected:
    virtual void SetUp() {
        clean();
    }
    virtual void TearDown() {
        clean();
    }
    static void clean() {
        execSQL("DELETE FROM GuildMember WHERE Name LIKE 'it-%'");
        execSQL("DELETE FROM GuildInfo WHERE GuildID >= 31000");
        execSQL("DELETE FROM GuildUnionMember WHERE OwnerGuildID >= 31000");
        execSQL("DELETE FROM GuildUnionInfo WHERE MasterGuildID >= 31000");
        execSQL("DELETE FROM GuildUnionOffer WHERE OwnerGuildID >= 31000");
        execSQL("DELETE FROM WarScheduleInfo WHERE WarID >= 31000");
        execSQL("DELETE FROM ReinforceRegisterInfo WHERE WarID >= 31000");
        execSQL("DELETE FROM CastleInfo WHERE ServerID >= 31000");
    }
};

TEST_F(GuildMySQL, MemberRowsAreCreatedRejoinedSavedExpiredAndDeleted) {
    GuildRepository& repository = defaultGuildRepository();

    EXPECT_FALSE(repository.memberExists("it-m1"));
    repository.insertMember(31000, "it-m1", 2);
    EXPECT_TRUE(repository.memberExists("it-m1"));

    GuildMemberRow row;
    ASSERT_TRUE(repository.loadMember("it-m1", row));
    EXPECT_EQ(31000, row.guildID);
    EXPECT_EQ("it-m1", row.name);
    EXPECT_EQ(2, row.rank);
    EXPECT_EQ(0, row.logOn);
    EXPECT_FALSE(repository.loadMember("it-none", row));

    // Expiring sets the rank and the expiry date.
    repository.setMemberRankAndExpireDate(5, "1260901", "it-m1");
    EXPECT_EQ("5", queryScalar("SELECT `Rank` FROM GuildMember WHERE Name='it-m1'"));
    EXPECT_EQ("1260901", queryScalar("SELECT ExpireDate FROM GuildMember WHERE Name='it-m1'"));
    // Re-joining rewrites the row and clears the expiry.
    repository.rejoinWaitingMember(31001, 1, "2026-01-02 03:04:05", "it-m1");
    EXPECT_EQ("31001", queryScalar("SELECT GuildID FROM GuildMember WHERE Name='it-m1'"));
    EXPECT_EQ("1", queryScalar("SELECT `Rank` FROM GuildMember WHERE Name='it-m1'"));
    EXPECT_EQ("", queryScalar("SELECT ExpireDate FROM GuildMember WHERE Name='it-m1'"));
    EXPECT_EQ("2026-01-02 03:04:05", queryScalar("SELECT RequestDateTime FROM GuildMember WHERE Name='it-m1'"));
    repository.rejoinMember(31000, 3, "it-m1");
    EXPECT_EQ("31000", queryScalar("SELECT GuildID FROM GuildMember WHERE Name='it-m1'"));
    EXPECT_EQ("3", queryScalar("SELECT `Rank` FROM GuildMember WHERE Name='it-m1'"));

    repository.saveMember(31002, 2, "it-m1");
    EXPECT_EQ("31002", queryScalar("SELECT GuildID FROM GuildMember WHERE Name='it-m1'"));

    repository.saveMemberIntro("hello", "it-m1");
    std::string intro;
    ASSERT_TRUE(repository.loadMemberIntro("it-m1", intro));
    EXPECT_EQ("hello", intro);
    EXPECT_FALSE(repository.loadMemberIntro("it-none", intro));

    // The boot-time member list takes ranks 0..3 only.
    repository.insertWaitingMember(31002, "it-m2", 0, "2026-02-03 04:05:06");
    repository.insertMember(31002, "it-m3", 7);
    std::vector<GuildMemberListRow> active = repository.loadActiveMembers();
    bool seenM1 = false, seenM2 = false, seenM3 = false;
    for (size_t r = 0; r < active.size(); r++) {
        if (active[r].name == "it-m1")
            seenM1 = true;
        if (active[r].name == "it-m2") {
            seenM2 = true;
            EXPECT_EQ(31002, active[r].guildID);
            EXPECT_EQ(0, active[r].rank);
            EXPECT_EQ("2026-02-03 04:05:06", active[r].requestDateTime);
        }
        if (active[r].name == "it-m3")
            seenM3 = true;
    }
    EXPECT_TRUE(seenM1);
    EXPECT_TRUE(seenM2);
    EXPECT_FALSE(seenM3);

    repository.deleteMember("it-m2");
    EXPECT_FALSE(repository.loadMember("it-m2", row));
}

TEST_F(GuildMySQL, TheThreeMembershipProbesEachReadTheirOwnColumns) {
    GuildRepository& repository = defaultGuildRepository();

    // One row, read through three different column lists. The handlers
    // read their columns POSITIONALLY, so what matters is that each
    // projection hands back the field its own handler reads.
    repository.insertMember(31007, "it-probe", 4);
    repository.setMemberRankAndExpireDate(5, "1260901", "it-probe");

    // CGRegistGuildHandler: SELECT `Rank`, ExpireDate.
    int rank = -1;
    std::string expireDate;
    ASSERT_TRUE(repository.loadMemberRankExpireDate("it-probe", rank, expireDate));
    EXPECT_EQ(5, rank);
    EXPECT_EQ("1260901", expireDate);

    // CGJoinGuildHandler: SELECT GuildID, `Rank`, ExpireDate.
    int guildID = -1;
    rank = -1;
    expireDate.clear();
    ASSERT_TRUE(repository.loadMemberGuildRankExpireDate("it-probe", guildID, rank, expireDate));
    EXPECT_EQ(31007, guildID);
    EXPECT_EQ(5, rank);
    EXPECT_EQ("1260901", expireDate);

    // CGTryJoinGuildHandler: SELECT GuildID, ExpireDate,`Rank`, of which
    // only ExpireDate was ever read. The three seeded values are
    // pairwise distinct, so a mismatch between a statement's column
    // order and its read indices shows up in ALL three probes, not just
    // this one. What no assertion here can catch is a CONSISTENT
    // rewrite of both, which stays invisible to the database.
    expireDate.clear();
    ASSERT_TRUE(repository.loadMemberExpireDate("it-probe", expireDate));
    EXPECT_EQ("1260901", expireDate);

    // No row at all: false everywhere, which every caller reads as
    // "belongs to no guild" and lets through.
    EXPECT_FALSE(repository.loadMemberRankExpireDate("it-none", rank, expireDate));
    EXPECT_FALSE(repository.loadMemberGuildRankExpireDate("it-none", guildID, rank, expireDate));
    EXPECT_FALSE(repository.loadMemberExpireDate("it-none", expireDate));

    // A fresh row's ExpireDate is "" and not NULL (varchar(7) NOT NULL
    // DEFAULT ''), so the size() == 7 test the callers make is false and
    // no substr runs against a short string.
    repository.insertMember(31008, "it-fresh", 2);
    ASSERT_TRUE(repository.loadMemberExpireDate("it-fresh", expireDate));
    EXPECT_EQ("", expireDate);
}

TEST_F(GuildMySQL, BothSpellingsOfTheMemberDeleteRemoveTheRow) {
    GuildRepository& repository = defaultGuildRepository();
    GuildMemberRow row;

    repository.insertMember(31009, "it-del1", 2);
    repository.deleteMemberSpelled(GUILD_MEMBER_DELETE_SPACED, "it-del1");
    EXPECT_FALSE(repository.loadMember("it-del1", row));

    repository.insertMember(31009, "it-del2", 2);
    repository.deleteMemberSpelled(GUILD_MEMBER_DELETE_UNSPACED, "it-del2");
    EXPECT_FALSE(repository.loadMember("it-del2", row));

    // deleteMember() IS the spaced spelling, delegating. What this test
    // cannot do: whitespace around "=" is not part of the parsed
    // statement, so no assertion here can tell the two literals apart, and
    // a swapped enumerator would pass. What is pinned is that neither
    // spelling is malformed.
    repository.insertMember(31009, "it-del3", 2);
    repository.deleteMember("it-del3");
    EXPECT_FALSE(repository.loadMember("it-del3", row));
}

TEST_F(GuildMySQL, TheGuildNameProbeSeesOnlyActiveAndWaitingGuilds) {
    GuildRepository& repository = defaultGuildRepository();

    GuildRecord record;
    record.id = 31010;
    record.name = "it-taken";
    record.type = 0;
    record.race = 0;
    record.state = 0; // GUILD_STATE_ACTIVE, the first of the probe's two
    record.serverGroupID = 1;
    record.zoneID = 31010;
    record.master = "it-master";
    record.date = "2026-09-04";
    record.intro = "";
    repository.insertGuild(record);
    EXPECT_TRUE(repository.guildNameInUse("it-taken"));

    // State 1 is GUILD_STATE_WAIT: a name is held from the moment the
    // registration is sent, before the sharedserver approves it.
    record.id = 31011;
    record.name = "it-pending";
    record.state = 1;
    repository.insertGuild(record);
    EXPECT_TRUE(repository.guildNameInUse("it-pending"));

    // States 2 and 3 are CANCEL and BROKEN, and release the name.
    record.id = 31012;
    record.name = "it-cancelled";
    record.state = 2;
    repository.insertGuild(record);
    EXPECT_FALSE(repository.guildNameInUse("it-cancelled"));

    record.id = 31013;
    record.name = "it-broken";
    record.state = 3;
    repository.insertGuild(record);
    EXPECT_FALSE(repository.guildNameInUse("it-broken"));

    EXPECT_FALSE(repository.guildNameInUse("it-unknown"));
}

TEST_F(GuildMySQL, TheGuildIdReadAnswersOnTheFirstRowWhereTheExistenceProbeCounts) {
    GuildRepository& repository = defaultGuildRepository();

    int guildID = -1;
    EXPECT_FALSE(repository.loadMemberGuildID("it-gid", guildID));
    EXPECT_EQ(-1, guildID); // left alone when there is no row

    repository.insertMember(31020, "it-gid", 2);
    EXPECT_TRUE(repository.memberExists("it-gid"));
    ASSERT_TRUE(repository.loadMemberGuildID("it-gid", guildID));
    EXPECT_EQ(31020, guildID);

    // Same statement, two questions: memberExists counts rows and this
    // one reads the id. Name is the primary key, so there is at most one
    // row and the two can never disagree.
    repository.saveMember(31021, 2, "it-gid");
    ASSERT_TRUE(repository.loadMemberGuildID("it-gid", guildID));
    EXPECT_EQ(31021, guildID);
}

TEST_F(GuildMySQL, GuildRowsAreCreatedLoadedSavedListedByStateAndDeletedWithTheirUnionRows) {
    GuildRepository& repository = defaultGuildRepository();

    GuildRecord record;
    record.id = 31000;
    record.name = "it-guild";
    record.type = 1;
    record.race = 2;
    record.state = 1;
    record.serverGroupID = 3;
    record.zoneID = 31000;
    record.master = "it-master";
    record.date = "2026-09-02";
    record.intro = "intro";
    repository.insertGuild(record);

    GuildRow row;
    ASSERT_TRUE(repository.loadGuild(31000, row));
    EXPECT_EQ("it-guild", row.name);
    EXPECT_EQ(1, row.type);
    EXPECT_EQ(2, row.race);
    EXPECT_EQ(1, row.state);
    EXPECT_EQ(3, row.serverGroupID);
    EXPECT_EQ(31000, row.zoneID);
    EXPECT_EQ("it-master", row.master);
    EXPECT_EQ("2026-09-02", row.date);
    EXPECT_EQ("intro", queryScalar("SELECT Intro FROM GuildInfo WHERE GuildID=31000"));
    EXPECT_FALSE(repository.loadGuild(31001, row));

    record.name = "it-renamed";
    record.state = 2;
    record.master = "it-master2";
    repository.saveGuild(record);
    ASSERT_TRUE(repository.loadGuild(31000, row));
    EXPECT_EQ("it-renamed", row.name);
    EXPECT_EQ(2, row.state);
    EXPECT_EQ("it-master2", row.master);
    EXPECT_EQ("intro", queryScalar("SELECT Intro FROM GuildInfo WHERE GuildID=31000")); // save leaves the intro alone

    record.id = 31001;
    record.state = 9;
    repository.insertGuild(record);
    std::vector<GuildListRow> listed = repository.loadGuildsInStates(1, 2);
    bool seen31000 = false, seen31001 = false;
    for (size_t r = 0; r < listed.size(); r++) {
        if (listed[r].id == 31000) {
            seen31000 = true;
            EXPECT_EQ("it-renamed", listed[r].name);
            EXPECT_EQ(2, listed[r].state);
            EXPECT_EQ("intro", listed[r].intro);
        }
        if (listed[r].id == 31001)
            seen31001 = true;
    }
    EXPECT_TRUE(seen31000);
    EXPECT_FALSE(seen31001);

    std::string name, master;
    ASSERT_TRUE(repository.loadGuildNameAndMaster(31000, name, master));
    EXPECT_EQ("it-renamed", name);
    EXPECT_EQ("it-master2", master);
    EXPECT_FALSE(repository.loadGuildNameAndMaster(31002, name, master));

    // Destroying a guild also drops its union memberships.
    execSQL("INSERT INTO GuildUnionMember (UnionID, OwnerGuildID) VALUES (31000, 31000)");
    repository.deleteGuild(31000);
    EXPECT_FALSE(repository.loadGuild(31000, row));
    EXPECT_EQ("0", queryScalar("SELECT COUNT(*) FROM GuildUnionMember WHERE OwnerGuildID=31000"));
    EXPECT_TRUE(repository.loadGuild(31001, row));
}

TEST_F(GuildMySQL, CastleAndWarReadsAreScopedToTheGuild) {
    execSQL("INSERT INTO CastleInfo (ServerID, ZoneID, GuildID, Name, BonusOptionType, ZoneIDList) "
            "VALUES (31000, 31000, 31000, 'it-castle', '', '')");
    execSQL("INSERT INTO WarScheduleInfo (WarID, ServerID, ZoneID, AttackGuildID3, Status) "
            "VALUES (31000, 31000, 31000, 31000, 'WAIT')");
    execSQL("INSERT INTO WarScheduleInfo (WarID, ServerID, ZoneID, AttackGuildID, Status) "
            "VALUES (31001, 31000, 31000, 31000, 'END')");
    execSQL("INSERT INTO WarScheduleInfo (WarID, ServerID, ZoneID, AttackGuildID, Status) "
            "VALUES (31002, 31000, 31000, 31002, 'START')");
    execSQL("INSERT INTO ReinforceRegisterInfo (WarID, ServerID, ReinforceGuildID, Status) "
            "VALUES (31000, 31000, 31005, 'WAIT')");
    execSQL("INSERT INTO ReinforceRegisterInfo (WarID, ServerID, ReinforceGuildID, Status) "
            "VALUES (31000, 31000, 31006, 'DENY')");

    GuildRepository& repository = defaultGuildRepository();

    EXPECT_EQ(1, repository.countCastlesOfGuild(31000));
    EXPECT_EQ(0, repository.countCastlesOfGuild(31001));
    int serverID = 0, zoneID = 0;
    ASSERT_TRUE(repository.loadCastleOfGuild(31000, serverID, zoneID));
    EXPECT_EQ(31000, serverID);
    EXPECT_EQ(31000, zoneID);
    EXPECT_FALSE(repository.loadCastleOfGuild(31001, serverID, zoneID));
    EXPECT_EQ(31000, serverID); // a miss leaves the out-params alone (hasActiveWar relies on it)
    EXPECT_EQ(31000, zoneID);

    // Two of the five attacker slots are seeded (AttackGuildID, AttackGuildID3);
    // either counts, but only WAIT/START wars.
    EXPECT_EQ(1, repository.countWarSchedulesOfAttacker(31000));
    EXPECT_EQ(1, repository.countWarSchedulesOfAttacker(31002));
    EXPECT_EQ(0, repository.countWarSchedulesOfAttacker(31003));

    EXPECT_EQ(1, repository.countReinforceRegistrations(31005));
    EXPECT_EQ(0, repository.countReinforceRegistrations(31006)); // DENY

    EXPECT_EQ(1, repository.countStartedWarsAtCastle(31000, 31000));
    EXPECT_EQ(0, repository.countStartedWarsAtCastle(31000, 31001));
    EXPECT_EQ(1, repository.countStartedWarsOfAttacker(31002));
    EXPECT_EQ(0, repository.countStartedWarsOfAttacker(31000)); // its war is WAIT
}


TEST_F(GuildMySQL, TheOfferPurgeReadsEveryRowWithItsAgeAndDeletesByUnionAndType) {
    GuildRepository& repository = defaultGuildRepository();

    // A fresh JOIN and QUIT to one union, an expired JOIN to it, and a fresh
    // ESCAPE naming it: the purge's view of the table.
    execSQL("INSERT INTO GuildUnionOffer (UnionID, OfferType, OwnerGuildID, OfferTime) "
            "VALUES (31000, 'JOIN', 31000, now())");
    execSQL("INSERT INTO GuildUnionOffer (UnionID, OfferType, OwnerGuildID, OfferTime) "
            "VALUES (31000, 'QUIT', 31001, now())");
    execSQL("INSERT INTO GuildUnionOffer (UnionID, OfferType, OwnerGuildID, OfferTime) "
            "VALUES (31000, 'JOIN', 31002, now() - interval 11 day)");
    execSQL("INSERT INTO GuildUnionOffer (UnionID, OfferType, OwnerGuildID, OfferTime) "
            "VALUES (31000, 'ESCAPE', 31003, now())");

    const std::vector<UnionOfferStateRow> states = repository.loadOfferStates();
    int seen = 0;
    for (size_t r = 0; r < states.size(); r++) {
        const UnionOfferStateRow& row = states[r];
        if (row.ownerGuildID < 31000)
            continue; // the seed's own rows
        seen++;
        EXPECT_EQ(31000, row.unionID);
        if (row.ownerGuildID == 31000) {
            EXPECT_EQ(UNION_OFFER_JOIN, row.offerType);
            EXPECT_FALSE(row.expired);
        } else if (row.ownerGuildID == 31001) {
            EXPECT_EQ(UNION_OFFER_QUIT, row.offerType);
            EXPECT_FALSE(row.expired);
        } else if (row.ownerGuildID == 31002) {
            EXPECT_EQ(UNION_OFFER_JOIN, row.offerType);
            EXPECT_TRUE(row.expired) << "eleven days is past the ten-day lifetime";
        } else {
            EXPECT_EQ(UNION_OFFER_ESCAPE, row.offerType);
            EXPECT_FALSE(row.expired);
        }
    }
    EXPECT_EQ(4, seen);

    // Only the fresh JOIN keeps the union: the expired one does not count.
    EXPECT_EQ(1, repository.countPendingJoinOffers(31000));

    // An offer to a union that has gone is deleted by guild, union and type:
    // another union's id, or an ESCAPE row, is left alone.
    repository.deleteOfferToUnion(31000, 31999);
    EXPECT_EQ(1, repository.countOffers(31000));
    repository.deleteOfferToUnion(31003, 31000);
    EXPECT_EQ(1, repository.countOffers(31003)) << "an ESCAPE row is the guild's penalty";
    repository.deleteOfferToUnion(31000, 31000);
    EXPECT_EQ(0, repository.countOffers(31000));
    EXPECT_EQ(0, repository.countPendingJoinOffers(31000));

    // A guild leaving its union takes its QUIT row, and only that.
    repository.deleteQuitOffer(31003);
    EXPECT_EQ(1, repository.countOffers(31003));
    repository.deleteQuitOffer(31001);
    EXPECT_EQ(0, repository.countOffers(31001));
}

TEST_F(GuildMySQL, ADissolvedUnionTakesTheJoinAndQuitOffersNamingIt) {
    GuildRepository& repository = defaultGuildRepository();

    repository.insertJoinOffer(31000, 31000);
    repository.insertQuitOffer(31000, 31001);
    repository.insertEscapeOffer(31000, 31002);
    repository.insertJoinOffer(31500, 31003);

    repository.deleteOffersToUnion(31000);

    EXPECT_EQ(0, repository.countOffers(31000));
    EXPECT_EQ(0, repository.countOffers(31001));
    EXPECT_EQ(1, repository.countRecentEscapes(31002)) << "the penalty outlives the union";
    EXPECT_EQ(1, repository.countOffers(31003)) << "another union's offer";
}

TEST_F(GuildMySQL, TheEscapeOfferIsAPositionalRowThatTheTenDayCountFinds) {
    GuildRepository& repository = defaultGuildRepository();

    const uint unionID = repository.insertUnion(31001);
    EXPECT_EQ(0, repository.countRecentEscapes(31002));

    // Written positionally — no column list — so the row depends on
    // GuildUnionOffer's column order (UnionID, OfferType, OwnerGuildID,
    // OfferTime), unlike insertJoinOffer / insertQuitOffer beside it.
    repository.insertEscapeOffer(unionID, 31002);

    const std::string where = "FROM GuildUnionOffer WHERE OwnerGuildID=31002";
    EXPECT_EQ(std::to_string(unionID), queryScalar("SELECT UnionID " + where));
    EXPECT_EQ("ESCAPE", queryScalar("SELECT OfferType " + where));

    // OfferTime is now(), so the ten-day window the join penalty uses sees
    // it; the column's own default is a 2004 date, which would not.
    EXPECT_EQ(1, repository.countRecentEscapes(31002));
    EXPECT_EQ(1, repository.countOffers(31002));

    // It is an ordinary offer row, so the repository's sweeper clears it.
    repository.deleteOffers(31002);
    EXPECT_EQ(0, repository.countOffers(31002));
}

TEST_F(GuildMySQL, UnionsAreCreatedWithAnAutoIncrementIdAndTheirMembersReadAndRemoved) {
    GuildRepository& repository = defaultGuildRepository();

    uint unionID = repository.insertUnion(31000);
    ASSERT_GT(unionID, 0u);
    repository.insertUnionMember(unionID, 31000);
    repository.insertUnionMember(unionID, 31001);

    std::vector<UnionRow> unions = repository.loadUnions();
    bool seen = false;
    for (size_t r = 0; r < unions.size(); r++) {
        if (unions[r].unionID == (int)unionID) {
            seen = true;
            EXPECT_EQ(31000, unions[r].masterGuildID);
        }
    }
    EXPECT_TRUE(seen);

    std::vector<int> members = repository.loadUnionMemberGuilds(unionID);
    ASSERT_EQ(2u, members.size());
    EXPECT_EQ(2, repository.countUnionMembers(unionID));

    int foundUnion = 0, foundOwner = 0;
    ASSERT_TRUE(repository.loadUnionOfGuild(31001, foundUnion, foundOwner));
    EXPECT_EQ((int)unionID, foundUnion);
    EXPECT_EQ(31001, foundOwner);
    EXPECT_FALSE(repository.loadUnionOfGuild(31002, foundUnion, foundOwner));

    int master = 0;
    ASSERT_TRUE(repository.loadUnionMaster((int)unionID, master));
    EXPECT_EQ(31000, master);

    EXPECT_TRUE(repository.deleteUnionMember(unionID, 31001));
    EXPECT_FALSE(repository.deleteUnionMember(unionID, 31001)); // already gone
    EXPECT_EQ(1, repository.countUnionMembers(unionID));

    repository.deleteUnion(unionID);
    EXPECT_FALSE(repository.loadUnionMaster((int)unionID, master));
    EXPECT_EQ(0, repository.countUnionMembers(unionID));
}

TEST_F(GuildMySQL, UnionOffersAreInsertedReadByTypeAgedAndCleared) {
    GuildRepository& repository = defaultGuildRepository();

    EXPECT_EQ(0, repository.countOffers(31000));
    repository.insertJoinOffer(31000, 31000);
    repository.insertQuitOffer(31000, 31001);
    EXPECT_EQ(1, repository.countOffers(31000));

    int unionID = 0;
    ASSERT_TRUE(repository.loadJoinOfferUnion(31000, unionID));
    EXPECT_EQ(31000, unionID);
    EXPECT_FALSE(repository.loadQuitOfferUnion(31000, unionID));
    ASSERT_TRUE(repository.loadQuitOfferUnion(31001, unionID));
    EXPECT_EQ(31000, unionID);

    std::vector<UnionOfferRow> offers = repository.loadOffers(31000);
    ASSERT_EQ(2u, offers.size());
    int today = atoi(queryScalar("SELECT DATE_FORMAT(now(),'%y%m%d')").c_str());
    for (size_t r = 0; r < offers.size(); r++) {
        EXPECT_EQ(today, offers[r].date);
        if (offers[r].ownerGuildID == 31000)
            EXPECT_EQ(1, offers[r].offerType); // JOIN is the enum's first value
        if (offers[r].ownerGuildID == 31001)
            EXPECT_EQ(2, offers[r].offerType); // QUIT
    }

    // The join penalty looks ten days back for an ESCAPE.
    execSQL("INSERT INTO GuildUnionOffer (UnionID, OfferType, OwnerGuildID, OfferTime) "
            "VALUES (31000, 'ESCAPE', 31002, now())");
    execSQL("INSERT INTO GuildUnionOffer (UnionID, OfferType, OwnerGuildID, OfferTime) "
            "VALUES (31000, 'ESCAPE', 31003, now() - interval 20 day)");
    EXPECT_EQ(1, repository.countRecentEscapes(31002));
    EXPECT_EQ(0, repository.countRecentEscapes(31003));

    repository.deleteStaleOffers(31003);
    EXPECT_EQ(0, repository.countOffers(31003));
    repository.deleteStaleOffers(31002);
    EXPECT_EQ(1, repository.countOffers(31002)); // fresh, kept

    repository.deleteOffers(31000);
    EXPECT_EQ(0, repository.countOffers(31000));
    EXPECT_EQ(1, repository.countOffers(31001));
}

// --- what END_DB throws ----------------------------------------------------

// Every statement in the tree answers a SQL failure with this value, and a
// SELECT against a table the schema does not have is the cheapest way to
// reach it. What is pinned is that the value OWNS its message: the error is
// copied out of the handler and read after the catch block has gone, and
// the text is the line END_DB wrote to DBError.log — the function the
// statement ran in, then the driver's own words. A pointer into the
// handler's string would give an empty or garbage read here.
TEST(DatabaseErrorMySQL, TheMessageOutlivesTheHandlerAndNamesTheFunctionAndTheSQLError) {
    DatabaseError caught{""};
    bool refused = false;

    try {
        execSQL("SELECT * FROM no_such_table_for_the_database_error_pin");
    } catch (const DatabaseError& error) {
        caught = error;
        refused = true;
    }

    ASSERT_TRUE(refused) << "the missing table did not raise";

    const std::string reported = caught.message();
    EXPECT_FALSE(reported.empty());
    EXPECT_NE(std::string::npos, reported.find("execSQL")) << "message was: " << reported;
    EXPECT_NE(std::string::npos, reported.find("SQLQueryException")) << "message was: " << reported;
    EXPECT_NE(std::string::npos, reported.find("no_such_table_for_the_database_error_pin"))
        << "message was: " << reported;
    EXPECT_STREQ(reported.c_str(), caught.what());
}

} // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::string host = env("IT_DB_HOST", "127.0.0.1");
    std::string db = env("IT_DB_DB", "DARKEDEN");
    std::string user = env("IT_DB_USER", "elcastle");
    std::string password = env("IT_DB_PASSWORD", "elca110");
    uint port = (uint)atoi(env("IT_DB_PORT", "3306").c_str());

    DatabaseManager* pDatabaseManager = new DatabaseManager();
    de::serverContext().setDatabaseManager(pDatabaseManager);
    pDatabaseManager->addConnection((int)(long)Thread::self(), new Connection(host, db, user, password, port));
    pDatabaseManager->addDistConnection((int)(long)Thread::self(), new Connection(host, db, user, password, port));
    // The USERINFO database SessionRepository writes UserStatus to; the
    // tier loads initdb/USERINFO.sql next to DARKEDEN.sql.
    std::string userInfoDb = env("IT_DB_USERINFO_DB", "USERINFO");
    pDatabaseManager->setUserInfoConnection(new Connection(host, userInfoDb, user, password, port));
    // The WorldDBInfo row-0 connection SpecialEventRepository reaches through
    // getConnection(int) — same server and schema here, as in the shipped
    // seeds (initdb's WorldDBInfo row 0 names the DARKEDEN schema).
    pDatabaseManager->setWorldDefaultConnection(new Connection(host, db, user, password, port));

    return RUN_ALL_TESTS();
}
