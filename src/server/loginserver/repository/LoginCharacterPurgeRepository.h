#ifndef __LOGIN_CHARACTER_PURGE_REPOSITORY_H__
#define __LOGIN_CHARACTER_PURGE_REPOSITORY_H__

#include <string>

#include "Types.h"

// The loginserver's character deletion (CLDeletePCHandler): the ownership
// check on the Slayer row, the Slayer row's retirement, the DeleteChar
// record, and the purge of the character's remaining rows -- the Vampire
// and Ousters rows, RankBonusData, 87 <Class>Object tables and GQuestSave,
// CoupleInfo by either partner column, fourteen Effect* tables and
// EnemyErase, FlagSet, TimeLimitItems, EventQuestAdvance and
// MofusPowerPoint. destroyItems is ItemDestroyer's older, shorter sweep of
// 40 item tables (MaceObject twice); the loginserver builds the class but
// nothing calls it (its one call site in the handler is commented out).
//
// Two connections. recordDeletion and destroyItems use the thread's
// DARKEDEN connection. The other three ask
// de::serverContext().database().getConnection(worldID), the int overload keyed by
// WorldID: the loginserver's DatabaseManager::init() opens one connection
// per WorldDBInfo row into that map, so the statements reach the world
// the character lives in. A WorldID with no row falls through to the
// world-default connection, which the loginserver never sets, so
// createStatement runs on a null Connection. A process whose map is empty
// (no WorldDBInfo rows; the integration tier) gets the world-default
// connection for every id and Asserts if that is unset too.
//
// The Slayer table is the character index: CLCreatePCHandler inserts a
// Slayer row for every race and a Vampire or Ousters row besides, so the
// ownership check and the retirement consult Slayer whatever the race, and
// the purge's Vampire/Ousters statements match the row the character has.
// The race rows are set INACTIVE, not deleted, and the three skill-save
// tables are left alone. The gameserver's own purge
// (CharacterPurgeRepository, run when a gameserver deletes a character)
// deletes the skill saves in every build, lacks six of the object tables
// here (CarryingReceiver, ShoulderArmor, Dermis, Persona, Fascia, Mitten)
// and MofusPowerPoint, and retires all three race rows by name alone.
//
// Not enclosed. The gameserver's repositories own the same tables'
// day-to-day rows (ItemObject, QuestItem, SkillSave, RankBonus, Couple,
// EffectSave, FlagSet, Item, QuestInfo, PlayRecord, MofusPoint, Character,
// CharacterPurge). In the loginserver CLCreatePCHandler.cpp inserts the
// race rows and FlagSet; CLSelectPCHandler.cpp updates ServerGroupID on
// the race rows and, with LoginPlayer.cpp and
// CLQueryCharacterNameHandler.cpp, reads them. The sharedserver's
// GS*GuildMemberHandler.cpp and GSQuitGuildHandler.cpp update GuildID on
// the race rows. DeleteChar is written by nothing else and read by nothing
// in the tree.
//
// The character name, the account id and the slot text are interpolated
// raw. The slot indexes Slot2String unchecked here; callers must preserve the
// slot range enforced by CLDeletePC::read.
class LoginCharacterPurgeRepository {
public:
    virtual ~LoginCharacterPurgeRepository() {}

    // The PlayerID of the ACTIVE Slayer row of that name, on the world
    // connection. True only when the statement returns exactly one row
    // (Name is the primary key, so never more); playerID is untouched
    // otherwise.
    virtual bool loadActiveSlayerOwner(WorldID_t worldID, const std::string& name, std::string& playerID) = 0;

    // Active='INACTIVE' (DELETE under the three build flags above) for the
    // Slayer row of that name AND slot, on the world connection. Returns
    // whether exactly one row was affected: a row already INACTIVE, or in
    // another slot, answers false.
    virtual bool retireSlayer(WorldID_t worldID, const std::string& name, Slot slot) = 0;

    // One DeleteChar row (PlayerID, WorldID, Name, delDate = now()) on the
    // DARKEDEN connection. Its caller skips it under the three build flags
    // above.
    virtual void recordDeletion(const std::string& playerID, WorldID_t worldID, const std::string& name) = 0;

    // The Vampire and Ousters rows of that name AND slot set INACTIVE
    // (DELETE under the flags), then every other row of that name in the
    // tables listed above -- 112 statements in the default build, in a
    // fixed order, on one Statement, with no transaction: a failure
    // part-way leaves the earlier deletes done. Does not touch Slayer.
    virtual void purgeCharacterRows(WorldID_t worldID, const std::string& name, Slot slot) = 0;

    // ItemDestroyer's 41 DELETEs by OwnerID on the DARKEDEN connection, one
    // Statement, no transaction.
    virtual void destroyItems(const std::string& ownerID) = 0;
};

// The process-wide MySQL-backed instance, wired in
// MySQLLoginCharacterPurgeRepository.cpp.
LoginCharacterPurgeRepository& defaultLoginCharacterPurgeRepository();

#endif
