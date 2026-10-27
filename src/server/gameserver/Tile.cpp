//////////////////////////////////////////////////////////////////////////////
// FileName 	: Tile.cpp
// Written By	: reiot@ewestsoft.com
// Description	:
//////////////////////////////////////////////////////////////////////////////

#include "Tile.h"

#include <algorithm>

#include "Assert.h"
#include "Creature.h"
#include "EffectDarkness.h"
#include "EffectGreenPoison.h"
#include "EffectTryingPosition.h"
#include "EffectYellowPoison.h"
#include "GamePlayer.h"
#include "Ousters.h"
#include "Player.h"
#include "Sector.h"
#include "Slayer.h"
#include "StringStream.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
Tile::Tile(WORD wFlags, WORD wOption)

{
    m_wFlags = wFlags;
    m_wOption = wOption;
}

//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////
Tile::~Tile()

{
    // Delete every object belonging to the tile.
    while (!m_Objects.empty()) {
        Object* pObj = m_Objects.front();
        SAFE_DELETE(pObj);
        m_Objects.pop_front();
    }
}

//////////////////////////////////////////////////////////////////////////////
// Add the creature to the list.
//
// The return value says whether this was a plain move (true) or activated a Portal (false).
//////////////////////////////////////////////////////////////////////////////
bool Tile::addCreature(Creature* pCreature, bool bCheckEffect, bool bCheckPortal) {
    __BEGIN_DEBUG

    Assert(pCreature != NULL);

    // Get the creature's MoveMode { WALKING | FLYING | BURROWING }.
    Creature::MoveMode mode = pCreature->getMoveMode();

    // The layer matching the added creature's MoveMode must not be blocked.
    Assert(!isBlocked(mode));

    // No creature with the same MoveMode may already be on the tile.
    // Assert(! hasCreature(mode));
    if (hasCreature(mode)) {
        StringStream msg;

        Creature* pWalkingCreature = getCreature(Creature::MOVE_MODE_WALKING);
        Creature* pFlyingCreature = getCreature(Creature::MOVE_MODE_FLYING);
        Creature* pBurrowingCreature = getCreature(Creature::MOVE_MODE_BURROWING);
        Item* pItem = getItem();

        msg << "TileInfo: ";

        if (pWalkingCreature != NULL) {
            msg << "Walking(" << pWalkingCreature->toString().c_str() << ") ";
        }
        if (pFlyingCreature != NULL) {
            msg << "Flying(" << pFlyingCreature->toString().c_str() << ") ";
        }
        if (pBurrowingCreature != NULL) {
            msg << "Burrowing(" << pBurrowingCreature->toString().c_str() << ") ";
        }
        if (pItem != NULL) {
            msg << "Item(" << pItem->toString().c_str() << ") ";
        }

        diagnosticFilelog("tileError.txt", "%s", msg.toString().c_str());

        Assert(false);
    }

    // Put the creature into the list.
    addObject(pCreature);

    // Turn on the matching creature flag.
    FLAG_SET(m_wFlags, TILE_WALKING_CREATURE + static_cast<int>(mode));

    // Turn on the matching blocking flag.
    FLAG_SET(m_wFlags, TILE_GROUND_BLOCKED + static_cast<int>(mode));

    Assert(isBlocked(mode));
    Assert(hasCreature(mode));

    if (bCheckPortal) {
        // A portal is present and the creature is a PC. (Monsters and NPCs do not use portals.)
        if (hasPortal() && pCreature->isPC()) {
            Portal* pPortal = getPortal();
            if (pPortal->activate(pCreature))
                return false;
        }
    }

    // Check effects.
    if (hasEffect()) {
        if (bCheckEffect) {
            EffectGreenPoison* pEGP = (EffectGreenPoison*)getEffect(Effect::EFFECT_CLASS_GREEN_POISON);
            // if (pCreature->isSlayer() && (pEGP = (EffectGreenPoison*)getEffect(Effect::EFFECT_CLASS_GREEN_POISON)))
            if (pEGP != NULL) {
                pEGP->affectCreature(pCreature, true);
            }
            EffectYellowPoison* pEYP = NULL;
            if ((pCreature->isSlayer() || pCreature->isOusters()) &&
                (pEYP = (EffectYellowPoison*)getEffect(Effect::EFFECT_CLASS_YELLOW_POISON))) {
                pEYP->affectCreature(pCreature, true);
            }
            // Apply it if it must be applied unconditionally.
            else if ((pEYP = (EffectYellowPoison*)getEffect(Effect::EFFECT_CLASS_YELLOW_POISON)) && pEYP->isForce()) {
                pEYP->affectCreature(pCreature, true);
            }

            EffectDarkness* pDarkness = NULL;
            if (pCreature->isSlayer() && (pDarkness = (EffectDarkness*)getEffect(Effect::EFFECT_CLASS_DARKNESS))) {
                pDarkness->affectObject(pCreature, true);
            }

            EffectTryingPosition* pTP;
            if (pCreature->isPC() &&
                (pTP = dynamic_cast<EffectTryingPosition*>(getEffect(Effect::EFFECT_CLASS_TRYING_POSITION)))) {
                pTP->affect(pCreature);
            }
        }
    } else {
        if ((pCreature->isOusters() || pCreature->isSlayer()) && pCreature->isFlag(Effect::EFFECT_CLASS_DARKNESS)) {
            pCreature->removeFlag(Effect::EFFECT_CLASS_DARKNESS);
        }
    }

    // PortalException removed.
    return true;

    __END_DEBUG
}

