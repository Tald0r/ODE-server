//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectMephisto.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectMephisto.h"

#include "Creature.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "Player.h"
#include "Vampire.h"

EffectMephisto::EffectMephisto(Creature* pCreature)

{
    setTarget(pCreature);
}

void EffectMephisto::affect(Creature* pCreature)

{}

void EffectMephisto::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectMephisto::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG


    Assert(pCreature != NULL);
    Assert(pCreature->isVampire());

    pCreature->removeFlag(Effect::EFFECT_CLASS_MEPHISTO);

    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

    VAMPIRE_RECORD prev;
    pVampire->getVampireRecord(prev);
    pVampire->initAllStat();
    pVampire->sendRealWearingInfo();
    pVampire->sendModifyInfo(prev);

    Zone* pZone = pVampire->getZone();
    Assert(pZone != NULL);

    // Tells clients that the effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pVampire->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_MEPHISTO);
    pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcRemoveEffect);


    __END_DEBUG
}

string EffectMephisto::toString() const {
    StringStream msg;
    msg << "EffectMephisto(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
