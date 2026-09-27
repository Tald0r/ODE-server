/////////////////////////////////////////////////////////////////////////////
// DynamicZoneGateOfAlter.cpp
/////////////////////////////////////////////////////////////////////////////

#include "DynamicZoneGateOfAlter.h"

#include <stdio.h>
#include <stdlib.h>

#include "Assert.h"
#include "DB.h"
#include "DynamicZoneInfo.h"
#include "EffectDeleteTile.h"
#include "EffectManager.h"
#include "GCAddEffectToTile.h"
#include "GCDeleteEffectFromTile.h"
#include "GCSystemMessage.h"
#include "GQuestManager.h"
#include "PCManager.h"
#include "PlayerCreature.h"
#include "Zone.h"
#include "ZoneUtil.h"

const BYTE EffectMask[32][32] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

enum GATE_OF_ALTER_STATUS {
    ADD_TILE_EFFECT_STATUS,
    WAIT_FOR_PC,
    DELETE_TILE_EFFECT_STATUS,
    OPEN_GATE_TO_ALTER,
    WAIT_FOR_CLEAR,
    REMOVE_TILE_EFFECT,
    WAIT_FOR_NO_EFFECT,
    GATE_OF_ALTER_STATUS_END,

    GATE_OF_ALTER_STATUS_MAX
};

DynamicZoneGateOfAlter::DynamicZoneGateOfAlter() {}

DynamicZoneGateOfAlter::~DynamicZoneGateOfAlter() {}

void DynamicZoneGateOfAlter::init() {
    m_GOAStatus = ADD_TILE_EFFECT_STATUS;
    getCurrentTime(m_Deadline);
    m_Deadline.tv_sec += 60;
}

void DynamicZoneGateOfAlter::heartbeat() {
    switch (m_GOAStatus) {
    case ADD_TILE_EFFECT_STATUS:
        if (addTileEffect()) {
            m_GOAStatus = WAIT_FOR_PC;
            cout << "WAIT_FOR_PC" << endl;
        }
        break;

    case WAIT_FOR_PC:
        if (checkPC()) {
            processEntering();
            addFakeEffect();
            m_GOAStatus = DELETE_TILE_EFFECT_STATUS;
            cout << "DELETE_TILE_EFFECT_STATUS" << endl;
        }
        break;

    case DELETE_TILE_EFFECT_STATUS:
        if (checkNoEffect()) {
            m_GOAStatus = OPEN_GATE_TO_ALTER;
            cout << "OPEN_GATE_TO_ALTER" << endl;
        }
        break;

    case OPEN_GATE_TO_ALTER:
        if (openGateToAlter()) {
            m_GOAStatus = WAIT_FOR_CLEAR;
            cout << "WAIT_FOR_CLEAR" << endl;
        }
        break;

    case WAIT_FOR_CLEAR:
        if (!checkPC()) {
            m_GOAStatus = REMOVE_TILE_EFFECT;
            cout << "REMOVE_TILE_EFFECT" << endl;
        }
        break;

    case REMOVE_TILE_EFFECT:
        if (removeTileEffect()) {
            m_GOAStatus = WAIT_FOR_NO_EFFECT;
            cout << "WAIT_FOR_NO_EFFECT" << endl;
        }
        break;

    case WAIT_FOR_NO_EFFECT:
        if (checkNoEffect()) {
            m_GOAStatus = GATE_OF_ALTER_STATUS_END;
            m_Status = DYNAMIC_ZONE_STATUS_READY;
            cout << "READY" << endl;
        }
        break;
    }

    if (!checkPC()) {
        if (m_GOAStatus == WAIT_FOR_PC || m_GOAStatus == ADD_TILE_EFFECT_STATUS) {
            Timeval current;
            getCurrentTime(current);

            if (current > m_Deadline) {
                cout << "----------------------------Wait Time Out" << endl;
                m_GOAStatus = WAIT_FOR_CLEAR;
            }
        } else if (m_GOAStatus == DELETE_TILE_EFFECT_STATUS) {
            m_GOAStatus = REMOVE_TILE_EFFECT;
        }
    }
}

bool DynamicZoneGateOfAlter::addTileEffect() {
    Assert(m_pZone != NULL);

    // Lay effects on the ground.
    for (ZoneCoord_t x = 0; x < m_pZone->getWidth(); ++x)
    // for ( ZoneCoord_t x = 15; x <= 17; ++x )
    {
        for (ZoneCoord_t y = 0; y < m_pZone->getHeight(); ++y)
        // for ( ZoneCoord_t y = 15; y <= 17; ++y )
        {
            if (EffectMask[y][x] != 0) {
                // Get the tile and check whether an effect can be created on it
                Tile& tile = m_pZone->getTile(x, y);

                if (!tile.canAddEffect() || tile.hasEffect())
                    continue;

                // Create the effect object
                EffectDeleteTile* pEffect = new EffectDeleteTile(m_pZone, x, y);
                pEffect->setDeadline(999999);

                // Register the OID with the zone
                m_pZone->registerObject(pEffect);
                m_pZone->addEffect(pEffect);

                // Attach it to the tile
                tile.addEffect(pEffect);
            }
        }
    }

    return true;
}