//////////////////////////////////////////////////////////////
// Delete the creature with the given ID from the list.
// This needs optimization. (search + delete)
//////////////////////////////////////////////////////////////
void Tile::deleteCreature(ObjectID_t creatureID) {
    __BEGIN_DEBUG

    try {
        // The current creature must be blocking one of the layers.
        Assert(isGroundBlocked() || isAirBlocked() || isUndergroundBlocked());

        // The current creature must exist on one of the layers.
        // Assert(hasWalkingCreature() || hasFlyingCreature() || hasBurrowingCreature());
        Assert(hasCreature()); // by sigi. 2002.5.8

        Creature* pCreature = dynamic_cast<Creature*>(getObject(creatureID));

        // If an effect exists, remove it from the creature.

        // NoSuchElementException removed.
        if (pCreature == NULL) {
            return;
        }

        if (hasEffect()) {
            EffectTryingPosition* pTP;
            if (pCreature->isPC() &&
                (pTP = dynamic_cast<EffectTryingPosition*>(getEffect(Effect::EFFECT_CLASS_TRYING_POSITION)))) {
                pTP->unaffect(pCreature);
            }
        }


        // Delete the node.
        deleteObject(creatureID);

        // Turn off the matching creature flag.
        FLAG_CLEAR(m_wFlags, TILE_WALKING_CREATURE + static_cast<int>(pCreature->getMoveMode()));

        // Turn off the matching blocking flag.
        FLAG_CLEAR(m_wFlags, TILE_GROUND_BLOCKED + static_cast<int>(pCreature->getMoveMode()));
    } catch (Throwable& t) {
        // cerr << "Delete Creature" << endl;
        // cerr << t.toString() << endl;
        diagnosticFilelog("tileError.txt", "Tile::deleteCreature - %s", t.toString().c_str());
    }

    __END_DEBUG
}

//////////////////////////////////////////////////////////////
// Return the creature with the given ID.
//////////////////////////////////////////////////////////////
Creature* Tile::getCreature(ObjectID_t creatureID) {
    Assert(hasWalkingCreature() || hasFlyingCreature() || hasBurrowingCreature());

    return (Creature*)(getObject(creatureID));
}

