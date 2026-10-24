//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectChargingPower.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectChargingPower.h"

#include "Creature.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "Player.h"
#include "Slayer.h"
#include "Vampire.h"

EffectChargingPower::EffectChargingPower(Creature* pCreature)

{
    setTarget(pCreature);
}

void EffectChargingPower::affect(Creature* pCreature)

{}

void EffectChargingPower::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectChargingPower::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG


    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());

    pCreature->removeFlag(Effect::EFFECT_CLASS_CHARGING_POWER);

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
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_CHARGING_POWER);
    pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcRemoveEffect);


    __END_DEBUG
}

string EffectChargingPower::toString() const {
    StringStream msg;
    msg << "EffectChargingPower(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
