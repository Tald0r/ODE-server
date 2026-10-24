//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectTrapTriggered.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectTrapTriggered.h"

#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Player.h"
#include "Slayer.h"

EffectTrapTriggered::EffectTrapTriggered(Creature* pCreature)

{
    setTarget(pCreature);
}

void EffectTrapTriggered::affect(Creature* pCreature)

{}

void EffectTrapTriggered::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectTrapTriggered::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG


    Assert(pCreature != NULL);

    pCreature->removeFlag(Effect::EFFECT_CLASS_TRAPPED);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Tells clients that the effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_TRAPPED);
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_TRAP_TRIGGERED);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);


    __END_DEBUG
}

string EffectTrapTriggered::toString() const {
    StringStream msg;
    msg << "EffectTrapTriggered(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