//////////////////////////////////////////////////////////////
// Return the creature of the given layer (move mode).
//////////////////////////////////////////////////////////////
Creature* Tile::getCreature(Creature::MoveMode mode) {
    Assert(hasCreature(mode));
    return (Creature*)getObject(ObjectPriority(OBJECT_PRIORITY_WALKING_CREATURE + static_cast<int>(mode)));
}

//////////////////////////////////////////////////////////////
// Add an item to the tile. The tile must not already hold an item.
// (There is one item per tile.)
//////////////////////////////////////////////////////////////
void Tile::addItem(Item* pItem)

{
    Assert(pItem != NULL);

    Assert(!hasItem());
    Assert(!hasBuilding());
    Assert(!hasObstacle());
    Assert(!hasPortal());
    addObject(pItem);

    FLAG_SET(m_wFlags, TILE_ITEM);
}

//////////////////////////////////////////////////////////////
// Delete the item from the tile. There is only one, so nothing needs to be named.
//////////////////////////////////////////////////////////////
void Tile::deleteItem() {
    __BEGIN_DEBUG

    // Assert(hasItem());
    if (!hasItem()) {
        // cerr << "Tile::hasItem() : there is no item." << endl;
        return;
    }

    deleteObject(OBJECT_PRIORITY_ITEM);

    FLAG_CLEAR(m_wFlags, TILE_ITEM);

    __END_DEBUG
}

//////////////////////////////////////////////////////////////
// Return the tile's item. There is only one, so nothing needs to be named.
//////////////////////////////////////////////////////////////
Item* Tile::getItem() {
    // Assert(hasItem());
    if (!hasItem())
        return NULL;

    return (Item*)getObject(OBJECT_PRIORITY_ITEM);
}

//////////////////////////////////////////////////////////////
// Add an obstacle to the tile.
//////////////////////////////////////////////////////////////
void Tile::addObstacle(Obstacle* pObstacle)

{
    Assert(pObstacle != NULL);

    // must be empty tile...
    Assert(!hasWalkingCreature());
    Assert(!hasFlyingCreature());
    Assert(!hasBurrowingCreature());
    Assert(!hasEffect());
    Assert(!hasObstacle());
    Assert(!hasItem());
    Assert(!hasBuilding());
    Assert(!hasPortal());
    Assert(!isTerrain());

    FLAG_SET(m_wFlags, TILE_OBSTACLE);

    addObject(pObstacle);
}

//////////////////////////////////////////////////////////////
// Delete the obstacle from the tile. There is only one, so nothing needs to be named.
//////////////////////////////////////////////////////////////
void Tile::deleteObstacle() {
    Assert(hasObstacle());

    deleteObject(OBJECT_PRIORITY_OBSTACLE);

    FLAG_CLEAR(m_wFlags, TILE_OBSTACLE);
}

//////////////////////////////////////////////////////////////
// Return the tile's obstacle. There is only one, so nothing needs to be named.
//////////////////////////////////////////////////////////////
Obstacle* Tile::getObstacle() {
    Assert(hasObstacle());

    return (Obstacle*)getObject(OBJECT_PRIORITY_OBSTACLE);
}

bool Tile::canAddEffect()

{
    return !(hasObstacle() || hasBuilding() || hasPortal());
}

//////////////////////////////////////////////////////////////
// Add a magic effect to the tile.
// If a creature or item is on the tile, the magic's effect is
// applied to that creature or item.
// A policy for duplicate magic is needed.... (the same magic in one place..)
//////////////////////////////////////////////////////////////
void Tile::addEffect(Effect* pEffect)

{
    Assert(pEffect != NULL);

    Assert(!hasObstacle());
    Assert(!hasBuilding());
    Assert(!hasPortal());

    addObject(pEffect);

    // This is where the effect is applied to the creature or item on the tile.
    // pEffect->affectTile();

    FLAG_SET(m_wFlags, TILE_EFFECT);
}

