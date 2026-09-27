//////////////////////////////////////////////////////////////////////////////
// Filename    : ZoneUtil.h
// Written by  : excel96
// Description :
// Functions that perform zone-related work; keeping them inside Zone made
// the zone file too large, so they were pulled out of it.
//////////////////////////////////////////////////////////////////////////////

#ifndef __ZONE_UTIL_H__
#define __ZONE_UTIL_H__

#include <list>

#include "Creature.h"
#include "Effect.h"
#include "Exception.h"
#include "Types.h"
#include "VSDateTime.h"

// forward declaration
class Zone;
class Mine;
class Effect;
class PlayerCreature;
class Item;
class Monster;
class Slayer;
class Corpse;

class ZONE_COORD;
struct SUMMON_INFO;

// Summoning by kind.
struct SUMMON_INFO2;

//////////////////////////////////////////////////////////////////////////////
// Find a position where a given creature can be added.
//
// Zone*       pZone        : pointer to the zone
// ZoneCoord_t cx           : initial x to add at
// ZoneCoord_t cy           : initial y to add at
// Creature::MoveMode MMode : the creature's move mode
//////////////////////////////////////////////////////////////////////////////
TPOINT findSuitablePosition(Zone* pZone, ZoneCoord_t cx, ZoneCoord_t cy, Creature::MoveMode MMode);


//////////////////////////////////////////////////////////////////////////////
// Find a position where a given item can be added.
//
// Zone*       pZone          : pointer to the zone
// ZoneCoord_t cx             : initial x to add at
// ZoneCoord_t cy             : initial y to add at
// bool        bAllowCreature : is a place holding a creature acceptable?
//////////////////////////////////////////////////////////////////////////////
TPOINT findSuitablePositionForItem(Zone* pZone, ZoneCoord_t cx, ZoneCoord_t cy, bool bAllowCreature,
                                   bool bAllowSafeZone = true, bool bForce = false);

//////////////////////////////////////////////////////////////////////////////
// Find a position where a given effect can be added.
//
// Zone*       pZone          : pointer to the zone
// ZoneCoord_t cx             : initial x to add at
// ZoneCoord_t cy             : initial y to add at
// Effect::EffectClass EClass : the effect class to add
//////////////////////////////////////////////////////////////////////////////
TPOINT findSuitablePositionForEffect(Zone* pZone, ZoneCoord_t cx, ZoneCoord_t cy, Effect::EffectClass EClass);

//////////////////////////////////////////////////////////////////////////////
// Check whether a creature of the given move mode can be added at a position.
//
// Zone*              pZone : pointer to the zone
// ZoneCoord_t        x     : x of the coordinate to burrow at
// ZoneCoord_t        y     : y of the coordinate to burrow at
// Creature::MoveMode MMode : the creature's move mode
//////////////////////////////////////////////////////////////////////////////
bool canAddCreature(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Creature::MoveMode MMode);


//////////////////////////////////////////////////////////////////////////////
// Check whether burrowing is possible at a position.
//
// Zone* pZone   : pointer to the zone
// ZoneCoord_t x : x of the coordinate to burrow at
// ZoneCoord_t y : y of the coordinate to burrow at
//////////////////////////////////////////////////////////////////////////////
bool canBurrow(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y);


//////////////////////////////////////////////////////////////////////////////
// Check whether unburrowing is possible at a position.
//
// Zone* pZone   : pointer to the zone
// ZoneCoord_t x : x of the coordinate to burrow at
// ZoneCoord_t y : y of the coordinate to burrow at
//////////////////////////////////////////////////////////////////////////////
bool canUnburrow(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y);


//////////////////////////////////////////////////////////////////////////////
// Push the creature backwards.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the creature to push back
// ZoneCoord_t originX   : x of the opponent that pushed pCreature back
// ZoneCoord_t originY   : y of the opponent that pushed pCreature back
//////////////////////////////////////////////////////////////////////////////
Dir_t knockbackCreature(Zone* pZone, Creature* pCreature, ZoneCoord_t originX, ZoneCoord_t originY);


//////////////////////////////////////////////////////////////////////////////
// Add a creature that has used hide to the zone.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the creature that used hide
// ZoneCoord_t cx        : the creature's original x
// ZoneCoord_t cy        : the creature's original y
//////////////////////////////////////////////////////////////////////////////
void addBurrowingCreature(Zone* pZone, Creature* pCreature, ZoneCoord_t cx, ZoneCoord_t cy);


//////////////////////////////////////////////////////////////////////////////
// Add a creature that has come out of hide to the zone.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the creature that left hide
// ZoneCoord_t cx        : the creature's original x
// ZoneCoord_t cy        : the creature's original y
// Dir_t       dir       : the direction the emerging creature faces
//////////////////////////////////////////////////////////////////////////////
void addUnburrowCreature(Zone* pZone, Creature* pCreature, ZoneCoord_t cx, ZoneCoord_t cy, Dir_t dir);


//////////////////////////////////////////////////////////////////////////////
// Add a creature that has left its transformation to the zone.
//
// Zone*     pZone     : pointer to the zone
// Creature* pCreature : the creature that left its transformation
// bool      bForce    : is it being ended by force while the effect's
//                       duration has not expired?
//////////////////////////////////////////////////////////////////////////////
void addUntransformCreature(Zone* pZone, Creature* pCreature, bool bForce);


