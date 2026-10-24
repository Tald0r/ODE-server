//----------------------------------------------------------------------
//
// Filename    : EffectAddItem.cpp
// Written by  : Reiot
//
//----------------------------------------------------------------------

// include files
#include "EffectAddItem.h"

#include <stdio.h>

#include "Assert.h"
#include "GCDeleteObject.h"
#include "Item.h"
#include "ParkingCenter.h"
#include "Tile.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "item/Motorcycle.h"

//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
EffectAddItem::EffectAddItem(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Item* pItem, Turn_t delay, bool bAllowCreature)

    : Effect(pZone, x, y, pItem, delay) {
    // Server-only effect.
    m_bBroadcastingEffect = false;

    Assert(getZone() != NULL);
    Assert(getTarget() != NULL);

    // m_ObjectID = pItem->getObjectID();
    m_ObjectID = pItem->getObjectID();
    m_bAllowCreature = bAllowCreature;
}


//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
EffectAddItem::~EffectAddItem()

{
    try {
        unaffect(m_pZone, m_X, m_Y, m_pTarget);
    } catch (Throwable& t) {
        filelog("destructor.log", "~EffectAddItem: %s", t.toString().c_str());
    }
}


//----------------------------------------------------------------------
// affect to target
// This effect does not belong to a tile, so affect() is never called:
// the target is set in the constructor and it does nothing.
//----------------------------------------------------------------------
void EffectAddItem::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    //	throw UnsupportedError();
}


//----------------------------------------------------------------------
// remove effect from target
//----------------------------------------------------------------------
void EffectAddItem::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    // cout << "EffectAddItem::unaffect (Zone* pZone , ZoneCoord_t x , ZoneCoord_t y , Object* pTarget)" << endl;

    // The coordinates must be valid.
    Assert(pTarget != NULL);
    Assert(isValidZoneCoord(pZone, x, y));

    Item* pItem = dynamic_cast<Item*>(pTarget);

    if (pItem != NULL) {
        pZone->getObjectRegistry().registerObject(pItem);

        // TPOINT pt =
        pZone->addItem(pItem, x, y, m_bAllowCreature);
    } else {
        SAFE_DELETE(pTarget);
        throw Error("EffectAddItem target is not an item");
    }

    pTarget = NULL;
}

//----------------------------------------------------------------------
// unaffect()
//----------------------------------------------------------------------
void EffectAddItem::unaffect()

{}

//----------------------------------------------------------------------
// unaffect()
//----------------------------------------------------------------------
void EffectAddItem::unaffect(Creature* pCreature)

{}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
string EffectAddItem::toString() const

{
    StringStream msg;

    msg << "EffectAddItem(" << "ZoneID:" << (int)m_pZone->getZoneID() << ",X:" << (int)getX() << ",Y:" << (int)getY();

    if (m_pTarget)
        msg << ",Target:" << m_pTarget->toString();
    else
        msg << ",Target:NULL";

    msg << ",Deadline:" << (int)m_Deadline.tv_sec << "." << (int)m_Deadline.tv_usec;

    msg << ")";

    return msg.toString();
}