//////////////////////////////////////////////////////////////
// Delete the magic effect with the given ID from the tile.
// The magic effect applied to the tile's creature or item must be
// removed at the same time.
// When deleting a magic effect, the TILE_EFFECT flag must not be
// turned off while other magic remains!
// This needs optimization.. (search - unaffect - flag clear in one pass..)
//////////////////////////////////////////////////////////////
void Tile::deleteEffect(ObjectID_t effectID) {
    if (!hasEffect()) {
        diagnosticFilelog("TileEffectBug.txt", "there is no effect with effect id %d", effectID);
        return;
    }
    //	Assert(hasEffect());

    // There can be more than one magic effect, so deleteObject(OBJECT_PRIORITY_EFFECT) cannot be used.
    deleteObject(effectID);

    // Undo every influence on the tile, that is, restore the magic
    // effects applied to its creature and item.
    // effect->unaffectTile();

    // Turn the flag off if no other magic remains.

    // NoSuchElementException removed.
    if (getObject(OBJECT_PRIORITY_EFFECT) == NULL) {
        FLAG_CLEAR(m_wFlags, TILE_EFFECT);
    }
}

//////////////////////////////////////////////////////////////
// Return the magic effect with the given ID.
//////////////////////////////////////////////////////////////
Effect* Tile::getEffect(ObjectID_t effectID) {
    if (hasEffect() == false)
        return NULL;
    return (Effect*)getObject(effectID);
}

//////////////////////////////////////////////////////////////
// Return the magic effect with the given EffectClass.
//////////////////////////////////////////////////////////////
Effect* Tile::getEffect(Effect::EffectClass effectClass)

{
    if (hasEffect()) {
        for (forward_list<Object*>::const_iterator itr = m_Objects.begin(); itr != m_Objects.end(); itr++) {
            Effect* pEffect = NULL;
            if ((*itr)->getObjectClass() == Object::OBJECT_CLASS_EFFECT) {
                if (effectClass == ((Effect*)(*itr))->getEffectClass()) {
                    // The effect of that class was found.
                    pEffect = dynamic_cast<Effect*>(*itr);
                    return pEffect;
                }
            }
        }
    }

    return NULL;
}


//////////////////////////////////////////////////////////////
// Mark the current tile as a building.
//////////////////////////////////////////////////////////////
void Tile::addBuilding(BuildingID_t buildingID)

{
    Assert(!hasWalkingCreature());
    Assert(!hasFlyingCreature());
    Assert(!hasBurrowingCreature());
    Assert(!hasEffect());
    Assert(!hasObstacle());
    Assert(!hasItem());
    Assert(!hasBuilding());
    Assert(!hasPortal());
    Assert(!isTerrain());

    FLAG_SET(m_wFlags, TILE_BUILDING);

    m_wOption = buildingID;
}

//////////////////////////////////////////////////////////////
// Delete the building from the tile. There is only one, so nothing needs to be named.
//////////////////////////////////////////////////////////////
void Tile::deleteBuilding()

{
    Assert(hasBuilding());

    FLAG_CLEAR(m_wFlags, TILE_BUILDING);

    m_wOption = 0;
}

//////////////////////////////////////////////////////////////
// Return the building id of the current tile.
//////////////////////////////////////////////////////////////
BuildingID_t Tile::getBuilding() const

{
    Assert(hasBuilding());

    return m_wOption;
}

//////////////////////////////////////////////////////////////
// Add a portal to the tile.
//////////////////////////////////////////////////////////////
void Tile::addPortal(Portal* pPortal)

{
    Assert(pPortal != NULL);

    // Should a flying creature or a burrowed creature be affected by a portal or not?
    // In any case the tile must be completely empty!
    Assert(!hasWalkingCreature());
    Assert(!hasFlyingCreature());
    Assert(!hasBurrowingCreature());
    Assert(!hasEffect());
    Assert(!hasObstacle());
    Assert(!hasItem());
    Assert(!hasBuilding());
    Assert(!hasPortal());
    Assert(!isTerrain());

    addObject(pPortal);

    FLAG_SET(m_wFlags, TILE_PORTAL);
}