bool DynamicZoneGateOfAlter::checkPC() {
    Assert(m_pZone != NULL);

    // Check the number of PCs
    uint size = m_pZone->getPCManager()->getSize();

    return size != 0;
}

bool DynamicZoneGateOfAlter::checkNoEffect() {
    Assert(m_pZone != NULL);

    // Check the number of effects
    uint size = m_pZone->getEffectManager()->getSize();

    //		char msg[8];
    //		sprintf( msg, "%u", size );
    //		GCSystemMessage gcmsg;
    //		gcmsg.setMessage( msg );
    //		m_pZone->broadcastPacket( &gcmsg );

    return size == 0;
}

bool DynamicZoneGateOfAlter::openGateToAlter() {
    Assert(m_pZone != NULL);

    // Advance the quest.
    unordered_map<ObjectID_t, Creature*>::const_iterator itr = m_pZone->getPCManager()->getCreatures().begin();
    unordered_map<ObjectID_t, Creature*>::const_iterator endItr = m_pZone->getPCManager()->getCreatures().end();

    for (; itr != endItr; ++itr) {
        Assert(itr->second != NULL);

        if (itr->second->isPC()) {
            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(itr->second);
            Assert(pPC != NULL);

            pPC->getGQuestManager()->clearDynamicZone(m_TemplateZoneID);
        }
    }

    // Find the portal tile and broadcast the tile-portal effect on it.
    int tx = -1;
    int ty = -1;
    for (int x = 0; x < m_pZone->getWidth(); x++) {
        for (int y = 0; y < m_pZone->getHeight(); y++) {
            Tile& tile = m_pZone->getTile(x, y);

            if (tile.hasPortal()) {
                tx = x;
                ty = y;
            }
        }
    }

    if (tx != -1) {
        GCAddEffectToTile gcAddEffectToTile;
        gcAddEffectToTile.setEffectID(Effect::EFFECT_CLASS_TILE_PORTAL);
        gcAddEffectToTile.setDuration(static_cast<Duration_t>(999999));
        gcAddEffectToTile.setXY(tx, ty);

        m_pZone->broadcastPacket(&gcAddEffectToTile);
    }

    return true;
}

bool DynamicZoneGateOfAlter::removeTileEffect() {
    Assert(m_pZone != NULL);

    // Clear every effect.
    m_pZone->getEffectManager()->setTimeOutAllEffect();

    return true;
}

void DynamicZoneGateOfAlter::removeEffect(ZoneCoord_t x, ZoneCoord_t y) {
    Assert(m_pZone != NULL);

    VSRect rect(0, 0, m_pZone->getWidth() - 1, m_pZone->getHeight() - 1);
    for (int i = (int)x - 1; i <= (int)x + 1; i++) {
        for (int j = (int)y - 1; j <= (int)y + 1; j++) {
            if (!rect.ptInRect(i, j))
                continue;

            Tile& tile = m_pZone->getTile(i, j);

            Effect* pEffect = tile.getEffect(Effect::EFFECT_CLASS_DELETE_TILE);

            if (pEffect != NULL) {
                pEffect->setDeadline(0);
            }
        }
    }
}

void DynamicZoneGateOfAlter::addFakeEffect() {
    Assert(m_pZone != NULL);

    // Send a FakeEffect to every portal position.
    for (int x = 0; x < m_pZone->getWidth(); x++) {
        for (int y = 0; y < m_pZone->getHeight(); y++) {
            Tile& tile = m_pZone->getTile(x, y);

            if (tile.hasPortal()) {
                GCDeleteEffectFromTile gcDeleteEffectFromTile;
                gcDeleteEffectFromTile.setEffectID(Effect::EFFECT_CLASS_DELETE_TILE);
                gcDeleteEffectFromTile.setXY(x, y);
                m_pZone->broadcastPacket(&gcDeleteEffectFromTile);
            }
        }
    }
}

void DynamicZoneGateOfAlter::processEntering() {
    // Announce that the quest zone has been entered.
    unordered_map<ObjectID_t, Creature*>::const_iterator itr = m_pZone->getPCManager()->getCreatures().begin();
    unordered_map<ObjectID_t, Creature*>::const_iterator endItr = m_pZone->getPCManager()->getCreatures().end();

    for (; itr != endItr; ++itr) {
        Assert(itr->second != NULL);

        if (itr->second->isPC()) {
            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(itr->second);
            Assert(pPC != NULL);

            pPC->getGQuestManager()->enterDynamicZone(m_TemplateZoneID);
        }
    }
}


DEFINE_DYNAMIC_ZONE_FACTORY(DynamicZoneGateOfAlter, DYNAMIC_ZONE_GATE_OF_ALTER)
