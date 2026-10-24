//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectDetectMine.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectDetectMine.h"

#include "Creature.h"
#include "DB.h"
#include "Slayer.h"

EffectDetectMine::EffectDetectMine(Creature* pCreature)

{
    setTarget(pCreature);
}

void EffectDetectMine::affect(Creature* pCreature)

{}

void EffectDetectMine::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

void EffectDetectMine::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    pCreature->removeFlag(Effect::EFFECT_CLASS_REVEALER);
    // Removes the mines that were visible through the effect.
    Zone* pZone = pCreature->getZone();
    pZone->updateMineScan(pCreature);
}

void EffectDetectMine::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectDetectMine::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

string EffectDetectMine::toString() const {
    StringStream msg;
    msg << "EffectDetectMine(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
