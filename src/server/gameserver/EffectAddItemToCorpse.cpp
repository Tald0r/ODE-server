//----------------------------------------------------------------------
//
// Filename    : EffectAddItemToCorpse.cpp
// Written by  : Reiot
//
//----------------------------------------------------------------------

// include files
#include "EffectAddItemToCorpse.h"

#include <stdio.h>

#include "Assert.h"
#include "Corpse.h"
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
EffectAddItemToCorpse::EffectAddItemToCorpse(Zone* pZone, Item* pItem, ObjectID_t corpseObjectID, Turn_t delay)

    : Effect(pZone, 0, 0, pItem, delay) {
    // Server-only effect.
    m_bBroadcastingEffect = false;

    Assert(getZone() != NULL);
    Assert(getTarget() != NULL);

    // m_ObjectID = pItem->getObjectID();
    m_CorpseObjectID = corpseObjectID;
}


//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
EffectAddItemToCorpse::~EffectAddItemToCorpse()

{
    unaffect(m_pZone, m_X, m_Y, m_pTarget);
}


//----------------------------------------------------------------------
// affect to target
// This effect does not belong to a tile, so affect() is never called:
// the target is set in the constructor and it does nothing.
//----------------------------------------------------------------------
void EffectAddItemToCorpse::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    //	throw UnsupportedError();
}


//----------------------------------------------------------------------
// remove effect from target
//----------------------------------------------------------------------
void EffectAddItemToCorpse::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    // The coordinates must be valid.
    Assert(pTarget != NULL);

    Item* pItem = dynamic_cast<Item*>(pTarget);
    Assert(pItem != NULL);

    pZone->getObjectRegistry().registerObject(pItem);

    Item* pCorpseItem = pZone->getItem(m_CorpseObjectID);

    if (pCorpseItem != NULL && pCorpseItem->getItemClass() == Item::ITEM_CLASS_CORPSE) {
        Corpse* pCorpse = dynamic_cast<Corpse*>(pCorpseItem);
        Assert(pCorpse != NULL);

        pCorpse->addTreasure(pItem);
    } else {
        throw Error("Not a corpse");
    }

    pTarget = NULL;
}

//----------------------------------------------------------------------
// unaffect()
//----------------------------------------------------------------------
void EffectAddItemToCorpse::unaffect()

{}

//----------------------------------------------------------------------
// unaffect()
//----------------------------------------------------------------------
void EffectAddItemToCorpse::unaffect(Creature* pCreature)

{}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
string EffectAddItemToCorpse::toString() const

{
    StringStream msg;

    msg << "EffectAddItemToCorpse(" << "ZoneID:" << (int)m_pZone->getZoneID() << ",X:" << (int)getX()
        << ",Y:" << (int)getY();

    if (m_pTarget)
        msg << ",Target:" << m_pTarget->toString();
    else
        msg << ",Target:NULL";

    msg << ",Deadline:" << (int)m_Deadline.tv_sec << "." << (int)m_Deadline.tv_usec;

    msg << ")";

    return msg.toString();
}
