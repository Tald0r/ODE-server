//////////////////////////////////////////////////////////////////////////////
// FileName		: Tile.h
// Written By	: reiot@ewestsoft.com
// Description	:
//////////////////////////////////////////////////////////////////////////////

#ifndef __TILE_H__
#define __TILE_H__

#include <forward_list>

#include "Creature.h"
#include "Effect.h"
#include "Exception.h"
#include "Item.h"
#include "Object.h"
#include "Obstacle.h"
#include "Portal.h"
#include "Sector.h"
#include "Types.h"

// macro
#define FLAG_ZERO(set) (set = 0)
#define FLAG_SET(set, flag) (set |= (1 << (flag)))
#define FLAG_CLEAR(set, flag) (set &= ~(1 << (flag)))
#define FLAG_ISSET(set, flag) (set & (1 << (flag)))

// type redefinition
typedef WORD BuildingID_t;
typedef WORD TerrainID_t;

//////////////////////////////////////////////////////////////////////////////
// Tile class:
//////////////////////////////////////////////////////////////////////////////

class Packet;

class Tile {
public:
    enum TileFlags {
        TILE_GROUND_BLOCKED = 0,  // The ground of the tile is blocked
        TILE_AIR_BLOCKED,         // The air of the tile is blocked
        TILE_UNDERGROUND_BLOCKED, // The underground of the tile is blocked
        TILE_WALKING_CREATURE,    // A creature is on the ground of the tile
        TILE_FLYING_CREATURE,     // A creature is in the air of the tile
        TILE_BURROWING_CREATURE,  // A creature is under the ground of the tile
        TILE_ITEM,                // An item is lying on the tile
        TILE_OBSTACLE,            // An obstacle that has state is on the tile
        TILE_EFFECT,              // A magic object is on the tile
        TILE_BUILDING,            // A building is on the tile
        TILE_PORTAL,              // A portal is on the tile
        TILE_TERRAIN              // A background tile that has an effect
    };

public:
    Tile(WORD m_Flag = 0, WORD m_Opt = 0);
    ~Tile();

public:
    bool addCreature(Creature* pCreature, bool bCheckEffect = true, bool bCheckPortal = true);
    void deleteCreature(ObjectID_t creatureID);
    void deleteCreature(Creature::MoveMode mode);
    Creature* getCreature(ObjectID_t creatureID);
    Creature* getCreature(Creature::MoveMode mode);

    bool hasWalkingCreature() const {
        return FLAG_ISSET(m_wFlags, TILE_WALKING_CREATURE) > 0;
    }
    bool hasFlyingCreature() const {
        return FLAG_ISSET(m_wFlags, TILE_FLYING_CREATURE) > 0;
    }
    bool hasBurrowingCreature() const {
        return FLAG_ISSET(m_wFlags, TILE_BURROWING_CREATURE) > 0;
    }
    bool hasCreature(Creature::MoveMode mode) const {
        return FLAG_ISSET(m_wFlags, static_cast<int>(mode) + TILE_WALKING_CREATURE) > 0;
    }
    bool hasCreature() const {
        return FLAG_ISSET(m_wFlags, TILE_WALKING_CREATURE) || FLAG_ISSET(m_wFlags, TILE_BURROWING_CREATURE) ||
               FLAG_ISSET(m_wFlags, TILE_FLYING_CREATURE);
    }

public:
    void addItem(Item* pItem);
    void deleteItem();
    Item* getItem();
    bool hasItem() const {
        return FLAG_ISSET(m_wFlags, TILE_ITEM) > 0;
    }

public:
    void addObstacle(Obstacle* pObstacle);
    void deleteObstacle();
    Obstacle* getObstacle();
    bool hasObstacle() const {
        return FLAG_ISSET(m_wFlags, TILE_OBSTACLE) > 0;
    }

public:
    void addEffect(Effect* pEffect);
    bool canAddEffect();
    void deleteEffect(ObjectID_t effectID);
    Effect* getEffect(ObjectID_t effectID);
    bool hasEffect() const {
        return FLAG_ISSET(m_wFlags, TILE_EFFECT) > 0;
    }
    Effect* getEffect(Effect::EffectClass effectClass);

public:
    void addBuilding(BuildingID_t buildingID);
    void deleteBuilding();
    BuildingID_t getBuilding() const;
    bool hasBuilding() const {
        return FLAG_ISSET(m_wFlags, TILE_BUILDING) > 0;
    }

public:
    void addPortal(Portal* pPortal);
    void deletePortal();
    Portal* getPortal() const;
    bool hasPortal() const {
        return FLAG_ISSET(m_wFlags, TILE_PORTAL) > 0;
    }

public:
    void addTerrain(TerrainID_t terrainID);
    void deleteTerrain();
    TerrainID_t getTerrain() const;
    bool isTerrain() const {
        return FLAG_ISSET(m_wFlags, TILE_TERRAIN) > 0;
    }

public:
    bool isGroundBlocked() const {
        return FLAG_ISSET(m_wFlags, TILE_GROUND_BLOCKED) > 0;
    }
    bool isAirBlocked() const {
        return FLAG_ISSET(m_wFlags, TILE_AIR_BLOCKED) > 0;
    }
    bool isUndergroundBlocked() const {
        return FLAG_ISSET(m_wFlags, TILE_UNDERGROUND_BLOCKED) > 0;
    }
    bool isBlocked(Creature::MoveMode mode) const {
        return FLAG_ISSET(m_wFlags, TILE_GROUND_BLOCKED + static_cast<int>(mode)) > 0;
    }
    void setBlocked(Creature::MoveMode mode) {
        FLAG_SET(m_wFlags, TILE_GROUND_BLOCKED + static_cast<int>(mode));
    }
    void clearBlocked(Creature::MoveMode mode) {
        FLAG_CLEAR(m_wFlags, TILE_GROUND_BLOCKED + static_cast<int>(mode));
    }

    bool isFixedGroundBlocked() const {
        return isGroundBlocked() && !hasWalkingCreature();
    }

public:
    void addObject(Object* pObject);
    void deleteObject(ObjectID_t objectID);
    void deleteObject(ObjectPriority tilePriority);
    Object* getObject(ObjectID_t objectID) const;
    Object* getObject(ObjectPriority tilePriority) const;

public:
    WORD getOption() const {
        return m_wOption;
    }
    void setOption(WORD option) {
        m_wOption = option;
    }

public:
    Sector* getSector(void) const {
        return m_pSector;
    }
    void setSector(Sector* pSector) {
        m_pSector = pSector;
    }

public:
    const forward_list<Object*>& getObjectList() const {
        return m_Objects;
    }
    forward_list<Object*>& getObjectList() {
        return m_Objects;
    }

    string toString() const;

private:
    WORD m_wFlags;                   // Tile attribute flags
    WORD m_wOption;                  // Option flags, used differently depending on the tile attribute flags
    forward_list<Object*> m_Objects; // List of object pointers
    Sector* m_pSector;               // Pointer to the sector this tile belongs to
};

#endif
