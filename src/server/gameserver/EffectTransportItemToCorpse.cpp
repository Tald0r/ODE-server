//----------------------------------------------------------------------
//
// Filename    : EffectTransportItemToCorpse.cpp
// Written by  : Reiot
//
//----------------------------------------------------------------------

// include files
#include "EffectTransportItemToCorpse.h"

#include "Assert.h"
#include "Corpse.h"
#include "GCDeleteObject.h"
#include "Item.h"
#include "Tile.h"
#include "Zone.h"
#include "ZoneUtil.h"

//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
EffectTransportItemToCorpse::EffectTransportItemToCorpse(Zone* pZone, Item* pItem, Zone* pTargetZone,
                                                         ObjectID_t corpseObjectID, Turn_t delay)

    : Effect(pZone, 0, 0, pItem, delay) {
    Assert(getZone() != NULL);
    Assert(getTarget() != NULL);
    Assert(pTargetZone != NULL);

    // pTargetZone, x, y
    // m_pZone, m_StartX, m_StartY
    m_pTargetZone = pTargetZone;
    m_CorpseObjectID = corpseObjectID;

    // Server-only effect.
    m_bBroadcastingEffect = false;
}


//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
EffectTransportItemToCorpse::~EffectTransportItemToCorpse()

{
    try {
        unaffect(m_pZone, m_X, m_Y, m_pTarget);
    } catch (Throwable& t) {
        filelog("destructor.log", "~EffectTransportItemToCorpse: %s", t.toString().c_str());
    }
}


//----------------------------------------------------------------------
// affect to target
// This effect does not belong to a tile, so affect() is never called:
// the target is set in the constructor and it does nothing.
//----------------------------------------------------------------------
void EffectTransportItemToCorpse::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    //	throw UnsupportedError();
}


//----------------------------------------------------------------------
// remove effect from target
//----------------------------------------------------------------------
void EffectTransportItemToCorpse::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    Assert(pZone != NULL);
    Assert(m_pTargetZone != NULL);

    Item* pItem = dynamic_cast<Item*>(pTarget);
    Assert(pItem != NULL);

    // Add to the other zone.
    // Multi-threaded, so be careful.
    // If it is the same ZoneGroup, just insert it.
    if (pZone->getZoneGroup() == m_pTargetZone->getZoneGroup())
    // if (pZone==m_pTargetZone)
    {
        // cout << "Zone->addItem" << endl;
        // pZone->addItem(pTempItem, x, y);

        Item* pCorpseItem = m_pTargetZone->getItem(m_CorpseObjectID);

        if (pCorpseItem != NULL && pCorpseItem->getItemClass() == Item::ITEM_CLASS_CORPSE) {
            Corpse* pCorpse = dynamic_cast<Corpse*>(pCorpseItem);
            Assert(pCorpse != NULL);

            pCorpse->addTreasure(pItem);
        } else {
            throw Error("Not a corpse");
        }
    } else {
        // cout << "Zone->addItemDelayed" << endl;
        m_pTargetZone->addItemToCorpseDelayed(pItem, m_CorpseObjectID);
    }

    pTarget = NULL;
}

//----------------------------------------------------------------------
// unaffect()
//----------------------------------------------------------------------
void EffectTransportItemToCorpse::unaffect()

{
    // cout << "EffectTransportItemToCorpse unaffect" << endl;
}

//----------------------------------------------------------------------
// unaffect()
//----------------------------------------------------------------------
void EffectTransportItemToCorpse::unaffect(Creature* pCreature)

{}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
string EffectTransportItemToCorpse::toString() const

{
    StringStream msg;

    msg << "EffectTransportItemToCorpse(" << "ZoneID:" << (int)m_pZone->getZoneID()
        << "TargetZoneID:" << (int)m_pTargetZone->getZoneID() << ",X:" << (int)getX() << ",Y:" << (int)getY();

    if (m_pTarget)
        msg << ",Target:" << m_pTarget->toString();
    else
        msg << ",Target:NULL";

    msg << ",Deadline:" << (int)m_Deadline.tv_sec << "." << (int)m_Deadline.tv_usec;

    msg << ")";

    return msg.toString();
}
