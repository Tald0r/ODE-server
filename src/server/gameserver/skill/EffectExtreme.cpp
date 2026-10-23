//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectExtreme.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectExtreme.h"

#include "Creature.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "Player.h"
#include "Vampire.h"

EffectExtreme::EffectExtreme(Creature* pCreature)

{
    setTarget(pCreature);
}

void EffectExtreme::affect(Creature* pCreature)

{}

void EffectExtreme::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectExtreme::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG


    Assert(pCreature != NULL);
    Assert(pCreature->isVampire());

    pCreature->removeFlag(Effect::EFFECT_CLASS_EXTREME);

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
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_EXTREME);
    pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcRemoveEffect);


    __END_DEBUG
}

string EffectExtreme::toString() const {
    StringStream msg;
    msg << "EffectExtreme(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
