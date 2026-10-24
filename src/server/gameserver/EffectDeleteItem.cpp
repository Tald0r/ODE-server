//----------------------------------------------------------------------
//
// Filename    : EffectDeleteItem.cpp
// Written by  : Reiot
//
//----------------------------------------------------------------------

// include fies
#include "EffectDeleteItem.h"

#include "Assert.h"
#include "GCDeleteObject.h"
#include "Item.h"
#include "Tile.h"
#include "Zone.h"
#include "ZoneUtil.h"

//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
EffectDeleteItem::EffectDeleteItem(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject, Turn_t delay)

    : Effect(pZone, x, y, pObject, delay) {
    Assert(getZone() != NULL);
    Assert(getTarget() != NULL);

    // m_ObjectID = pItem->getObjectID();
    m_ObjectID = pObject->getObjectID();

    // Server-only effect.
    m_bBroadcastingEffect = false;
}


//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
EffectDeleteItem::~EffectDeleteItem()

{
    try {
        unaffect(m_pZone, m_X, m_Y, m_pTarget);
    } catch (Throwable& t) {
        filelog("destructor.log", "~EffectDeleteItem: %s", t.toString().c_str());
    }
}


//----------------------------------------------------------------------
// affect to target
// This effect does not belong to a tile, so affect() is never called:
// the target is set in the constructor and it does nothing.
//----------------------------------------------------------------------
void EffectDeleteItem::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    //	throw UnsupportedError();
}


//----------------------------------------------------------------------
// remove effect from target
//----------------------------------------------------------------------
void EffectDeleteItem::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    // The coordinates must be valid.
    // Assert(pTarget!=NULL);
    Assert(isValidZoneCoord(pZone, x, y));

    // pZone->deleteFromItemList(pTarget->getObjectID());
    //(pZone->getTile( x, y)).deleteItem();
    // pZone->deleteItem( pTarget, x, y );

    Item* pItem = NULL;

    Tile& tile = pZone->getTile(x, y);

    if (tile.hasItem()) {
        pItem = tile.getItem();

        if (pItem != NULL) {
            if (pItem->getObjectID() == m_ObjectID) {
                pZone->deleteItem(pItem, x, y);

                GCDeleteObject gcDO;
                gcDO.setObjectID(pTarget->getObjectID());
                pZone->broadcastPacket(x, y, &gcDO);

                SAFE_DELETE(pTarget);
            }
        }
    }
}

//----------------------------------------------------------------------
// unaffect()
//----------------------------------------------------------------------
void EffectDeleteItem::unaffect()

{}

//----------------------------------------------------------------------
// unaffect()
//----------------------------------------------------------------------
void EffectDeleteItem::unaffect(Creature* pCreature)

{}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
string EffectDeleteItem::toString() const

{
    StringStream msg;

    msg << "EffectDeleteItem(" << "ZoneID:" << (int)m_pZone->getZoneID() << ",X:" << (int)getX()
        << ",Y:" << (int)getY();

    if (m_pTarget)
        msg << ",Target:" << m_pTarget->toString();
    else
        msg << ",Target:NULL";

    msg << ",Deadline:" << (int)m_Deadline.tv_sec << "." << (int)m_Deadline.tv_usec;

    msg << ")";

    return msg.toString();
}
