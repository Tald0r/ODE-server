//----------------------------------------------------------------------
//
// Filename    : EffectTransportItem.cpp
// Written by  : Reiot
//
//----------------------------------------------------------------------

// include files
#include "EffectTransportItem.h"

#include "Assert.h"
#include "GCDeleteObject.h"
#include "Item.h"
#include "Tile.h"
#include "Zone.h"
#include "ZoneUtil.h"

//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
EffectTransportItem::EffectTransportItem(Zone* pZone, ZoneCoord_t sx, ZoneCoord_t sy, Zone* pTargetZone, ZoneCoord_t x,
                                         ZoneCoord_t y, Item* pItem, Turn_t delay)

    : Effect(pZone, x, y, pItem, delay) {
    Assert(getZone() != NULL);
    Assert(getTarget() != NULL);
    Assert(pTargetZone != NULL);

    // m_ObjectID = pItem->getObjectID();
    m_ObjectID = pItem->getObjectID();

    // pTargetZone, x, y
    // m_pZone, m_StartX, m_StartY
    m_pTargetZone = pTargetZone;
    m_StartX = sx;
    m_StartY = sy;

    // Server-only effect.
    m_bBroadcastingEffect = false;
}


//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
EffectTransportItem::~EffectTransportItem()

{
    try {
        unaffect(m_pZone, m_X, m_Y, m_pTarget);
    } catch (Throwable& t) {
        filelog("destructor.log", "~EffectTransportItem: %s", t.toString().c_str());
    }
}


//----------------------------------------------------------------------
// affect to target
// This effect does not belong to a tile, so affect() is never called,
// because the target is set in the constructor and it does nothing.
//----------------------------------------------------------------------
void EffectTransportItem::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    //	throw UnsupportedError();
}


//----------------------------------------------------------------------
// remove effect from target
//----------------------------------------------------------------------
void EffectTransportItem::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    // cout << "EffectTransportItem unaffect(pZone,x,y,pTarget)" << endl;

    // pTargetZone, x, y
    // m_pZone, m_StartX, m_StartY

    // The coordinates must be valid.
    if (isValidZoneCoord(pZone, m_StartX, m_StartY)) {
        // Assert(isValidZoneCoord(pZone, m_StartX, m_StartY));

        // Take a TempItem variable.
        Item* pTempItem = NULL;

        // The designated item may be gone here, or a different item may be lying there.
        // In that case compare the original item with the one now on the ground before deleting.
        // If there is none, ignore it.
        Tile& tile = pZone->getTile(m_StartX, m_StartY);

        if (tile.hasItem()) {
            pTempItem = tile.getItem();

            if (pTempItem != NULL) {
                // The same ObjectID means it is the same item.
                // if (pTempItem->getObjectID() == m_ObjectID) {
                if (pTempItem->getObjectID() == m_ObjectID) {
                    pZone->deleteItem(pTempItem, m_StartX, m_StartY);

                    // Send the packet saying the item is gone.
                    GCDeleteObject gcDeleteObject;
                    gcDeleteObject.setObjectID(m_ObjectID);

                    pZone->broadcastPacket(m_StartX, m_StartY, &gcDeleteObject);

                    // Add it to the other zone.
                    // This is multi-threaded, so be careful.
                    // If it is the same ZoneGroup, just put it in.
                    if (pZone->getZoneGroup() == m_pTargetZone->getZoneGroup())
                    // if (pZone==m_pTargetZone)
                    {
                        // cout << "Zone->addItem" << endl;
                        m_pTargetZone->addItem(pTempItem, x, y);
                    } else {
                        // cout << "Zone->addItemDelayed" << endl;
                        m_pTargetZone->addItemDelayed(pTempItem, x, y);
                    }

                    // cout << "addItemDelayed OK" << endl;
                }
            }
        }
    } else {
        filelog("motorBug.txt", "ZoneID=%d, x=%d, y=%d", (int)pZone->getZoneID(), (int)m_StartX, (int)m_StartY);
    }

    pTarget = NULL;
}

//----------------------------------------------------------------------
// unaffect()
//----------------------------------------------------------------------
void EffectTransportItem::unaffect()

{
    // cout << "EffectTransportItem unaffect" << endl;
}

//----------------------------------------------------------------------
// unaffect()
//----------------------------------------------------------------------
void EffectTransportItem::unaffect(Creature* pCreature)

{}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
string EffectTransportItem::toString() const

{
    StringStream msg;

    msg << "EffectTransportItem(" << "ZoneID:" << (int)m_pZone->getZoneID()
        << "TargetZoneID:" << (int)m_pTargetZone->getZoneID() << ",X:" << (int)getX() << ",Y:" << (int)getY();

    if (m_pTarget)
        msg << ",Target:" << m_pTarget->toString();
    else
        msg << ",Target:NULL";

    msg << ",Deadline:" << (int)m_Deadline.tv_sec << "." << (int)m_Deadline.tv_usec;

    msg << ")";

    return msg.toString();
}