//////////////////////////////////////////////////////////////
// Delete the portal from the tile. There is only one, so nothing needs to be named.
//////////////////////////////////////////////////////////////
void Tile::deletePortal()

{
    Assert(hasPortal());

    deleteObject(OBJECT_PRIORITY_PORTAL);

    FLAG_CLEAR(m_wFlags, TILE_PORTAL);
}

//////////////////////////////////////////////////////////////
// Return the portal object.
//////////////////////////////////////////////////////////////
Portal* Tile::getPortal() const

{
    Assert(hasPortal());

    return (Portal*)getObject(OBJECT_PRIORITY_PORTAL);
}

//////////////////////////////////////////////////////////////
// Add a terrain to the tile.
//////////////////////////////////////////////////////////////
void Tile::addTerrain(TerrainID_t terrainID)

{
    // If the tile already holds an obstacle, building or portal, which
    // also use m_wOption, that is an error; the caller must check.
    Assert(!hasObstacle());
    Assert(!hasBuilding());
    Assert(!hasPortal());

    // Turn on the Terrain flag.
    FLAG_SET(m_wFlags, TILE_TERRAIN);

    // Set the option to the Terrain ID.
    m_wOption = terrainID;
}

//////////////////////////////////////////////////////////////
// Delete the terrain from the tile.
//////////////////////////////////////////////////////////////
void Tile::deleteTerrain()

{
    Assert(isTerrain());

    // Clear the Terrain flag.
    FLAG_CLEAR(m_wFlags, TILE_TERRAIN);

    // Clear the option.
    m_wOption = 0;
}

//////////////////////////////////////////////////////////////
// Return the terrain id.
//////////////////////////////////////////////////////////////
TerrainID_t Tile::getTerrain() const

{
    Assert(isTerrain());

    return m_wOption;
}


//////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////
string Tile::toString() const

{
    StringStream msg;

    msg << "Tile(";
    msg << "Flag:" << m_wFlags;
    msg << "\nObjects:";
    forward_list<Object*>::const_iterator itr = m_Objects.begin();
    for (; itr != m_Objects.end(); itr++) {
        msg << (*itr)->toString() << "\n";
    }

    msg << "TileOption:" << (int)m_wOption;
    msg << ")";

    return msg.toString();
}

//////////////////////////////////////////////////////////////
// add object into object list
//////////////////////////////////////////////////////////////
void Tile::addObject(Object* pObject) {
    __BEGIN_DEBUG

    Assert(pObject != NULL);
    Assert(pObject->getObjectID() != 0);

    forward_list<Object*>::iterator before = m_Objects.end();
    forward_list<Object*>::iterator current = m_Objects.begin();

    for (; current != m_Objects.end(); before = current, current++) {
        // The object list is sorted in ascending order.
        // So loop until the ObjectPriority of the object being inserted is
        // smaller than the ObjectPriority the iterator currently points at.

        if (pObject->getObjectPriority() < (*current)->getObjectPriority()) {
            if (before == m_Objects.end()) {
                // The object has the smallest tile priority, so put it at the front of the list.
                m_Objects.push_front(pObject);
            } else {
                // Put it in the middle of the list.
                // O(1) insertion
                m_Objects.insert_after(before, pObject);
            }
            return;
        } else if (pObject->getObjectPriority() == (*current)->getObjectPriority()) {
            // Effects may be duplicated.
            if (pObject->getObjectPriority() == OBJECT_PRIORITY_EFFECT) {
                if (before == m_Objects.end()) {
                    m_Objects.push_front(pObject);
                } else {
                    m_Objects.insert_after(before, pObject);
                }
                return;
            } else {
                cerr << toString() << endl;
                cerr << "Overlapping tile priority value = " << (int)pObject->getObjectPriority() << endl;
                cerr << "Flag value = " << m_wFlags << endl;
                filelog("TILEBUG.log", "%s", toString().c_str());
                throw DuplicatedException("tile priority duplicated");
            }
        }
    }

    // The loop above fails to find a place when
    // (1) the list holds no object at all, or
    // (2) the object has to go at the very end of the list.
    if (current == m_Objects.end()) {
        if (before == m_Objects.end()) {
            // The list is empty, so put it at the front of the list.
            m_Objects.push_front(pObject);
        } else {
            // This object has the largest OBJECT_PRIORITY, so put it at the end of the list.
            // O(1) insertion
            m_Objects.insert_after(before, pObject);
        }
    }

    __END_DEBUG
}

