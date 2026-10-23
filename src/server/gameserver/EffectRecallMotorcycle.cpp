//----------------------------------------------------------------------
//
// Filename    : EffectRecallMotorcycle.cpp
// Written by  : Reiot
//
//----------------------------------------------------------------------

// include files
#include "EffectRecallMotorcycle.h"

#include "Assert.h"
#include "EffectRideMotorcycle.h"
#include "GCDeleteObject.h"
#include "Item.h"
#include "Slayer.h"
#include "Tile.h"
#include "Zone.h"
#include "ZoneUtil.h"

//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
EffectRecallMotorcycle::EffectRecallMotorcycle(Zone* pZone, ZoneCoord_t sx, ZoneCoord_t sy, Zone* pTargetZone,
                                               ZoneCoord_t x, ZoneCoord_t y, Item* pItem, ObjectID_t ownerOID,
                                               Turn_t delay)

    : Effect(pZone, x, y, pItem, delay), m_OwnerOID(ownerOID) {
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
EffectRecallMotorcycle::~EffectRecallMotorcycle()

{
    __BEGIN_TRY

    __END_CATCH_NO_RETHROW
}

//----------------------------------------------------------------------
// unaffect()
//----------------------------------------------------------------------
void EffectRecallMotorcycle::unaffect()

{
    // cout << "EffectRecallMotorcycle unaffect" << endl;

    Zone* pZone = m_pZone;
    ZoneCoord_t x = m_X;
    ZoneCoord_t y = m_Y;

    // The coordinates must be valid.
    if (isValidZoneCoord(pZone, m_StartX, m_StartY)) {
        // Assert(isValidZoneCoord(pZone, m_StartX, m_StartY));

        // Set up the TempItem variable.
        Item* pTempItem = NULL;

        // The item named here may be gone, or another item may be lying there instead.
        // In that case compare the original item with the one now on the ground before
        // deleting. If there is none, just ignore it.
        Tile& tile = pZone->getTile(m_StartX, m_StartY);

        if (tile.hasItem()) {
            pTempItem = tile.getItem();

            if (pTempItem != NULL) {
                // The same ObjectID means the same item.
                if (pTempItem->getObjectID() == m_ObjectID) {
                    pZone->deleteItem(pTempItem, m_StartX, m_StartY);

                    // Send the packet saying the item is gone.
                    GCDeleteObject gcDeleteObject;
                    gcDeleteObject.setObjectID(m_ObjectID);

                    pZone->broadcastPacket(m_StartX, m_StartY, &gcDeleteObject);

                    if (pZone->getZoneGroup() == m_pTargetZone->getZoneGroup()) {
                        m_pTargetZone->addItem(pTempItem, x, y);

                        Slayer* pOwner = dynamic_cast<Slayer*>((m_pTargetZone->getCreature(m_OwnerOID)));
                        if (pOwner != NULL) {
                            EffectRideMotorcycle* pEffect = new EffectRideMotorcycle(pOwner, pTempItem, x, y);
                            pEffect->setDeadline(0);
                            m_pTargetZone->registerObject(pEffect);
                            m_pTargetZone->addEffect(pEffect);
                        }
                    } else {
                        // cout << "Zone->addItemDelayed" << endl;
                        m_pTargetZone->addItemDelayed(pTempItem, x, y);

                        Slayer* pOwner = dynamic_cast<Slayer*>((m_pTargetZone->getCreature(m_OwnerOID)));
                        if (pOwner != NULL) {
                            EffectRideMotorcycle* pEffect = new EffectRideMotorcycle(pOwner, pTempItem, x, y);
                            pEffect->setDeadline(0);
                            m_pTargetZone->registerObject(pEffect);
                            m_pTargetZone->addEffect_LOCKING(pEffect);
                        }
                    }
                }
            }
        }
    } else {
        filelog("motorBug.txt", "ZoneID=%d, x=%d, y=%d", (int)pZone->getZoneID(), (int)m_StartX, (int)m_StartY);
    }
}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
string EffectRecallMotorcycle::toString() const

{
    StringStream msg;

    msg << "EffectRecallMotorcycle(" << "ZoneID:" << (int)m_pZone->getZoneID()
        << "TargetZoneID:" << (int)m_pTargetZone->getZoneID() << ",X:" << (int)getX() << ",Y:" << (int)getY();

    if (m_pTarget)
        msg << ",Target:" << m_pTarget->toString();
    else
        msg << ",Target:NULL";

    msg << ",Deadline:" << (int)m_Deadline.tv_sec << "." << (int)m_Deadline.tv_usec;

    msg << ")";

    return msg.toString();
}
