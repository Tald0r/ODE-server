//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectConcealment.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectConcealment.h"

#include "GCRemoveEffect.h"
#include "Monster.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectConcealment::EffectConcealment(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());

    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectConcealment::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());

    // Turns off the flag.
    pCreature->removeFlag(Effect::EFFECT_CLASS_CONCEALMENT);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    Slayer* pTargetSlayer = dynamic_cast<Slayer*>(pCreature);
    Assert(pTargetSlayer != NULL);

    SLAYER_RECORD prev;

    pTargetSlayer->getSlayerRecord(prev);
    pTargetSlayer->initAllStat();
    pTargetSlayer->sendRealWearingInfo();
    pTargetSlayer->sendModifyInfo(prev);

    // Tells clients to remove the effect.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_CONCEALMENT);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectConcealment::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectConcealment::toString() const {
    StringStream msg;
    msg << "EffectConcealment(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