//////////////////////////////////////////////////////////////////////////////
// Add an invisible creature.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the invisible creature
// ZoneCoord_t cx        : the creature's original x
// ZoneCoord_t cy        : the creature's original y
//////////////////////////////////////////////////////////////////////////////
void addInvisibleCreature(Zone* pZone, Creature* pCreature, ZoneCoord_t cx, ZoneCoord_t cy);


//////////////////////////////////////////////////////////////////////////////
// Add a creature that was invisible and has become visible.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the creature that was invisible
// bool        bForced   : was it forced into the visible state?
//////////////////////////////////////////////////////////////////////////////
void addVisibleCreature(Zone* pZone, Creature* pCreature, bool bForced);


//////////////////////////////////////////////////////////////////////////////
// Add a creature in sniping mode.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the creature entering sniping mode
// ZoneCoord_t cx        : the creature's original x
// ZoneCoord_t cy        : the creature's original y
//////////////////////////////////////////////////////////////////////////////
void addSnipingModeCreature(Zone* pZone, Creature* pCreature, ZoneCoord_t cx, ZoneCoord_t cy);


//////////////////////////////////////////////////////////////////////////////
// Add a creature that left sniping mode.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the creature that was in sniping mode
// bool        bForced   : was it forced out of sniping mode?
//////////////////////////////////////////////////////////////////////////////
void addUnSnipingModeCreature(Zone* pZone, Creature* pCreature, bool bForced);


//////////////////////////////////////////////////////////////////////////////
// Add a mine to the zone.
//
// Zone*       pZone : pointer to the zone
// Mine*       pMine : pointer to the mine object
// ZoneCoord_t cx    : x to add the mine at
// ZoneCoord_t cy    : y to add the mine at
//////////////////////////////////////////////////////////////////////////////
void addInstalledMine(Zone* pZone, Mine* pMine, ZoneCoord_t cx, ZoneCoord_t cy);


//////////////////////////////////////////////////////////////////////////////
// Check whether a creature has stepped on a mine.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the creature to check
// ZoneCoord_t X         : x of the coordinate to check
// ZoneCoord_t Y         : y of the coordinate to check
//////////////////////////////////////////////////////////////////////////////
bool checkMine(Zone* pZone, Creature* pCreature, ZoneCoord_t X, ZoneCoord_t Y);
bool checkMine(Zone* pZone, ZoneCoord_t X, ZoneCoord_t Y);

bool checkTrap(Zone* pZone, Creature* pCreature);

//////////////////////////////////////////////////////////////////////////////
// Move a creature to another zone.
//
// Creature*   pCreature    : the creature to move
// ZoneID_t    TargetZoneID : ID of the zone to move to
// ZoneCoord_t TargetX      : X in the destination zone
// ZoneCoord_t TargetY      : Y in the destination zone
// bool        bSendMoveOK  : whether GCMoveOK is sent
//////////////////////////////////////////////////////////////////////////////
void transportCreature(Creature* pCreature, ZoneID_t TargetZoneID, ZoneCoord_t TX, ZoneCoord_t TY,
                       bool bSendMoveOK = true);


//////////////////////////////////////////////////////////////////////////////
// Find the zone with the given zone ID and return its pointer.
// ZoneID_t ZID : the zone ID to look for
//////////////////////////////////////////////////////////////////////////////
Zone* getZoneByZoneID(ZoneID_t ZID);

//////////////////////////////////////////////////////////////////////////////
// Operator command that adds monsters of a given type to a zone.
//////////////////////////////////////////////////////////////////////////////
void addMonstersToZone(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, SpriteType_t SType, MonsterType_t MType, int num,
                       const SUMMON_INFO& summonInfo, list<Monster*>* pSummonedMonsters = NULL);

void addMonstersToZone(Zone* pZone, const SUMMON_INFO2& summonInfo, list<Monster*>* pSummonedMonsters = NULL);

//////////////////////////////////////////////////////////////////////////////
// Checks whether a creature is currently inside a safe zone.
// Used when exchanging.
//////////////////////////////////////////////////////////////////////////////
bool isInSafeZone(Creature* pCreature);

//////////////////////////////////////////////////////////////////////////////
// Check whether the coordinate is inside the zone's bounds.
//////////////////////////////////////////////////////////////////////////////
bool isValidZoneCoord(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, int offset = 0);

//////////////////////////////////////////////////////////////////////////////
// Can pCreature enter the master lair?
//////////////////////////////////////////////////////////////////////////////
bool enterMasterLair(Zone* pZone, Creature* pCreature);

// Information about the destination zone when the field HQ sends beginners elsewhere.
void checkNewbieTransportToGuild(Slayer* pSlayer);
void getNewbieTransportZoneInfo(Slayer* pSlayer, ZONE_COORD& zoneInfo);

// Add a Corpse to a Zone.
bool addCorpseToZone(Corpse* pCorpse, Zone* pZone, ZoneCoord_t cx, ZoneCoord_t cy);

// Check whether a corpse of the given monster is inside the range.
// true if present, false otherwise.
bool checkCorpse(Zone* pZone, MonsterType_t MType, ZoneCoord_t x1, ZoneCoord_t y1, ZoneCoord_t x2, ZoneCoord_t y2);

void makeZoneIDList(const string& zoneIDs, list<ZoneID_t>& zoneIDList);

uint getZoneTimeband(Zone* pZone);

bool createBulletinBoard(Zone* pZone, ZoneCoord_t X, ZoneCoord_t Y, MonsterType_t type, const string& msg,
                         const VSDateTime& timeLimit);
void loadBulletinBoard(Zone* pZone);

void forbidDarkness(Zone* pZone, ZoneCoord_t X, ZoneCoord_t Y, int range);

#endif
