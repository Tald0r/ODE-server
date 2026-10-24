//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectSeduction.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectSeduction.h"

#include "Creature.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "Ousters.h"
#include "Player.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectSeduction::EffectSeduction(Creature* pCreature)

{
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSeduction::affect(Creature* pCreature)

{}

void EffectSeduction::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSeduction::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG


    Assert(pCreature != NULL);

    // Restoring the stats requires clearing the flag and
    // calling initAllStat.
    pCreature->removeFlag(Effect::EFFECT_CLASS_SEDUCTION);

    if (pCreature->isSlayer()) {
        Slayer* pTargetSlayer = dynamic_cast<Slayer*>(pCreature);
        SLAYER_RECORD prev;

        pTargetSlayer->getSlayerRecord(prev);
        pTargetSlayer->initAllStat();
        pTargetSlayer->sendRealWearingInfo();
        pTargetSlayer->sendModifyInfo(prev);
    } else if (pCreature->isVampire()) {
        Vampire* pTargetVampire = dynamic_cast<Vampire*>(pCreature);
        VAMPIRE_RECORD prev;

        pTargetVampire->getVampireRecord(prev);
        pTargetVampire->initAllStat();
        pTargetVampire->sendRealWearingInfo();
        pTargetVampire->sendModifyInfo(prev);
    } else if (pCreature->isOusters()) {
        Ousters* pTargetOusters = dynamic_cast<Ousters*>(pCreature);
        OUSTERS_RECORD prev;

        pTargetOusters->getOustersRecord(prev);
        pTargetOusters->initAllStat();
        pTargetOusters->sendRealWearingInfo();
        pTargetOusters->sendModifyInfo(prev);
    } else if (pCreature->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pCreature);
        pMonster->initAllStat();
    } else
        Assert(false);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_SEDUCTION);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);


    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectSeduction::toString() const {
    StringStream msg;

    msg << "EffectSeduction(" << "ObjectID:" << getObjectID() << ",ToHit:" << m_ToHitPenalty
        << ",Damage:" << m_DamagePenalty << ")";

    return msg.toString();
}
