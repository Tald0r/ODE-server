//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectInvisibility.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectInvisibility.h"

#include "Creature.h"
#include "DB.h"
#include "GCStatusCurrentHP.h"
#include "Slayer.h"
#include "Vampire.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectInvisibility::EffectInvisibility(Creature* pCreature)

{
    setTarget(pCreature);
}

void EffectInvisibility::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectInvisibility::affect(Creature* pCreature)

{
    if (!pCreature->isVampire())
        return;

    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

    HP_t currentHP = pVampire->getHP();

    if (currentHP == 1) {
        setDeadline(0);
        return;
    }

    HP_t decreaseHP = 3 + pVampire->getINT() / 250 + pVampire->getDEX() / 60 + pVampire->getSTR() / 200;
    int finalHP = currentHP - decreaseHP;

    if (finalHP < 1)
        finalHP = 1;
    pVampire->setHP(finalHP);

    GCStatusCurrentHP gcHP;
    gcHP.setObjectID(pVampire->getObjectID());
    gcHP.setCurrentHP(finalHP);

    pVampire->getZone()->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcHP);

    setNextTime(10);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectInvisibility::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectInvisibility::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    Zone* pZone = pCreature->getZone();
    Assert(pZone);

    // A true argument makes it delete the effect from the effect manager.
    addVisibleCreature(pZone, pCreature, false);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectInvisibility::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectInvisibility::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectInvisibility::toString() const {
    StringStream msg;

    msg << "EffectInvisibility(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
