//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectMagnumSpear.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectMagnumSpear.h"

#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Player.h"
#include "SkillUtil.h"
#include "Slayer.h"

EffectMagnumSpear::EffectMagnumSpear(Creature* pCreature)

{
    setTarget(pCreature);
}

void EffectMagnumSpear::affect() {
    if (m_pTarget == NULL || m_pTarget->getObjectClass() != OBJECT_CLASS_CREATURE) {
        setDeadline(0);
        return;
    }
    affect(dynamic_cast<Creature*>(m_pTarget));
}

void EffectMagnumSpear::affect(Creature* pCreature)

{
    Assert(pCreature != NULL);
    if (pCreature->isDead()) {
        setDeadline(0);
        return;
    }

    Creature* pAttacker = pCreature->getZone()->getCreature(m_CasterOID);
    Damage_t damage = m_Damage / m_TotalTimes;
    if (m_Times == m_TotalTimes)
        damage += m_Damage % m_TotalTimes;

    GCModifyInformation gcMI, gcAttackerMI;

    ::setDamage(pCreature, damage, pAttacker, SKILL_MAGNUM_SPEAR, &gcMI, &gcAttackerMI);

    if (pCreature->isDead() && pAttacker != NULL && pAttacker->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pAttacker);
        Exp_t exp = computeCreatureExp(pCreature, KILL_EXP);
        shareOustersExp(pOusters, exp, gcAttackerMI);
        increaseAlignment(pAttacker, pCreature, gcAttackerMI);
    }

    if (pAttacker != NULL && pAttacker->isPC()) {
        computeAlignmentChange(pCreature, damage, pAttacker, &gcMI, &gcAttackerMI);
        pAttacker->getPlayer()->sendPacket(&gcAttackerMI);
    }

    if (pCreature->isPC()) {
        pCreature->getPlayer()->sendPacket(&gcMI);
    }

    --m_Times;

    if (m_Times == 0)
        setDeadline(0);
    else
        setNextTime(m_Tick);
}

void EffectMagnumSpear::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectMagnumSpear::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG


    Assert(pCreature != NULL);

    pCreature->removeFlag(Effect::EFFECT_CLASS_MAGNUM_SPEAR);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Tells clients that the effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_MAGNUM_SPEAR);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);


    __END_DEBUG
}

string EffectMagnumSpear::toString() const {
    StringStream msg;
    msg << "EffectMagnumSpear(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
