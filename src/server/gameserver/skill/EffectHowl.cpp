//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectHowl.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectHowl.h"

#include "Creature.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "Player.h"
#include "Vampire.h"

EffectHowl::EffectHowl(Creature* pCreature)

{
    setTarget(pCreature);
}

void EffectHowl::affect(Creature* pCreature)

{}

void EffectHowl::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectHowl::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG


    Assert(pCreature != NULL);
    Assert(pCreature->isVampire());

    pCreature->removeFlag(Effect::EFFECT_CLASS_HOWL);

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
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_HOWL);
    pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcRemoveEffect);


    __END_DEBUG
}

string EffectHowl::toString() const {
    StringStream msg;
    msg << "EffectHowl(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
