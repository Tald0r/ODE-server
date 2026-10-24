//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectCrossGuard.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectCrossGuard.h"

#include "GCRemoveEffect.h"
#include "Ousters.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectCrossGuard::EffectCrossGuard(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isOusters());

    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectCrossGuard::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isOusters());

    // Turns off the flag.
    pCreature->removeFlag(Effect::EFFECT_CLASS_CROSS_GUARD);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    Ousters* pTargetOusters = dynamic_cast<Ousters*>(pCreature);
    Assert(pTargetOusters != NULL);

    OUSTERS_RECORD prev;

    pTargetOusters->getOustersRecord(prev);
    pTargetOusters->initAllStat();
    pTargetOusters->sendRealWearingInfo();
    pTargetOusters->sendModifyInfo(prev);

    // Tells clients to remove the effect.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_CROSS_GUARD);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectCrossGuard::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectCrossGuard::toString() const {
    StringStream msg;
    msg << "EffectCrossGuard(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