//////////////////////////////////////////////////////////////
// Delete object from object list
//////////////////////////////////////////////////////////////
void Tile::deleteObject(ObjectID_t objectID) {
    forward_list<Object*>::iterator before = m_Objects.end();
    forward_list<Object*>::iterator current = m_Objects.begin();

    for (; current != m_Objects.end(); before = current++) {
        if (objectID == (*current)->getObjectID()) {
            // An object with that id was found.
            if (before == m_Objects.end()) {
                // Delete first node
                m_Objects.pop_front();
            } else {
                // O(1) deletion
                m_Objects.erase_after(before);
            }

            return;
        }
    }

    Assert(false);

    // NoSuchElementException removed.
    // throw NoSuchElementException("invalid object id");
}

//////////////////////////////////////////////////////////////
// Delete the object with the given Tile Priority.
//////////////////////////////////////////////////////////////
void Tile::deleteObject(ObjectPriority objectPriority) {
    forward_list<Object*>::iterator before = m_Objects.end();
    forward_list<Object*>::iterator current = m_Objects.begin();

    for (; current != m_Objects.end(); before = current++) {
        if (objectPriority == (*current)->getObjectPriority()) {
            // An object with that tp was found.
            if (before == m_Objects.end()) {
                // Delete first node
                m_Objects.pop_front();
            } else {
                // O(1) deletion
                m_Objects.erase_after(before);
            }

            return;
        } else if (objectPriority < (*current)->getObjectPriority()) {
            // The list is sorted in ascending order of object tp, so if the
            // iterator's tp is greater than the tp being looked for,
            // no object with that priority exists.
            // ex> in [0] - [3] - [4], the iterator points at [3] while the tp sought is 2.
            break;
        }
    }

    // NoSuchElementException removed.
    // throw NoSuchElementException("invalid object priority");
}
//////////////////////////////////////////////////////////////
// Return the object with the given ID from the current tile.
// The whole list has to be searched.
//////////////////////////////////////////////////////////////
Object* Tile::getObject(ObjectID_t objectID) const {
    for (forward_list<Object*>::const_iterator itr = m_Objects.begin(); itr != m_Objects.end(); itr++) {
        if (objectID == (*itr)->getObjectID()) {
            // An object with that id was found.
            return *itr;
        }
    }

    // no object with that id exists.
    // NoSuchElementException removed.
    // throw NoSuchElementException("invalid object id");

    // Present only to avoid a warning.
    return NULL;
}

//////////////////////////////////////////////////////////////
// Return the object with the given Tile Priority.
//////////////////////////////////////////////////////////////
Object* Tile::getObject(ObjectPriority objectPriority) const {
    for (forward_list<Object*>::const_iterator itr = m_Objects.begin(); itr != m_Objects.end(); itr++) {
        if (objectPriority == (*itr)->getObjectPriority()) {
            // An object with that priority was found.
            return *itr;
        } else if (objectPriority < (*itr)->getObjectPriority()) {
            // The list is sorted in ascending order of object tp, so if the
            // iterator's tp is greater than the tp being looked for,
            // no object with that priority exists.
            // ex> in [0] - [3] - [4], the iterator points at [3] while the tp sought is 2.
            break;
        }
    }

    // NoSuchElementException removed.
    // throw NoSuchElementException("invalid tile priority");
    return NULL;
}
