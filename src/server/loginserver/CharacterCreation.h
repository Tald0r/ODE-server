//////////////////////////////////////////////////////////////////////////////
// Filename    : CharacterCreation.h
// Description : the loginserver's character-creation decision, separated
//               from the CLCreatePC handler so it can be exercised without
//               a socket or a database.
//////////////////////////////////////////////////////////////////////////////

#ifndef __CHARACTER_CREATION_H__
#define __CHARACTER_CREATION_H__

#include <functional>
#include <string>

#include "Outcome.h"
#include "Types.h"
#include "repository/LoginCharacterRepository.h"

// Why a creation was refused. Each value maps to one LCCreatePCError code
// except InvalidAttributes, InvalidSlot and InvalidHairStyle, which no
// client can produce by playing and which the handler answers by dropping
// the connection.
//
// They are decided in this order: ReservedName, DisallowedCharacters,
// NameTaken, InvalidSlot, InvalidHairStyle, SlotOccupied,
// InvalidAttributes, UnknownRace.
enum class CreatePCRejection {
    // The name contains a reserved token (see isAvailableID).
    ReservedName,
    // A Slayer row of that name already exists in the world.
    NameTaken,
    // The account already has an ACTIVE character in that slot.
    SlotOccupied,
    // The name uses characters the regional charset filter refuses. Only
    // the Thailand and China builds have that filter, so only they
    // produce this.
    DisallowedCharacters,
    // STR/DEX/INT are outside what the race allows.
    InvalidAttributes,
    // The slot byte names none of the three slots. It is the index into
    // Slot2String on every row the creation writes, so it is checked
    // before the first of them.
    InvalidSlot,
    // The hair style names none of the three styles. It is the index
    // into HairStyle2String on the Slayer row.
    InvalidHairStyle,
    // The race byte names none of the three races.
    UnknownRace
};

// A creation request: the CLCreatePC packet's fields plus the session
// state the handler reads off the player.
struct CreatePCRequest {
    WorldID_t worldID = 0;
    ServerGroupID_t serverGroupID = 0;
    std::string playerID;
    std::string name;
    // The slot and hair style as plain integers rather than Slot / HairStyle,
    // so a caller can hand the decision a value that names no enumerator
    // and have it refused. Holding one in an enum member would be
    // undefined to load, which would put the value beyond the reach of the
    // range check that guards Slot2String / HairStyle2String.
    int slot = SLOT1;
    Sex sex = FEMALE;
    int hairStyle = HAIR_STYLE1;
    Color_t hairColor = 0;
    Color_t skinColor = 0;
    Attr_t str = 0;
    Attr_t dex = 0;
    Attr_t inte = 0;
    Race_t race = RACE_SLAYER;
};

struct CreatePCActions {
    // Raw nonnegative draws, reduced by the decision with the legacy modulo
    // rules. Required only for a valid Vampire request; exactly two draws are
    // consumed even when the second modulo has only one possible result.
    std::function<unsigned()> random;
    // Optional best-effort diagnostic. Its failures cannot change the result.
    std::function<void(const CreatePCRequest&)> reportLowOusters;
};

// The rows an accepted creation writes. Every character gets the Slayer
// row; hasOustersRow says whether the second row is the Ousters one or
// the Vampire one.
struct CreatedCharacter {
    // The attributes the character is actually created with. A vampire's
    // are rolled during the decision and differ from the request's.
    Attr_t str = 0;
    Attr_t dex = 0;
    Attr_t inte = 0;

    LoginNewSlayer slayer;
    bool hasOustersRow = false;
    LoginNewVampire vampire;
    LoginNewOusters ousters;
    LoginFlagSetPreset flagSet = LOGIN_FLAGSET_SLAYER;
};

// The level-1 balance rows a creation prices its starting stats from.
// They never change while a server runs, so each is read once and kept;
// a row that is missing is retried on the next creation. One instance
// lives for the loginserver's lifetime, which is why the values are not
// keyed by world: the first world to create a character fixes them.
class CreatePCBalanceCache {
public:
    // RankEXPInfo.GoalExp at Level 1 for one rank type (0 Slayer,
    // 1 Vampire, 2 Ousters). -1 while the row has not been read.
    int rankGoalExp(LoginCharacterRepository& repository, WorldID_t worldID, int rankType);
    // VampEXPBalanceInfo.GoalExp / OustersEXPBalanceInfo.GoalExp at Level 1.
    int vampireGoalExp(LoginCharacterRepository& repository, WorldID_t worldID);
    int oustersGoalExp(LoginCharacterRepository& repository, WorldID_t worldID);
    // <STR|DEX|INT>BalanceInfo.GoalExp / .AccumExp at one level, 0 when
    // there is no such row. A level outside the cached range is read
    // straight from the repository and not kept.
    int attrGoalExp(LoginCharacterRepository& repository, WorldID_t worldID, LoginAttrTable attr, int level);
    int attrAccumExp(LoginCharacterRepository& repository, WorldID_t worldID, LoginAttrTable attr, int level);

private:
    static const int kRankTypeMax = 3;
    static const int kCachedLevels = 100;

    int m_RankGoalExp[kRankTypeMax] = {-1, -1, -1};
    int m_VampireGoalExp = -1;
    int m_OustersGoalExp = -1;
    int m_AttrGoalExp[LOGIN_ATTR_TABLE_MAX][kCachedLevels] = {};
    int m_AttrAccumExp[LOGIN_ATTR_TABLE_MAX][kCachedLevels] = {};
};

// Decide whether a character may be created, and with which rows.
//
// The repository is passed in because two of the rejections and all of
// the starting stats are database reads; the writes stay with the caller,
// so this function decides over supplied reads and random draws and needs no
// database, process random state or log file in a test. Callbacks are synchronous
// and must not mutate the request. Reporting cannot replace an operation failure.
//
// Repository and random-source failures propagate unchanged. Earlier cache
// fills and consumed draws are retained after failure; they cannot be rolled back.
[[nodiscard]] Outcome<CreatedCharacter, CreatePCRejection> decideCreatePC(const CreatePCRequest& request,
                                                                          LoginCharacterRepository& repository,
                                                                          CreatePCBalanceCache& balance,
                                                                          const CreatePCActions& actions);

// Is the name free of the reserved tokens (NONE, GM, the Korean staff
// words)? Shared with CLQueryCharacterNameHandler.
bool isAvailableID(const char* pID);

#endif
