////////////////////////////////////////////////////////////////////////////////
// Filename : CreatureUtil.h
// Description :
// Holds the parts common to Slayer and Vampire, pulled out to keep the size
// of those two files as small as possible.
// Once a PlayerCreature class exists, the contents of this file should be
// folded into it.
////////////////////////////////////////////////////////////////////////////////

#ifndef __CREATURE_UTIL_H__
#define __CREATURE_UTIL_H__

#include "Creature.h"
#include "Item.h"

class EffectObservingEye;
class EffectGnomesWhisper;
class Ousters;
class PlayerCreature;
class GamePlayer;

////////////////////////////////////////////////////////////////////////////////
// Are the two creatures of the same creature class?
////////////////////////////////////////////////////////////////////////////////
bool isSameRace(Creature* pCreature1, Creature* pCreature2);

////////////////////////////////////////////////////////////////////////////////
// Find an item by ObjectID
////////////////////////////////////////////////////////////////////////////////
Item* findItemOID(Creature* pCreature, ObjectID_t objectID);
Item* findItemOID(Creature* pCreature, ObjectID_t objectID, Item::ItemClass IClass);
Item* findItemOID(Creature* pCreature, ObjectID_t objectID, int& storage, int& x, int& y);
Item* findItemOID(Creature* pCreature, ObjectID_t objectID, Item::ItemClass IClass, int& storage, int& x, int& y);

////////////////////////////////////////////////////////////////////////////////
// Find an item by ItemID
////////////////////////////////////////////////////////////////////////////////
Item* findItemIID(Creature* pCreature, ItemID_t itemID);
Item* findItemIID(Creature* pCreature, ItemID_t itemID, Item::ItemClass IClass);
Item* findItemIID(Creature* pCreature, ItemID_t itemID, int& storage, int& x, int& y);
Item* findItemIID(Creature* pCreature, ItemID_t itemID, Item::ItemClass IClass, int& storage, int& x, int& y);

////////////////////////////////////////////////////////////////////////////////
// Compute experience.
// Function that computes the experience gained when a Vampire drains or
// kills a creature.
////////////////////////////////////////////////////////////////////////////////

const int BLOODDRAIN_EXP = 70; // 70% when drained
const int KILL_EXP = 30;       // 30% when killed

int computeCreatureExp(Creature* pCreature, int percent, Ousters* pOusters = NULL);
int computeBloodDrainHealPoint(Creature* pCreature, int percent);

////////////////////////////////////////////////////////////////////////////////
// Check whether the creature is an event monster.
////////////////////////////////////////////////////////////////////////////////
bool isEventMonster(Creature* pCreature);

////////////////////////////////////////////////////////////////////////////////
// Function that checks whether a given creature can currently move
////////////////////////////////////////////////////////////////////////////////
bool isAbleToMove(Creature* pCreature);

////////////////////////////////////////////////////////////////////////////////
// Function that checks whether a given creature can currently use a skill
// SkillType is the skill being used, with no default: a transformed vampire's
// allowed skills are read from it.
////////////////////////////////////////////////////////////////////////////////
bool isAbleToUseSelfSkill(Creature* pCreature, SkillType_t SkillType);
bool isAbleToUseObjectSkill(Creature* pCreature, SkillType_t SkillType);
bool isAbleToUseTileSkill(Creature* pCreature, SkillType_t SkillType);
bool isAbleToUseInventorySkill(Creature* pCreature, BYTE X, BYTE Y, BYTE TX, BYTE TY, SkillType_t SkillType);

////////////////////////////////////////////////////////////////////////////////
// Function that checks whether a given creature can currently pick up an item
////////////////////////////////////////////////////////////////////////////////
bool isAbleToPickupItem(Creature* pCreature, Item* pItem);
bool canDropToZone(Creature* pCreature, Item* pItem);

// void confirmGrandMaster(Creature* pCreature);

bool getRaceFromDB(const string& name, Race_t& race);
bool getGuildIDFromDB(const string& name, Race_t race, GuildID_t& guildID);

bool canSee(const Creature* pSource, Creature* pTarget, EffectObservingEye* pEffectObservingEye = NULL,
            EffectGnomesWhisper* pEffectGnomesWhisper = NULL);

int changeSexEx(PlayerCreature* pPC);
void initAllStatAndSendChange(PlayerCreature* pPC);

void addSimpleCreatureEffect(Creature* pCreature, Effect::EffectClass eClass, int time = -1, bool isSend = true);
void deleteCreatureEffect(Creature* pCreature, Effect::EffectClass eClass);
bool dropFlagToZone(Creature* pPC, bool bSendPacket = true);

void disableFlags(Creature* pCreature, Zone* pZone, SkillType_t SkillType);
bool canEnterBeginnerZone(Creature* pCreature);

void disableFlags(Creature* pCreature, Zone* pZone, SkillType_t SkillType);
bool dropSweeperToZone(Creature* pPC, bool bSendPacket = true);

Level_t getPCLevel(PlayerCreature* pPC);
void sendPetInfo(GamePlayer* pGamePlayer, bool bBroadcast = false, bool bSummon = false);
void giveGoldMedal(PlayerCreature* pPC);
void giveLotto(PlayerCreature* pPC, BYTE type, uint num);

void addOlympicStat(PlayerCreature* pPC, BYTE type, uint num = 1);

void deletePC(PlayerCreature* pPC);

bool isAffectExp2X();

#endif
