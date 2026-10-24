//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectDancingSword.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectDancingSword.h"

#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Player.h"
#include "Slayer.h"

EffectDancingSword::EffectDancingSword(Creature* pCreature)

{
    setTarget(pCreature);
}

void EffectDancingSword::affect(Creature* pCreature)

{}

void EffectDancingSword::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectDancingSword::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG


    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());

    pCreature->removeFlag(Effect::EFFECT_CLASS_DANCING_SWORD);

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

    SLAYER_RECORD prev;
    pSlayer->getSlayerRecord(prev);
    pSlayer->initAllStat();
    pSlayer->sendRealWearingInfo();
    pSlayer->sendModifyInfo(prev);

    Zone* pZone = pSlayer->getZone();
    Assert(pZone != NULL);

    // Tells clients that the effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pSlayer->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_DANCING_SWORD);
    pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcRemoveEffect);


    __END_DEBUG
}

string EffectDancingSword::toString() const {
    StringStream msg;
    msg << "EffectDancingSword(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
