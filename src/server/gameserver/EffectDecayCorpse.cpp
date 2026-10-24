//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectDecayCorpse.cpp
// Written by  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectDecayCorpse.h"

#include "Assert.h"
#include "Corpse.h"
#include "GCDeleteObject.h"
#include "MonsterCorpse.h"
#include "SlayerCorpse.h"
#include "Tile.h"
#include "VampireCorpse.h"
#include "Zone.h"
#include "ZoneUtil.h"

EffectDecayCorpse::EffectDecayCorpse(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Corpse* pCorpse, Turn_t delay)

    : Effect(pZone, x, y, pCorpse, delay) {
    Assert(getZone() != NULL);
    Assert(getTarget() != NULL);

    m_ObjectID = pCorpse->getObjectID();

    // Server-only effect.
    m_bBroadcastingEffect = false;
}

EffectDecayCorpse::~EffectDecayCorpse()

{
    try {
        unaffect(m_pZone, m_X, m_Y, m_pTarget);
    } catch (Throwable& t) {
        diagnosticFilelog("destructorError.log", "~EffectDecayCorpse: %s", t.toString().c_str());
    }
}

void EffectDecayCorpse::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    throw UnsupportedError();
}

void EffectDecayCorpse::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    // The coordinates must be valid.
    Assert(isValidZoneCoord(pZone, x, y));

    // Reach the corpse.
    if (pZone->getTile(x, y).hasItem()) {
        Item* pItem = pZone->getTile(x, y).getItem();

        if (pItem != NULL) {
            // Only an item on the tile with the same object id is the same corpse.
            if (pItem->getObjectID() == m_ObjectID) {
                Corpse* pCorpse = dynamic_cast<Corpse*>(pTarget);
                try {
                    // Delete the corpse from the zone.
                    Assert(pZone->getTile(x, y).getItem() == pCorpse);
                    pZone->deleteItem(pCorpse, x, y);
                } catch (NoSuchElementException& nsee) {
                    cerr << "EffectDecayCorpse::unaffect() : NoSuchElementException" << endl;
                    throw Error(nsee.toString());
                }

                GCDeleteObject gcDeleteObject;
                gcDeleteObject.setObjectID(pCorpse->getObjectID());
                pZone->broadcastPacket(x, y, &gcDeleteObject);

                // Delete the corpse object itself.
                SAFE_DELETE(pCorpse);
            }
        }
    }

    pTarget = NULL;
}

void EffectDecayCorpse::unaffect()

{}

void EffectDecayCorpse::unaffect(Creature* pCreature)

{}

string EffectDecayCorpse::toString() const

{
    StringStream msg;

    msg << "EffectDecayCorpse(" << "ZoneID:" << (int)m_pZone->getZoneID() << ",X:" << (int)getX()
        << ",Y:" << (int)getY();

    if (m_pTarget)
        msg << ",Target:" << m_pTarget->toString();
    else
        msg << ",Target:NULL";

    msg << ",Deadline:" << (int)m_Deadline.tv_sec << "." << (int)m_Deadline.tv_usec;

    msg << ")";

    return msg.toString();
}
